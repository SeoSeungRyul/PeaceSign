// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSTileTypes.h"
#include "PSGridWorld.generated.h"

class APSTileChunkActor;

UCLASS()
class PEACESIGN_API APSGridWorld : public AActor
{
	GENERATED_BODY()

public:
	APSGridWorld();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Grid World")
	FIntPoint WorldToCell(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Grid World")
	FVector CellToWorldCenter(FIntPoint Cell) const;

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	bool ToggleGroundTile(FIntPoint Cell);

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	bool ResetWorld();

	UFUNCTION(BlueprintPure, Category = "Grid World")
	float GetCellSize() const { return CellSize; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World", meta = (ClampMin = "1.0"))
	float CellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World", meta = (ClampMin = "1", ClampMax = "64"))
	int32 ChunkSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World", meta = (ClampMin = "0", ClampMax = "8"))
	int32 LoadRadius = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World", meta = (ClampMin = "1"))
	int32 WorldHalfExtentInCells = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World")
	int32 WorldSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Save")
	FString SaveSlotName = TEXT("PeaceSignWorld");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Rendering")
	TSubclassOf<APSTileChunkActor> ChunkActorClass;

private:
	FPSChunkData GenerateChunk(FIntPoint ChunkCoordinate) const;
	FPSChunkData& GetOrCreateChunk(FIntPoint ChunkCoordinate);
	void UpdateActiveChunks(FIntPoint PlayerChunk);
	void SpawnChunkRenderer(FIntPoint ChunkCoordinate);
	void RebuildChunk(FIntPoint ChunkCoordinate);
	bool IsCellInsideWorld(FIntPoint Cell) const;
	void LoadWorld();
	void SaveWorld() const;

	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<APSTileChunkActor>> ActiveChunkActors;

	TMap<FIntPoint, FPSChunkData> LoadedChunks;
	TMap<FIntPoint, FPSChunkSaveData> ModifiedChunks;
	int32 StartingWorldSeed = 1337;
	FIntPoint LastPlayerChunk = FIntPoint(MAX_int32, MAX_int32);
};
