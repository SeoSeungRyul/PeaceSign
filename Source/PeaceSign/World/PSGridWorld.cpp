// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSGridWorld.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "PSTileChunkActor.h"
#include "PSDirtAutoTileSet.h"
#include "PSWorldSaveGame.h"
#include "PSCropGrowth.h"
#include "../Time/PSGameTimeSubsystem.h"

namespace
{
	constexpr int32 CurrentWorldSaveVersion = 3;
	constexpr int32 LegacyStoneHealthScale = 10;

	uint32 HashCell(const FIntPoint Cell, const int32 Seed)
	{
		uint32 Hash = static_cast<uint32>(Cell.X) * 0x8da6b343u;
		Hash ^= static_cast<uint32>(Cell.Y) * 0xd8163841u;
		Hash ^= static_cast<uint32>(Seed) * 0xcb1ab31fu;
		Hash ^= Hash >> 16;
		Hash *= 0x7feb352du;
		Hash ^= Hash >> 15;
		Hash *= 0x846ca68bu;
		return Hash ^ (Hash >> 16);
	}
}

APSGridWorld::APSGridWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	const auto AddMineralRule = [this](const EPSMineralType MineralType, const int32 MinimumBand, const int32 Weight)
	{
		FPSMineralSpawnRule& Rule = MineralSpawnRules.AddDefaulted_GetRef();
		Rule.MineralType = MineralType;
		Rule.MinimumDistanceBand = MinimumBand;
		Rule.Weight = Weight;
	};
	AddMineralRule(EPSMineralType::Copper, 0, 30);
	AddMineralRule(EPSMineralType::Iron, 1, 20);
	AddMineralRule(EPSMineralType::Silver, 2, 14);
	AddMineralRule(EPSMineralType::Gold, 3, 10);
	AddMineralRule(EPSMineralType::Titanium, 4, 7);
	AddMineralRule(EPSMineralType::Lumistone, 5, 4);
	AddMineralRule(EPSMineralType::Asterium, 7, 2);

	ChunkActorClass = APSTileChunkActor::StaticClass();
}

void APSGridWorld::BeginPlay()
{
	Super::BeginPlay();

#if WITH_EDITOR
	if (IsValid(EditorPreviewActor))
	{
		EditorPreviewActor->SetActorHiddenInGame(true);
		EditorPreviewActor->SetActorEnableCollision(false);
	}
#endif

	StartingWorldSeed = WorldSeed;
	EnsureGrowthUpdates();
	LoadWorld();

	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FIntPoint PlayerChunk = PSGrid::CellToChunk(WorldToCell(PlayerPawn->GetActorLocation()), ChunkSize);
		UpdateActiveChunks(PlayerChunk);
		LastPlayerChunk = PlayerChunk;
	}
}

void APSGridWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GrowthClock) GrowthClock->OnClockChanged.RemoveDynamic(this, &ThisClass::HandleClockChanged);
	SaveWorld();
	Super::EndPlay(EndPlayReason);
}

void APSGridWorld::EnsureGrowthUpdates()
{
	if (!GrowthClock && GetWorld()) GrowthClock = GetWorld()->GetSubsystem<UPSGameTimeSubsystem>();
	if (GrowthClock) GrowthClock->OnClockChanged.AddUniqueDynamic(this, &ThisClass::HandleClockChanged);
}

int64 APSGridWorld::GetGrowthHalfHour() const
{
	const UPSGameTimeSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UPSGameTimeSubsystem>() : nullptr;
	return Clock ? Clock->GetHalfHourIndex() : 12;
}

void APSGridWorld::HandleClockChanged()
{
	if (GrowingCrops.IsEmpty())
	{
		SaveWorld();
		return;
	}
	const int64 Now = GetGrowthHalfHour();
	TSet<FIntPoint> ChangedChunks;
	for (auto It = GrowingCrops.CreateIterator(); It; ++It)
	{
		const uint8 Stage = PSCropGrowth::GetStage(It.Key(), Now);
		for (const FIntPoint Cell : It.Value())
		{
			const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
			const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
			FPSChunkSaveData* Saved = ModifiedChunks.Find(ChunkCoordinate);
			if (!Saved || !Saved->Cells.IsValidIndex(Index)) continue;
			FPSTileCell& Tile = Saved->Cells[Index];
			if (Tile.CropType == EPSCropType::None || Tile.PlantedHalfHour != It.Key() || Tile.GrowthStage == Stage) continue;
			Tile.GrowthStage = Stage;
			if (FPSChunkData* Loaded = LoadedChunks.Find(ChunkCoordinate))
			{
				if (Loaded->Cells.IsValidIndex(Index)) Loaded->Cells[Index] = Tile;
			}
			ChangedChunks.Add(ChunkCoordinate);
		}
		if (Stage == PSCropGrowth::MaxStage) It.RemoveCurrent();
	}
	for (const FIntPoint Chunk : ChangedChunks) RebuildChunk(Chunk);
	// Save once per clock event, including half-hour progress between visible stages.
	SaveWorld();
}

void APSGridWorld::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FIntPoint PlayerChunk = PSGrid::CellToChunk(WorldToCell(PlayerPawn->GetActorLocation()), ChunkSize);
	if (PlayerChunk != LastPlayerChunk)
	{
		UpdateActiveChunks(PlayerChunk);
		LastPlayerChunk = PlayerChunk;
	}
}

FIntPoint APSGridWorld::WorldToCell(const FVector& WorldPosition) const
{
	return PSGrid::WorldToCell(WorldPosition - GetActorLocation(), CellSize);
}

FVector APSGridWorld::CellToWorldCenter(const FIntPoint Cell) const
{
	return GetActorLocation() + PSGrid::CellToWorldCenter(Cell, CellSize, 2.0f);
}

EPSTileType APSGridWorld::GetGroundTile(const FIntPoint Cell) const
{
	if (!IsCellInsideWorld(Cell))
	{
		return EPSTileType::Empty;
	}

	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const FIntPoint LocalCell = PSGrid::CellToLocal(Cell, ChunkSize);
	const int32 CellIndex = PSGrid::LocalToIndex(LocalCell, ChunkSize);
	if (const FPSChunkData* LoadedChunk = LoadedChunks.Find(ChunkCoordinate))
	{
		if (LoadedChunk->Cells.IsValidIndex(CellIndex))
		{
			return LoadedChunk->Cells[CellIndex].GroundType;
		}
	}

	if (const FPSChunkSaveData* SavedChunk = ModifiedChunks.Find(ChunkCoordinate))
	{
		if (SavedChunk->Cells.IsValidIndex(CellIndex))
		{
			return SavedChunk->Cells[CellIndex].GroundType;
		}
	}

	return GenerateGroundTile(Cell);
}

int32 APSGridWorld::GetStoneHealth(const FIntPoint Cell) const
{
	if (GetWorldObjectType(Cell) != EPSWorldObjectType::Stone) return 0;
	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const int32 CellIndex = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
	if (const FPSChunkData* Chunk = LoadedChunks.Find(ChunkCoordinate))
	{
		if (Chunk->Cells.IsValidIndex(CellIndex))
			return FMath::Max(1, Chunk->Cells[CellIndex].ObjectHealth);
	}
	if (const FPSChunkSaveData* Chunk = ModifiedChunks.Find(ChunkCoordinate))
	{
		if (Chunk->Cells.IsValidIndex(CellIndex))
			return FMath::Max(1, Chunk->Cells[CellIndex].ObjectHealth);
	}
	return GetStoneMaxHealth(GenerateMineralType(Cell));
}

EPSWorldObjectType APSGridWorld::GetWorldObjectType(const FIntPoint Cell) const
{
	if (!IsCellInsideWorld(Cell)) return EPSWorldObjectType::None;
	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const int32 CellIndex = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
	if (const FPSChunkData* Chunk = LoadedChunks.Find(ChunkCoordinate))
		return Chunk->Cells.IsValidIndex(CellIndex) ? Chunk->Cells[CellIndex].ObjectType : EPSWorldObjectType::None;
	if (const FPSChunkSaveData* Chunk = ModifiedChunks.Find(ChunkCoordinate))
		return Chunk->Cells.IsValidIndex(CellIndex) ? Chunk->Cells[CellIndex].ObjectType : EPSWorldObjectType::None;
	return GenerateWorldObjectType(Cell);
}

EPSMineralType APSGridWorld::GetMineralType(const FIntPoint Cell) const
{
	if (GetWorldObjectType(Cell) != EPSWorldObjectType::Stone) return EPSMineralType::None;
	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const int32 CellIndex = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
	if (const FPSChunkData* Chunk = LoadedChunks.Find(ChunkCoordinate))
		return Chunk->Cells.IsValidIndex(CellIndex) ? Chunk->Cells[CellIndex].MineralType : EPSMineralType::None;
	if (const FPSChunkSaveData* Chunk = ModifiedChunks.Find(ChunkCoordinate))
		return Chunk->Cells.IsValidIndex(CellIndex) ? Chunk->Cells[CellIndex].MineralType : EPSMineralType::None;
	return GenerateMineralType(Cell);
}

int32 APSGridWorld::GetStoneMaxHealth(const EPSMineralType MineralType)
{
	switch (MineralType)
	{
	case EPSMineralType::Copper: return DefaultStoneHealth + 15;
	case EPSMineralType::Iron: return DefaultStoneHealth + 45;
	case EPSMineralType::Silver: return DefaultStoneHealth + 90;
	case EPSMineralType::Gold: return DefaultStoneHealth + 165;
	case EPSMineralType::Titanium: return DefaultStoneHealth + 285;
	case EPSMineralType::Lumistone: return DefaultStoneHealth + 480;
	case EPSMineralType::Asterium: return DefaultStoneHealth + 795;
	case EPSMineralType::None:
	default: return DefaultStoneHealth;
	}
}

bool APSGridWorld::CanMineFrom(const FIntPoint PlayerCell, const FIntPoint StoneCell) const
{
	if (!IsCellInsideWorld(PlayerCell) || !IsCellInsideWorld(StoneCell)) return false;
	const EPSTileType PlayerGround = GetGroundTile(PlayerCell);
	const int32 DX = FMath::Abs(PlayerCell.X - StoneCell.X);
	const int32 DY = FMath::Abs(PlayerCell.Y - StoneCell.Y);
	return PlayerGround != EPSTileType::Empty && PlayerGround != EPSTileType::Water
		&& DX + DY >= 1 && DX + DY <= 2
		&& GetWorldObjectType(StoneCell) == EPSWorldObjectType::Stone;
}

EPSTileInteractionResult APSGridWorld::MineCell(const FIntPoint Cell, const int32 Damage)
{
	if (!IsCellInsideWorld(Cell)) return EPSTileInteractionResult::InvalidCell;
	if (GetWorldObjectType(Cell) != EPSWorldObjectType::Stone || Damage <= 0)
		return EPSTileInteractionResult::NoEffect;

	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const int32 CellIndex = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
	FPSChunkData& Chunk = GetOrCreateChunk(ChunkCoordinate);
	FPSTileCell& Tile = Chunk.Cells[CellIndex];
	if (Tile.ObjectHealth <= 0) Tile.ObjectHealth = GetStoneMaxHealth(Tile.MineralType);
	Tile.ObjectHealth = FMath::Max(0, Tile.ObjectHealth - Damage);
	const bool bDestroyed = Tile.ObjectHealth == 0;
	if (bDestroyed)
	{
		Tile.ObjectType = EPSWorldObjectType::None;
		Tile.MineralType = EPSMineralType::None;
	}

	FPSChunkSaveData& SavedChunk = ModifiedChunks.FindOrAdd(ChunkCoordinate);
	SavedChunk.Coordinate = ChunkCoordinate;
	SavedChunk.Cells = Chunk.Cells;
	RebuildChunk(ChunkCoordinate);
	SaveWorld();
	return bDestroyed ? EPSTileInteractionResult::Mined : EPSTileInteractionResult::StoneDamaged;
}

EPSCropType APSGridWorld::GetCropType(const FIntPoint Cell) const
{
	if (!IsCellInsideWorld(Cell)) return EPSCropType::None;
	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const int32 CellIndex = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
	if (const FPSChunkData* Chunk = LoadedChunks.Find(ChunkCoordinate))
		return Chunk->Cells.IsValidIndex(CellIndex) ? Chunk->Cells[CellIndex].CropType : EPSCropType::None;
	if (const FPSChunkSaveData* Chunk = ModifiedChunks.Find(ChunkCoordinate))
		return Chunk->Cells.IsValidIndex(CellIndex) ? Chunk->Cells[CellIndex].CropType : EPSCropType::None;
	return EPSCropType::None;
}

int32 APSGridWorld::GetCropId(const FIntPoint Cell) const
{
	if (!IsCellInsideWorld(Cell)) return INDEX_NONE;
	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const int32 CellIndex = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
	if (const FPSChunkData* Chunk = LoadedChunks.Find(ChunkCoordinate))
		return Chunk->Cells.IsValidIndex(CellIndex) ? Chunk->Cells[CellIndex].CropId : INDEX_NONE;
	if (const FPSChunkSaveData* Chunk = ModifiedChunks.Find(ChunkCoordinate))
		return Chunk->Cells.IsValidIndex(CellIndex) ? Chunk->Cells[CellIndex].CropId : INDEX_NONE;
	return INDEX_NONE;
}

bool APSGridWorld::CanFishFrom(const FIntPoint PlayerCell, const FIntPoint WaterCell) const
{
	if (!IsCellInsideWorld(PlayerCell) || !IsCellInsideWorld(WaterCell)) return false;
	const EPSTileType Ground = GetGroundTile(PlayerCell);
	const int32 DX = FMath::Abs(PlayerCell.X - WaterCell.X);
	const int32 DY = FMath::Abs(PlayerCell.Y - WaterCell.Y);
	return Ground != EPSTileType::Empty && Ground != EPSTileType::Water
		&& DX + DY >= 1 && DX + DY <= 2
		&& GetGroundTile(WaterCell) == EPSTileType::Water;
}

int32 APSGridWorld::GetFishingLocationId(const FIntPoint WaterCell) const
{
	return GetGroundTile(WaterCell) == EPSTileType::Water ? static_cast<int32>(FishingLocation) : INDEX_NONE;
}

EPSTileInteractionResult APSGridWorld::TillCell(const FIntPoint Cell)
{
	if (GetWorldObjectType(Cell) != EPSWorldObjectType::None) return EPSTileInteractionResult::NoEffect;
	switch (GetGroundTile(Cell))
	{
	case EPSTileType::Grass:
	case EPSTileType::Dirt:
		return SetGroundTile(Cell, EPSTileType::TilledSoil)
			? EPSTileInteractionResult::Tilled
			: EPSTileInteractionResult::NoEffect;
	case EPSTileType::Water:
	case EPSTileType::TilledSoil:
		return EPSTileInteractionResult::NoEffect;
	case EPSTileType::Empty:
	default:
		return EPSTileInteractionResult::InvalidCell;
	}
}

int32 APSGridWorld::GetCropStage(const FIntPoint Cell) const
{
	if (!IsCellInsideWorld(Cell)) return 0;
	const FIntPoint Chunk = PSGrid::CellToChunk(Cell, ChunkSize);
	const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
	const TArray<FPSTileCell>* Cells = nullptr;
	if (const FPSChunkData* Loaded = LoadedChunks.Find(Chunk)) Cells = &Loaded->Cells;
	else if (const FPSChunkSaveData* Saved = ModifiedChunks.Find(Chunk)) Cells = &Saved->Cells;
	return Cells && Cells->IsValidIndex(Index) && (*Cells)[Index].CropType != EPSCropType::None
		? (*Cells)[Index].GrowthStage : 0;
}

EPSTileInteractionResult APSGridWorld::PlantSeed(const FIntPoint Cell, const int32 CropId)
{
	if (!IsCellInsideWorld(Cell)) return EPSTileInteractionResult::InvalidCell;
	if (CropId < 0) return EPSTileInteractionResult::NoEffect;
	if (GetGroundTile(Cell) != EPSTileType::TilledSoil
		|| GetWorldObjectType(Cell) != EPSWorldObjectType::None
		|| GetCropType(Cell) != EPSCropType::None)
		return EPSTileInteractionResult::NoEffect;
	return SetCropType(Cell, EPSCropType::TestCrop, CropId)
		? EPSTileInteractionResult::Planted
		: EPSTileInteractionResult::NoEffect;
}

EPSTileInteractionResult APSGridWorld::HarvestCrop(const FIntPoint Cell)
{
	if (!IsCellInsideWorld(Cell)) return EPSTileInteractionResult::InvalidCell;
	if (GetCropType(Cell) == EPSCropType::None || GetCropStage(Cell) < PSCropGrowth::MaxStage)
		return EPSTileInteractionResult::NoEffect;
	return SetCropType(Cell, EPSCropType::None)
		? EPSTileInteractionResult::Harvested
		: EPSTileInteractionResult::NoEffect;
}

EPSTileInteractionResult APSGridWorld::RemoveCrop(const FIntPoint Cell)
{
	if (!IsCellInsideWorld(Cell)) return EPSTileInteractionResult::InvalidCell;
	if (GetCropType(Cell) == EPSCropType::None) return EPSTileInteractionResult::NoEffect;
	return SetCropType(Cell, EPSCropType::None)
		? EPSTileInteractionResult::CropRemoved
		: EPSTileInteractionResult::NoEffect;
}

bool APSGridWorld::ResetWorld()
{
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0)
		&& !UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to delete grid world save slot '%s'"), *SaveSlotName);
		return false;
	}

	TArray<FIntPoint> VisibleChunks;
	ActiveChunkActors.GenerateKeyArray(VisibleChunks);
	LoadedChunks.Reset();
	ModifiedChunks.Reset();
	GrowingCrops.Reset();
	WorldSeed = StartingWorldSeed;

	// Rebuild existing renderers in place. Destroying and respawning them in the same
	// frame can leave the deferred old HISM visible after a development reset.
	for (const FIntPoint ChunkCoordinate : VisibleChunks)
	{
		APSTileChunkActor* ChunkActor = ActiveChunkActors.FindRef(ChunkCoordinate);
		if (!IsValid(ChunkActor))
		{
			ActiveChunkActors.Remove(ChunkCoordinate);
			continue;
		}
		const FPSChunkData RenderData = BuildRenderChunkData(ChunkCoordinate, GetOrCreateChunk(ChunkCoordinate));
		ChunkActor->Rebuild(RenderData, ChunkSize, CellSize, DirtAutoTileSet);
	}

	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FIntPoint PlayerChunk = PSGrid::CellToChunk(WorldToCell(PlayerPawn->GetActorLocation()), ChunkSize);
		UpdateActiveChunks(PlayerChunk);
		LastPlayerChunk = PlayerChunk;
	}
	else
	{
		LastPlayerChunk = FIntPoint(MAX_int32, MAX_int32);
	}

	UE_LOG(LogTemp, Log, TEXT("Grid world reset to seed %d"), WorldSeed);
	return true;
}

void APSGridWorld::GenerateEditorPreview()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || !ChunkActorClass)
	{
		return;
	}

	ClearEditorPreview();

	const int32 PreviewHalfExtent = FMath::Min(EditorPreviewHalfExtentInCells, WorldHalfExtentInCells);
	const int32 PreviewSize = PreviewHalfExtent * 2;
	FPSChunkData PreviewData;
	PreviewData.Cells.SetNum(PreviewSize * PreviewSize);

	for (int32 LocalY = 0; LocalY < PreviewSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < PreviewSize; ++LocalX)
		{
			const FIntPoint Cell(LocalX - PreviewHalfExtent, LocalY - PreviewHalfExtent);
			FPSTileCell& Tile = PreviewData.Cells[PSGrid::LocalToIndex(FIntPoint(LocalX, LocalY), PreviewSize)];
			Tile.GroundType = GenerateGroundTile(Cell);
			Tile.ObjectType = GenerateWorldObjectType(Cell);
			Tile.MineralType = Tile.ObjectType == EPSWorldObjectType::Stone
				? GenerateMineralType(Cell) : EPSMineralType::None;
			Tile.ObjectHealth = Tile.ObjectType == EPSWorldObjectType::Stone
				? GetStoneMaxHealth(Tile.MineralType) : 0;
		}
	}
	for (int32 LocalY = 0; LocalY < PreviewSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < PreviewSize; ++LocalX)
		{
			const FIntPoint Cell(LocalX - PreviewHalfExtent, LocalY - PreviewHalfExtent);
			FPSTileCell& Tile = PreviewData.Cells[PSGrid::LocalToIndex(FIntPoint(LocalX, LocalY), PreviewSize)];
			if (Tile.GroundType != EPSTileType::Grass && Tile.GroundType != EPSTileType::Dirt) continue;
			const uint16 Mask = BuildDirtNeighborMask(Cell);
			const uint32 VisualSeed = HashCell(Cell, WorldSeed + 104729);
			Tile.Variant = DirtAutoTileSet
				? DirtAutoTileSet->SelectVariant(Mask, VisualSeed)
				: PSDirtAutoTile::SelectDefaultVariant(Mask, VisualSeed);
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.OverrideLevel = GetLevel();
	SpawnParameters.ObjectFlags = RF_Transactional;
	SpawnParameters.InitialActorLabel = TEXT("Grid Preview");
	const FVector PreviewOrigin = GetActorLocation() + FVector(
		-static_cast<float>(PreviewHalfExtent) * CellSize,
		-static_cast<float>(PreviewHalfExtent) * CellSize,
		0.0f);
	APSTileChunkActor* PreviewActor = GetWorld()->SpawnActor<APSTileChunkActor>(
		ChunkActorClass,
		PreviewOrigin,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!PreviewActor)
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to create grid editor preview"));
		return;
	}

	Modify();
	PreviewActor->bIsEditorOnlyActor = true;
	PreviewActor->Tags.AddUnique(TEXT("PSGridEditorPreview"));
	PreviewActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	PreviewActor->Rebuild(PreviewData, PreviewSize, CellSize, DirtAutoTileSet);
	EditorPreviewActor = PreviewActor;
	GetLevel()->MarkPackageDirty();
#endif
}

void APSGridWorld::ClearEditorPreview()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld())
	{
		return;
	}

	if (!IsValid(EditorPreviewActor))
	{
		EditorPreviewActor = nullptr;
		return;
	}

	Modify();
	EditorPreviewActor->Modify();
	EditorPreviewActor->Destroy();
	EditorPreviewActor = nullptr;
	GetLevel()->MarkPackageDirty();
#endif
}

bool APSGridWorld::IsLakeCell(const FIntPoint Cell) const
{
	// One rectangular lake per selected 8x8 region, with a dry border between lakes.
	// Floor division keeps the same layout on both sides of the world origin.
	constexpr int32 RegionSize = 8;
	const FIntPoint Region = PSGrid::CellToChunk(Cell, RegionSize);
	const uint32 Hash = HashCell(Region, WorldSeed);
	if (Hash % 100 >= 35) return false;
	const FIntPoint Size(2 + (Hash >> 8) % 3, 2 + (Hash >> 12) % 3);
	const FIntPoint Min = Region * RegionSize + FIntPoint(2, 2);
	const FIntPoint Max = Min + Size - FIntPoint(1, 1);
	if (Cell.X < Min.X || Cell.Y < Min.Y || Cell.X > Max.X || Cell.Y > Max.Y) return false;
	// Reject entire lakes at world edges and near the starting area; never clip to one tile.
	if (!IsCellInsideWorld(Min) || !IsCellInsideWorld(Max)
		|| (Min.X <= 2 && Max.X >= -2 && Min.Y <= 2 && Max.Y >= -2)) return false;
	// Old saves contain whole chunks, including untouched land. Suppress a whole lake
	// if it overlaps saved land so loading an old chunk cannot cut a lake into fragments.
	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	{
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			const FIntPoint LakeCell(X, Y);
			if (const FPSChunkSaveData* Saved = ModifiedChunks.Find(PSGrid::CellToChunk(LakeCell, ChunkSize)))
			{
				const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(LakeCell, ChunkSize), ChunkSize);
				if (Saved->Cells.IsValidIndex(Index) && Saved->Cells[Index].GroundType != EPSTileType::Water) return false;
			}
		}
	}
	return true;
}

bool APSGridWorld::IsDirtPatchCell(const FIntPoint Cell) const
{
	// Dirt is generated as sparse ellipses instead of independent noisy cells. At the
	// current settings it covers roughly ten percent of non-water terrain.
	// Larger patches reduce the cross-shaped silhouette of tiny grid ellipses.
	// Doubling the region and radii preserves approximately the same patch coverage.
	constexpr int32 RegionSize = 16;
	constexpr uint32 PatchChancePercent = 32;
	const FIntPoint CellRegion = PSGrid::CellToChunk(Cell, RegionSize);
	for (int32 RegionY = -1; RegionY <= 1; ++RegionY)
	{
		for (int32 RegionX = -1; RegionX <= 1; ++RegionX)
		{
			const FIntPoint Region = CellRegion + FIntPoint(RegionX, RegionY);
			const uint32 Hash = HashCell(Region, WorldSeed + 48611);
			if (Hash % 100 >= PatchChancePercent) continue;

			const FIntPoint Center = Region * RegionSize + FIntPoint(
				4 + static_cast<int32>((Hash >> 8) % 8),
				4 + static_cast<int32>((Hash >> 12) % 8));
			const int32 RadiusX = 4 + static_cast<int32>((Hash >> 16) % 3);
			const int32 RadiusY = 4 + static_cast<int32>((Hash >> 20) % 3);
			// Sample cell centers against a patch centered on a grid vertex. This
			// avoids the single-cell tips produced by integer-centered circles.
			const int32 DX = 2 * (Cell.X - Center.X) + 1;
			const int32 DY = 2 * (Cell.Y - Center.Y) + 1;
			if (DX * DX * RadiusY * RadiusY + DY * DY * RadiusX * RadiusX
				<= 4 * RadiusX * RadiusX * RadiusY * RadiusY)
			{
				return true;
			}
		}
	}
	return false;
}

EPSTileType APSGridWorld::GenerateGroundTile(const FIntPoint Cell) const
{
	if (!IsCellInsideWorld(Cell))
	{
		return EPSTileType::Empty;
	}

	if (IsLakeCell(Cell)) return EPSTileType::Water;
	return IsDirtPatchCell(Cell) ? EPSTileType::Dirt : EPSTileType::Grass;
}

EPSWorldObjectType APSGridWorld::GenerateWorldObjectType(const FIntPoint Cell) const
{
	const EPSTileType Ground = GenerateGroundTile(Cell);
	if (Ground == EPSTileType::Empty || Ground == EPSTileType::Water)
		return EPSWorldObjectType::None;
	return HashCell(Cell, WorldSeed) % 100 < 5
		? EPSWorldObjectType::Stone : EPSWorldObjectType::None;
}

EPSMineralType APSGridWorld::GenerateMineralType(const FIntPoint Cell) const
{
	if (GenerateWorldObjectType(Cell) != EPSWorldObjectType::Stone) return EPSMineralType::None;
	const int32 Distance = FMath::Max(FMath::Abs(Cell.X), FMath::Abs(Cell.Y));
	const int32 DistanceBand = FMath::Clamp(
		Distance * 8 / FMath::Max(1, WorldHalfExtentInCells), 0, 7);
	const int32 PlainWeight = FMath::Max(1, PlainStoneSpawnWeight);
	int32 TotalWeight = PlainWeight;
	for (const FPSMineralSpawnRule& Rule : MineralSpawnRules)
		if (Rule.MineralType != EPSMineralType::None && DistanceBand >= Rule.MinimumDistanceBand)
			TotalWeight += FMath::Max(0, Rule.Weight);

	int32 Roll = static_cast<int32>(HashCell(Cell, WorldSeed + 7919) % static_cast<uint32>(TotalWeight));
	if (Roll < PlainWeight) return EPSMineralType::None;
	Roll -= PlainWeight;
	for (const FPSMineralSpawnRule& Rule : MineralSpawnRules)
	{
		if (Rule.MineralType == EPSMineralType::None || DistanceBand < Rule.MinimumDistanceBand) continue;
		const int32 Weight = FMath::Max(0, Rule.Weight);
		if (Roll < Weight) return Rule.MineralType;
		Roll -= Weight;
	}
	return EPSMineralType::None;
}

FPSChunkData APSGridWorld::GenerateChunk(const FIntPoint ChunkCoordinate) const
{
	FPSChunkData Chunk;
	Chunk.Coordinate = ChunkCoordinate;
	Chunk.Cells.SetNum(ChunkSize * ChunkSize);

	for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
		{
			const FIntPoint Cell(
				ChunkCoordinate.X * ChunkSize + LocalX,
				ChunkCoordinate.Y * ChunkSize + LocalY);
			FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(FIntPoint(LocalX, LocalY), ChunkSize)];
			Tile.GroundType = GenerateGroundTile(Cell);
			Tile.ObjectType = GenerateWorldObjectType(Cell);
			Tile.MineralType = Tile.ObjectType == EPSWorldObjectType::Stone
				? GenerateMineralType(Cell) : EPSMineralType::None;
			Tile.ObjectHealth = Tile.ObjectType == EPSWorldObjectType::Stone
				? GetStoneMaxHealth(Tile.MineralType) : 0;
		}
	}

	return Chunk;
}

uint16 APSGridWorld::BuildDirtNeighborMask(const FIntPoint Cell) const
{
	const auto IsDirt = [this](const FIntPoint Candidate)
	{
		return GetGroundTile(Candidate) == EPSTileType::Dirt;
	};

	uint16 Mask = IsDirt(Cell) ? PSDirtAutoTile::Center : 0;
	// Plane UVs increase U along +X and V along +Y. Image top (V=0)
	// therefore faces -Y, regardless of the camera's screen orientation.
	if (IsDirt(Cell + FIntPoint(0, -1))) Mask |= PSDirtAutoTile::North;
	if (IsDirt(Cell + FIntPoint(1, 0))) Mask |= PSDirtAutoTile::East;
	if (IsDirt(Cell + FIntPoint(0, 1))) Mask |= PSDirtAutoTile::South;
	if (IsDirt(Cell + FIntPoint(-1, 0))) Mask |= PSDirtAutoTile::West;
	if (IsDirt(Cell + FIntPoint(-1, -1))) Mask |= PSDirtAutoTile::NorthWest;
	if (IsDirt(Cell + FIntPoint(1, -1))) Mask |= PSDirtAutoTile::NorthEast;
	if (IsDirt(Cell + FIntPoint(1, 1))) Mask |= PSDirtAutoTile::SouthEast;
	if (IsDirt(Cell + FIntPoint(-1, 1))) Mask |= PSDirtAutoTile::SouthWest;
	return Mask;
}

FPSChunkData APSGridWorld::BuildRenderChunkData(
	const FIntPoint ChunkCoordinate,
	const FPSChunkData& Source) const
{
	FPSChunkData Result = Source;
	for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
		{
			FPSTileCell& Tile = Result.Cells[PSGrid::LocalToIndex(FIntPoint(LocalX, LocalY), ChunkSize)];
			Tile.Variant = PSDirtAutoTile::NoVariant;
			if (Tile.GroundType != EPSTileType::Grass && Tile.GroundType != EPSTileType::Dirt) continue;
			const FIntPoint Cell(
				ChunkCoordinate.X * ChunkSize + LocalX,
				ChunkCoordinate.Y * ChunkSize + LocalY);
			const uint16 Mask = BuildDirtNeighborMask(Cell);
			const uint32 VisualSeed = HashCell(Cell, WorldSeed + 104729);
			Tile.Variant = DirtAutoTileSet
				? DirtAutoTileSet->SelectVariant(Mask, VisualSeed)
				: PSDirtAutoTile::SelectDefaultVariant(Mask, VisualSeed);
		}
	}
	return Result;
}

FPSChunkData& APSGridWorld::GetOrCreateChunk(const FIntPoint ChunkCoordinate)
{
	if (FPSChunkData* ExistingChunk = LoadedChunks.Find(ChunkCoordinate))
	{
		return *ExistingChunk;
	}

	FPSChunkData Chunk = GenerateChunk(ChunkCoordinate);
	if (const FPSChunkSaveData* SavedChunk = ModifiedChunks.Find(ChunkCoordinate))
	{
		if (SavedChunk->Cells.Num() == ChunkSize * ChunkSize)
		{
			Chunk.Cells = SavedChunk->Cells;
		}
	}

	return LoadedChunks.Add(ChunkCoordinate, MoveTemp(Chunk));
}

void APSGridWorld::UpdateActiveChunks(const FIntPoint PlayerChunk)
{
	TSet<FIntPoint> RequiredChunks;
	for (int32 OffsetY = -LoadRadius; OffsetY <= LoadRadius; ++OffsetY)
	{
		for (int32 OffsetX = -LoadRadius; OffsetX <= LoadRadius; ++OffsetX)
		{
			const FIntPoint ChunkCoordinate = PlayerChunk + FIntPoint(OffsetX, OffsetY);
			RequiredChunks.Add(ChunkCoordinate);
			if (!ActiveChunkActors.Contains(ChunkCoordinate))
			{
				SpawnChunkRenderer(ChunkCoordinate);
			}
		}
	}

	TArray<FIntPoint> ChunksToRemove;
	for (const TPair<FIntPoint, TObjectPtr<APSTileChunkActor>>& Pair : ActiveChunkActors)
	{
		if (!RequiredChunks.Contains(Pair.Key))
		{
			ChunksToRemove.Add(Pair.Key);
		}
	}

	for (const FIntPoint ChunkCoordinate : ChunksToRemove)
	{
		if (APSTileChunkActor* ChunkActor = ActiveChunkActors.FindRef(ChunkCoordinate))
		{
			ChunkActor->Destroy();
		}
		ActiveChunkActors.Remove(ChunkCoordinate);
		LoadedChunks.Remove(ChunkCoordinate);
	}
}

void APSGridWorld::SpawnChunkRenderer(const FIntPoint ChunkCoordinate)
{
	if (!ChunkActorClass)
	{
		return;
	}

	const float ChunkWorldSize = static_cast<float>(ChunkSize) * CellSize;
	const FVector ChunkLocation = GetActorLocation() + FVector(
		static_cast<float>(ChunkCoordinate.X) * ChunkWorldSize,
		static_cast<float>(ChunkCoordinate.Y) * ChunkWorldSize,
		0.0f);
	APSTileChunkActor* ChunkActor = GetWorld()->SpawnActor<APSTileChunkActor>(
		ChunkActorClass,
		ChunkLocation,
		FRotator::ZeroRotator);
	if (!ChunkActor)
	{
		return;
	}

	ActiveChunkActors.Add(ChunkCoordinate, ChunkActor);
	const FPSChunkData RenderData = BuildRenderChunkData(ChunkCoordinate, GetOrCreateChunk(ChunkCoordinate));
	ChunkActor->Rebuild(RenderData, ChunkSize, CellSize, DirtAutoTileSet);
}

void APSGridWorld::RebuildChunk(const FIntPoint ChunkCoordinate)
{
	if (APSTileChunkActor* ChunkActor = ActiveChunkActors.FindRef(ChunkCoordinate))
	{
		const FPSChunkData RenderData = BuildRenderChunkData(ChunkCoordinate, GetOrCreateChunk(ChunkCoordinate));
		ChunkActor->Rebuild(RenderData, ChunkSize, CellSize, DirtAutoTileSet);
	}
}

void APSGridWorld::RebuildChunksAroundCell(const FIntPoint Cell)
{
	TSet<FIntPoint> Chunks;
	for (int32 Y = -1; Y <= 1; ++Y)
		for (int32 X = -1; X <= 1; ++X)
			Chunks.Add(PSGrid::CellToChunk(Cell + FIntPoint(X, Y), ChunkSize));
	for (const FIntPoint Chunk : Chunks) RebuildChunk(Chunk);
}

bool APSGridWorld::SetGroundTile(const FIntPoint Cell, const EPSTileType GroundType)
{
	if (!IsCellInsideWorld(Cell) || GroundType == EPSTileType::Empty || GetGroundTile(Cell) == GroundType)
	{
		return false;
	}

	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const FIntPoint LocalCell = PSGrid::CellToLocal(Cell, ChunkSize);
	FPSChunkData& Chunk = GetOrCreateChunk(ChunkCoordinate);
	FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(LocalCell, ChunkSize)];
	Tile.GroundType = GroundType;
	if (GroundType == EPSTileType::Water)
	{
		Tile.ObjectType = EPSWorldObjectType::None;
		Tile.ObjectHealth = 0;
		Tile.MineralType = EPSMineralType::None;
	}

	FPSChunkSaveData& SavedChunk = ModifiedChunks.FindOrAdd(ChunkCoordinate);
	SavedChunk.Coordinate = ChunkCoordinate;
	SavedChunk.Cells = Chunk.Cells;

	RebuildChunksAroundCell(Cell);
	SaveWorld();
	return true;
}

bool APSGridWorld::SetCropType(const FIntPoint Cell, const EPSCropType CropType, const int32 CropId)
{
	if (!IsCellInsideWorld(Cell) || GetCropType(Cell) == CropType) return false;
	EnsureGrowthUpdates();
	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const FIntPoint LocalCell = PSGrid::CellToLocal(Cell, ChunkSize);
	FPSChunkData& Chunk = GetOrCreateChunk(ChunkCoordinate);
	FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(LocalCell, ChunkSize)];
	if (Tile.PlantedHalfHour >= 0)
	{
		if (TArray<FIntPoint>* Bucket = GrowingCrops.Find(Tile.PlantedHalfHour))
		{
			Bucket->RemoveSingleSwap(Cell);
			if (Bucket->IsEmpty()) GrowingCrops.Remove(Tile.PlantedHalfHour);
		}
	}
	Tile.CropType = CropType;
	Tile.CropId = CropType == EPSCropType::None ? INDEX_NONE : FMath::Max(0, CropId);
	Tile.GrowthStage = 1;
	Tile.PlantedHalfHour = CropType == EPSCropType::None ? -1 : GetGrowthHalfHour();
	if (CropType != EPSCropType::None) GrowingCrops.FindOrAdd(Tile.PlantedHalfHour).AddUnique(Cell);
	FPSChunkSaveData& SavedChunk = ModifiedChunks.FindOrAdd(ChunkCoordinate);
	SavedChunk.Coordinate = ChunkCoordinate;
	SavedChunk.Cells = Chunk.Cells;
	RebuildChunk(ChunkCoordinate);
	SaveWorld();
	return true;
}

bool APSGridWorld::IsCellInsideWorld(const FIntPoint Cell) const
{
	return Cell.X >= -WorldHalfExtentInCells && Cell.X < WorldHalfExtentInCells
		&& Cell.Y >= -WorldHalfExtentInCells && Cell.Y < WorldHalfExtentInCells;
}

void APSGridWorld::LoadWorld()
{
	EnsureGrowthUpdates();
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		return;
	}

	const UPSWorldSaveGame* SaveGame = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!SaveGame)
	{
		return;
	}

	WorldSeed = SaveGame->WorldSeed;
	if (SaveGame->SavedClockHalfHour >= 0 && GrowthClock)
	{
		GrowthClock->OnClockChanged.RemoveDynamic(this, &ThisClass::HandleClockChanged);
		GrowthClock->RestoreClock(SaveGame->SavedClockHalfHour, SaveGame->SavedClockSecondsIntoStep);
	}
	const int64 Now = GetGrowthHalfHour();
	GrowingCrops.Reset();
	LoadedChunks.Reset();
	ModifiedChunks.Reset();
	for (const FPSChunkSaveData& Chunk : SaveGame->ModifiedChunks)
	{
		if (Chunk.Cells.Num() != ChunkSize * ChunkSize) continue;
		FPSChunkSaveData Restored = Chunk;
		for (int32 Index = 0; Index < Restored.Cells.Num(); ++Index)
		{
			FPSTileCell& Tile = Restored.Cells[Index];
			const FIntPoint Cell(Chunk.Coordinate.X * ChunkSize + Index % ChunkSize,
				Chunk.Coordinate.Y * ChunkSize + Index / ChunkSize);
			if (Tile.GroundType == EPSTileType::Stone && SaveGame->DataVersion < 1)
			{
				Tile.StoneHealth = Tile.StoneHealth > 0
					? FMath::Clamp(Tile.StoneHealth * LegacyStoneHealthScale, LegacyStoneHealthScale, DefaultStoneHealth)
					: DefaultStoneHealth;
			}
			if (Tile.GroundType == EPSTileType::Stone && SaveGame->DataVersion < 2)
			{
				const int32 PreviousDamage = FMath::Max(0, DefaultStoneHealth - Tile.StoneHealth);
				Tile.ObjectType = EPSWorldObjectType::Stone;
				Tile.MineralType = GenerateMineralType(Cell);
				Tile.StoneHealth = FMath::Max(1, GetStoneMaxHealth(Tile.MineralType) - PreviousDamage);
			}
			if (SaveGame->DataVersion < 3)
			{
				if (Tile.GroundType == EPSTileType::Stone)
				{
					Tile.GroundType = GenerateGroundTile(Cell);
					Tile.ObjectType = EPSWorldObjectType::Stone;
					Tile.ObjectHealth = Tile.StoneHealth > 0
						? Tile.StoneHealth : GetStoneMaxHealth(Tile.MineralType);
				}
				else
				{
					Tile.ObjectType = EPSWorldObjectType::None;
					Tile.ObjectHealth = 0;
					Tile.MineralType = EPSMineralType::None;
				}
				Tile.StoneHealth = 0;
			}
			else if (Tile.ObjectType == EPSWorldObjectType::Stone)
			{
				if (Tile.ObjectHealth <= 0) Tile.ObjectHealth = GetStoneMaxHealth(Tile.MineralType);
			}
			else
			{
				Tile.ObjectHealth = 0;
				Tile.MineralType = EPSMineralType::None;
			}
			if (Tile.CropType == EPSCropType::None) continue;
			Tile.CropId = FMath::Max(0, Tile.CropId);
			const int64 SavedStageAge = (FMath::Clamp<int32>(Tile.GrowthStage, 1, PSCropGrowth::MaxStage) - 1)
				* PSCropGrowth::HalfHoursPerStage;
			const int64 TimestampAge = SaveGame->GrowthClockHalfHour >= 0 && Tile.PlantedHalfHour >= 0
				? FMath::Clamp<int64>(SaveGame->GrowthClockHalfHour - Tile.PlantedHalfHour, 0, PSCropGrowth::MaxElapsedHalfHours) : 0;
			// Preserve stages saved under an older growth interval instead of moving crops backwards.
			const int64 Age = FMath::Max(SavedStageAge, TimestampAge);
			Tile.PlantedHalfHour = Now - Age;
			Tile.GrowthStage = PSCropGrowth::GetStage(Tile.PlantedHalfHour, Now);
			if (Tile.GrowthStage < PSCropGrowth::MaxStage)
			{
				GrowingCrops.FindOrAdd(Tile.PlantedHalfHour).Add(Cell);
			}
		}
		ModifiedChunks.Add(Chunk.Coordinate, MoveTemp(Restored));
	}
	EnsureGrowthUpdates();
}

void APSGridWorld::SaveWorld() const
{
	UPSWorldSaveGame* SaveGame = Cast<UPSWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UPSWorldSaveGame::StaticClass()));
	if (!SaveGame)
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to create grid world save object"));
		return;
	}

	SaveGame->WorldSeed = WorldSeed;
	SaveGame->DataVersion = CurrentWorldSaveVersion;
	SaveGame->GrowthClockHalfHour = GetGrowthHalfHour();
	if (const UPSGameTimeSubsystem* Clock = GetWorld() ? GetWorld()->GetSubsystem<UPSGameTimeSubsystem>() : nullptr)
	{
		SaveGame->SavedClockHalfHour = Clock->GetHalfHourIndex();
		SaveGame->SavedClockSecondsIntoStep = Clock->GetSecondsIntoStep();
	}
	ModifiedChunks.GenerateValueArray(SaveGame->ModifiedChunks);
	if (!UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to save grid world to slot '%s'"), *SaveSlotName);
	}
}
