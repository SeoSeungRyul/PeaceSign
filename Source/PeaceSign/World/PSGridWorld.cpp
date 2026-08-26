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
	ChunkActorClass = APSTileChunkActor::StaticClass();
}

void APSGridWorld::BeginPlay()
{
	Super::BeginPlay();
	StartingWorldSeed = WorldSeed;
	LoadWorld();
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

bool APSGridWorld::ToggleGroundTile(const FIntPoint Cell)
{
	if (!IsCellInsideWorld(Cell))
	{
		return false;
	}

	const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(Cell, ChunkSize);
	const FIntPoint LocalCell = PSGrid::CellToLocal(Cell, ChunkSize);
	FPSChunkData& Chunk = GetOrCreateChunk(ChunkCoordinate);
	FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(LocalCell, ChunkSize)];
	Tile.GroundType = Tile.GroundType == EPSTileType::Dirt ? EPSTileType::Grass : EPSTileType::Dirt;

	FPSChunkSaveData& SavedChunk = ModifiedChunks.FindOrAdd(ChunkCoordinate);
	SavedChunk.Coordinate = ChunkCoordinate;
	SavedChunk.Cells = Chunk.Cells;

	RebuildChunk(ChunkCoordinate);
	SaveWorld();
	return true;
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
			if (!IsCellInsideWorld(Cell))
			{
				Tile.GroundType = EPSTileType::Empty;
				continue;
			}

			const uint32 Roll = HashCell(Cell, WorldSeed) % 100;
			Tile.GroundType = Roll < 5 ? EPSTileType::Stone : Roll < 15 ? EPSTileType::Dirt : EPSTileType::Grass;
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
