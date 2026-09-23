// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Fishing/PSFishingTypes.h"
#include "PSTileTypes.h"
#include "PSGridWorld.generated.h"

class APSTileChunkActor;
class UPSGameTimeSubsystem;

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

	UFUNCTION(BlueprintPure, Category = "Grid World")
	EPSTileType GetGroundTile(FIntPoint Cell) const;

	// Cast from land up to two cells in a straight line or one cell diagonally.
	UFUNCTION(BlueprintPure, Category = "Fishing")
	bool CanFishFrom(FIntPoint PlayerCell, FIntPoint WaterCell) const;

	/** 0=바다, 1=강, 2=호수. 물이 아닌 칸은 INDEX_NONE을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Fishing")
	int32 GetFishingLocationId(FIntPoint WaterCell) const;

	UFUNCTION(BlueprintPure, Category = "Grid World")
	EPSCropType GetCropType(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Grid World")
	int32 GetCropId(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Grid World")
	int32 GetCropStage(FIntPoint Cell) const;

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult TillCell(FIntPoint Cell);

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult PlantSeed(FIntPoint Cell, int32 CropId = 0);

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult HarvestCrop(FIntPoint Cell);

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult RemoveCrop(FIntPoint Cell);

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	bool ResetWorld();

	UFUNCTION(CallInEditor, Category = "Grid World|Editor Preview", meta = (DisplayName = "Generate Grid Preview"))
	void GenerateEditorPreview();

	UFUNCTION(CallInEditor, Category = "Grid World|Editor Preview", meta = (DisplayName = "Clear Grid Preview"))
	void ClearEditorPreview();

	UFUNCTION(BlueprintPure, Category = "Grid World")
	float GetCellSize() const { return CellSize; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid World")
	TObjectPtr<USceneComponent> SceneRoot;

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

	/** 이 Grid World의 물 타일에 적용할 낚시 장소 ID. 현재 절차 생성 물은 호수다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Fishing")
	EPSFishingLocation FishingLocation = EPSFishingLocation::Lake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Save")
	FString SaveSlotName = TEXT("PeaceSignWorld");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Rendering")
	TSubclassOf<APSTileChunkActor> ChunkActorClass;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Grid World|Editor Preview", meta = (ClampMin = "1", ClampMax = "128"))
	int32 EditorPreviewHalfExtentInCells = 50;

	UPROPERTY(VisibleInstanceOnly, Category = "Grid World|Editor Preview")
	TObjectPtr<APSTileChunkActor> EditorPreviewActor;
#endif

private:
	friend class FPSCropGrowthTest;
	friend class FPSFishingTest;
	friend class FPSFarmingInventoryTest;
	bool IsLakeCell(FIntPoint Cell) const;
	void EnsureGrowthUpdates();
	UFUNCTION() void HandleClockChanged();
	int64 GetGrowthHalfHour() const;
	UPROPERTY(Transient) TObjectPtr<UPSGameTimeSubsystem> GrowthClock;
	// Mature crops are omitted. A timestamp bucket shares one growth calculation.
	TMap<int64, TArray<FIntPoint>> GrowingCrops;
	EPSTileType GenerateGroundTile(FIntPoint Cell) const;
	FPSChunkData GenerateChunk(FIntPoint ChunkCoordinate) const;
	FPSChunkData& GetOrCreateChunk(FIntPoint ChunkCoordinate);
	void UpdateActiveChunks(FIntPoint PlayerChunk);
	void SpawnChunkRenderer(FIntPoint ChunkCoordinate);
	void RebuildChunk(FIntPoint ChunkCoordinate);
	bool SetGroundTile(FIntPoint Cell, EPSTileType GroundType);
	bool SetCropType(FIntPoint Cell, EPSCropType CropType, int32 CropId = -1);
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
