// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Fishing/PSFishingTypes.h"
#include "PSTileTypes.h"
#include "PSGridWorld.generated.h"

class APSTileChunkActor;
class UPSGameTimeSubsystem;
class UPSDirtAutoTileSet;
class UPSGatheringWorldComponent;
class UPSBushWorldComponent;
class USkyLightComponent;
class UTextureCube;
class UDataTable;
struct FPSCropDefinition;

UCLASS()
class PEACESIGN_API APSGridWorld : public AActor
{
	GENERATED_BODY()

public:
	APSGridWorld();
	static constexpr int32 DefaultStoneHealth = 30;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gathering")
	TObjectPtr<UPSGatheringWorldComponent> Gathering;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gathering")
	TObjectPtr<UPSBushWorldComponent> Bushes;
	bool CanPlaceGatheringTree(FIntPoint Cell) const;
	int32 GetGatheringSeed() const { return WorldSeed; }
	void SaveGatheringState() const { SaveWorld(); }

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Grid World")
	FIntPoint WorldToCell(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Grid World")
	FVector CellToWorldCenter(FIntPoint Cell) const;
	UFUNCTION(BlueprintPure, Category="Grid World|Water") FVector WaterVisualLocation(FIntPoint Cell) const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grid World|Water") bool bUseNativeWater = true;
	/** Temporarily supply the second water's daylight reflections; restore when disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grid World|Water") bool bSupplyNativeWaterReflections = true;
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Grid World|Water") void RefreshWaterRendering();

	UFUNCTION(BlueprintPure, Category = "Grid World")
	EPSTileType GetGroundTile(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Grid World|Objects")
	EPSWorldObjectType GetWorldObjectType(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Mining")
	int32 GetStoneHealth(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Mining")
	EPSMineralType GetMineralType(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Mining")
	static int32 GetStoneMaxHealth(EPSMineralType MineralType);

	/** Mine from land up to two cells straight or one cell diagonally. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool CanMineFrom(FIntPoint PlayerCell, FIntPoint StoneCell) const;

	UFUNCTION(BlueprintCallable, Category = "Mining")
	EPSTileInteractionResult MineCell(FIntPoint Cell, int32 Damage = 1);

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
	const FPSCropDefinition* FindCropDefinition(int32 CropId) const;
	double GetCropPlantingMinute(FIntPoint Cell) const;
	double GetCropGrowthMinutes(FIntPoint Cell) const;

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult TillCell(FIntPoint Cell);
	UFUNCTION(BlueprintPure, Category = "Farming") bool CanTillCell(FIntPoint Cell) const;
	UFUNCTION(BlueprintPure, Category = "Farming") bool CanWaterCell(FIntPoint Cell) const;

	/** Toggle unoccupied grass/dirt and refresh neighboring terrain transitions. */
	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult ShovelCell(FIntPoint Cell);

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult PlantSeed(FIntPoint Cell, int32 CropId = 0);
	UFUNCTION(BlueprintCallable, Category = "Farming")
	EPSTileInteractionResult WaterCell(FIntPoint Cell);
	UFUNCTION(BlueprintPure, Category = "Farming")
	bool IsCellWatered(FIntPoint Cell) const;

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult HarvestCrop(FIntPoint Cell);
	EPSTileInteractionResult HarvestCropForPlayer(FIntPoint Cell);

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	EPSTileInteractionResult RemoveCrop(FIntPoint Cell);

	UFUNCTION(BlueprintCallable, Category = "Grid World")
	bool ResetWorld();

	UFUNCTION(CallInEditor, Category = "Grid World|Editor Preview", meta = (DisplayName = "Generate Grid Preview"))
	void GenerateEditorPreview();

	UFUNCTION(CallInEditor, Category = "Grid World|Editor Preview", meta = (DisplayName = "Clear Grid Preview"))
	void ClearEditorPreview();

	/** Add the selected stamp to the map's authored layout; does not overwrite save files. */
	UFUNCTION(CallInEditor, Category = "Grid World|Tile Stamps", meta = (DisplayName = "Add Tile Stamp"))
	void AddTileStamp();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid World|Tile Stamps")
	FPSTileStamp SelectedTileStamp;

	/** Later stamps win overlaps. Existing saved cells take precedence at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid World|Tile Stamps")
	TArray<FPSTileStamp> TileStamps;

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
	int32 ChunkSize = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World", meta = (ClampMin = "0", ClampMax = "8"))
	int32 LoadRadius = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World", meta = (ClampMin = "1"))
	int32 WorldHalfExtentInCells = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World")
	int32 WorldSeed = 1337;

	/** 이 Grid World의 물 타일에 적용할 낚시 장소 ID. 현재 절차 생성 물은 호수다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Fishing")
	EPSFishingLocation FishingLocation = EPSFishingLocation::Lake;
	/** Temporary authored fishing area; enabled only on the development map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Fishing")
	bool bEnableFishingTestPond = false;
	/** Lowest X/Y water cell. The pond is 8x8 with a central 2x2 grass island. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Fishing", meta = (EditCondition = "bEnableFishingTestPond"))
	FIntPoint FishingTestPondOrigin = FIntPoint(2, -4);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Save")
	FString SaveSlotName = TEXT("PeaceSignWorld");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Mining", meta = (ClampMin = "1"))
	int32 PlainStoneSpawnWeight = 70;

	/** Data-driven weights; higher tiers become eligible in farther distance bands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Mining")
	TArray<FPSMineralSpawnRule> MineralSpawnRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Mining")
	FPSOreGenerationSettings OreGenerationSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Rendering")
	TSubclassOf<APSTileChunkActor> ChunkActorClass;

	/** Optional rule override. The imported Dirt Set1-8 textures are used when this is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid World|Rendering")
	TObjectPtr<UPSDirtAutoTileSet> DirtAutoTileSet;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Grid World|Editor Preview", meta = (ClampMin = "1", ClampMax = "128"))
	int32 EditorPreviewHalfExtentInCells = 50;

	UPROPERTY(VisibleInstanceOnly, Category = "Grid World|Editor Preview")
	TObjectPtr<APSTileChunkActor> EditorPreviewActor;
#endif

private:
	UPROPERTY(Transient) TObjectPtr<USkyLightComponent> NativeWaterReflections;
	UPROPERTY() TObjectPtr<UTextureCube> NativeWaterReflectionCube;
	void UpdateWaterLighting();
	friend class FPSGatheringTest;
	friend class FPSBushTest;
	friend class FPSGameDataTablesTest;
	friend class FPSGatheringInventoryTest;
	friend class FPSPlacedTreeSpriteTest;
	friend class FPSWorldResetInventoryTest;
	friend class FPSDirtAutoTileTest;
	friend class FPSTileStampTest;
	friend class FPSShovelTest;
	friend class FPSWaterAutoTileTest;
	friend class FPSCropGrowthTest;
	friend class FPSFarmVisualTest;
	friend class FPSFarmToolActionTest;
	friend class FPSChargedHoeTest;
	friend class FPSSecondHoeTest;
	friend class FPSFishingTest;
	friend class FPSFishingOct26Test;
	friend class FPSFarmingInventoryTest;
	friend class FPSMiningTest;
	friend class FPSOreVeinTest;
	bool IsMiningEligible(FIntPoint Cell) const;
	void EnsureMiningGeneration() const;
	const FPSChunkData& GetGeneratedMiningChunk(FIntPoint ChunkCoordinate) const;
	uint32 MiningInputsHash() const;
	bool IsLakeCell(FIntPoint Cell) const;
	bool IsDirtPatchCell(FIntPoint Cell) const;
	bool IsRawDirtPatchCell(FIntPoint Cell) const;
	bool BuildStampedCell(FIntPoint Cell, FPSTileCell& OutCell) const;
	bool BuildFishingTestPondCell(FIntPoint Cell, FPSTileCell& OutCell) const;
	void ApplyFishingTestPond();
	void EnsureGrowthUpdates();
	UFUNCTION() void HandleClockChanged();
	UFUNCTION() void HandleFarmGrowthChanged();
	void AdvanceFarmGrowth(double ToMinute);
	double GetFarmMinute() const;
	int64 GetGrowthHalfHour() const;
	UPROPERTY(Transient) TObjectPtr<UPSGameTimeSubsystem> GrowthClock;
	UPROPERTY(EditDefaultsOnly, Category="Farming")
	TSoftObjectPtr<UDataTable> CropDataTable{FSoftObjectPath(TEXT("/Game/DataBase/DT_Crop.DT_Crop"))};
	double FarmGrowthMinute = -1;
	EPSTileType GenerateGroundTile(FIntPoint Cell) const;
	EPSWorldObjectType GenerateWorldObjectType(FIntPoint Cell) const;
	EPSMineralType GenerateMineralType(FIntPoint Cell) const;
	FPSChunkData GenerateChunk(FIntPoint ChunkCoordinate) const;
	FPSChunkData BuildRenderChunkData(FIntPoint ChunkCoordinate, const FPSChunkData& Source) const;
	uint16 BuildDirtNeighborMask(FIntPoint Cell) const;
	uint16 BuildTerrainNeighborMask(FIntPoint Cell, EPSTileType Terrain) const;
	FPSChunkData& GetOrCreateChunk(FIntPoint ChunkCoordinate);
	void UpdateActiveChunks(FIntPoint PlayerChunk);
	void SpawnChunkRenderer(FIntPoint ChunkCoordinate);
	void RebuildChunk(FIntPoint ChunkCoordinate);
	void RebuildChunksAroundCell(FIntPoint Cell);
	bool SetGroundTile(FIntPoint Cell, EPSTileType GroundType);
	bool SetCropType(FIntPoint Cell, EPSCropType CropType, int32 CropId = -1, bool bSave = true);
	const FPSTileCell* FindSavedFarmTile(FIntPoint Cell) const;
	bool IsCellInsideWorld(FIntPoint Cell) const;
	void LoadWorld();
	void SaveWorld() const;

	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<APSTileChunkActor>> ActiveChunkActors;

	TMap<FIntPoint, FPSChunkData> LoadedChunks;
	TMap<FIntPoint, FPSChunkSaveData> ModifiedChunks;
	// Unvisited query/preview results are cached but do not count as visited saves.
	mutable TMap<FIntPoint, FPSChunkData> GeneratedMiningChunks;
	mutable bool bMiningGenerationReady = false;
	mutable uint32 CachedMiningInputsHash = 0;
	mutable double MinimumMiningDistance = 0.0;
	mutable double MaximumMiningDistance = 0.0;
	int32 MiningGenerationVersion = 1;
	int32 StartingWorldSeed = 1337;
	FIntPoint LastPlayerChunk = FIntPoint(MAX_int32, MAX_int32);
};
