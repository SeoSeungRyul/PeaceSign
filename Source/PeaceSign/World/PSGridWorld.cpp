// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSGridWorld.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "PSTileChunkActor.h"
#include "PSWorldSaveGame.h"

namespace
{
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
	LoadWorld();

	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FIntPoint PlayerChunk = PSGrid::CellToChunk(WorldToCell(PlayerPawn->GetActorLocation()), ChunkSize);
		UpdateActiveChunks(PlayerChunk);
		LastPlayerChunk = PlayerChunk;
	}
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

EPSTileInteractionResult APSGridWorld::TillCell(const FIntPoint Cell)
{
	switch (GetGroundTile(Cell))
	{
	case EPSTileType::Grass:
	case EPSTileType::Dirt:
		return SetGroundTile(Cell, EPSTileType::TilledSoil)
			? EPSTileInteractionResult::Tilled
			: EPSTileInteractionResult::NoEffect;
	case EPSTileType::Stone:
	case EPSTileType::TilledSoil:
		return EPSTileInteractionResult::NoEffect;
	case EPSTileType::Empty:
	default:
		return EPSTileInteractionResult::InvalidCell;
	}
}

EPSTileInteractionResult APSGridWorld::PlantSeed(const FIntPoint Cell)
{
	if (!IsCellInsideWorld(Cell)) return EPSTileInteractionResult::InvalidCell;
	if (GetGroundTile(Cell) != EPSTileType::TilledSoil || GetCropType(Cell) != EPSCropType::None)
		return EPSTileInteractionResult::NoEffect;
	return SetCropType(Cell, EPSCropType::TestCrop)
		? EPSTileInteractionResult::Planted
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

	for (const TPair<FIntPoint, TObjectPtr<APSTileChunkActor>>& Pair : ActiveChunkActors)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}

	ActiveChunkActors.Reset();
	LoadedChunks.Reset();
	ModifiedChunks.Reset();
	WorldSeed = StartingWorldSeed;
	LastPlayerChunk = FIntPoint(MAX_int32, MAX_int32);

	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FIntPoint PlayerChunk = PSGrid::CellToChunk(WorldToCell(PlayerPawn->GetActorLocation()), ChunkSize);
		UpdateActiveChunks(PlayerChunk);
		LastPlayerChunk = PlayerChunk;
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
			PreviewData.Cells[PSGrid::LocalToIndex(FIntPoint(LocalX, LocalY), PreviewSize)].GroundType =
				GenerateGroundTile(Cell);
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
	PreviewActor->Rebuild(PreviewData, PreviewSize, CellSize);
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

EPSTileType APSGridWorld::GenerateGroundTile(const FIntPoint Cell) const
{
	if (!IsCellInsideWorld(Cell))
	{
		return EPSTileType::Empty;
	}

	const uint32 Roll = HashCell(Cell, WorldSeed) % 100;
	return Roll < 5 ? EPSTileType::Stone : Roll < 15 ? EPSTileType::Dirt : EPSTileType::Grass;
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
		}
	}

	return Chunk;
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
	ChunkActor->Rebuild(GetOrCreateChunk(ChunkCoordinate), ChunkSize, CellSize);
}

void APSGridWorld::RebuildChunk(const FIntPoint ChunkCoordinate)
{
	if (APSTileChunkActor* ChunkActor = ActiveChunkActors.FindRef(ChunkCoordinate))
	{
		ChunkActor->Rebuild(GetOrCreateChunk(ChunkCoordinate), ChunkSize, CellSize);
	}
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
	Chunk.Cells[PSGrid::LocalToIndex(LocalCell, ChunkSize)].GroundType = GroundType;

	FPSChunkSaveData& SavedChunk = ModifiedChunks.FindOrAdd(ChunkCoordinate);
	SavedChunk.Coordinate = ChunkCoordinate;
	SavedChunk.Cells = Chunk.Cells;

	RebuildChunk(ChunkCoordinate);
	SaveWorld();
	return true;
}

bool APSGridWorld::SetCropType(const FIntPoint Cell, const EPSCropType CropType)
{
	if (!IsCellInsideWorld(Cell) || GetCropType(Cell) == CropType) return false;
	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const FIntPoint LocalCell = PSGrid::CellToLocal(Cell, ChunkSize);
	FPSChunkData& Chunk = GetOrCreateChunk(ChunkCoordinate);
	Chunk.Cells[PSGrid::LocalToIndex(LocalCell, ChunkSize)].CropType = CropType;
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
	for (const FPSChunkSaveData& Chunk : SaveGame->ModifiedChunks)
	{
		ModifiedChunks.Add(Chunk.Coordinate, Chunk);
	}
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
	ModifiedChunks.GenerateValueArray(SaveGame->ModifiedChunks);
	if (!UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to save grid world to slot '%s'"), *SaveSlotName);
	}
}
