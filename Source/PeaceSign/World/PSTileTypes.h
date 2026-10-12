// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PSTileTypes.generated.h"

UENUM(BlueprintType)
enum class EPSTileType : uint8
{
	Empty,
	Grass,
	Dirt,
	/** Legacy save value. Mineable rocks now live in the object layer. */
	Stone UMETA(Hidden),
	TilledSoil,
	Water
};

/** Occupant rendered and interacted with independently from the ground tile. */
UENUM(BlueprintType)
enum class EPSWorldObjectType : uint8
{
	None,
	Stone
};

UENUM(BlueprintType)
enum class EPSTileInteractionResult : uint8
{
	InvalidCell,
	NoEffect,
	Tilled,
	StoneDamaged,
	Mined,
	Planted,
	Harvested,
	CropRemoved,
	InventoryFull,
	GroundChanged
};

UENUM(BlueprintType)
enum class EPSCropType : uint8
{
	None,
	TestCrop
};

/** Optional ore embedded in a stone block. Plain stone uses None. */
UENUM(BlueprintType)
enum class EPSMineralType : uint8
{
	None,
	Copper,
	Iron,
	Silver,
	Gold,
	Titanium,
	Lumistone,
	Asterium
};

USTRUCT(BlueprintType)
struct FPSMineralSpawnRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPSMineralType MineralType = EPSMineralType::None;

	/** The world is split into eight distance bands, numbered 0 through 7. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "7"))
	int32 MinimumDistanceBand = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Weight = 0;
};

/** Inclusive cell bounds reserved for buildings or required progression routes. */
USTRUCT(BlueprintType)
struct FPSMiningExclusionArea
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FIntPoint Min = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FIntPoint Max = FIntPoint::ZeroValue;
};

USTRUCT(BlueprintType)
struct FPSOreGenerationSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FIntPoint BaseCell = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0")) int32 SafeRadius = 3;
	/** Keep a three-cell-wide cross through the base open in the prototype world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bReserveBaseRoutes = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FPSMiningExclusionArea> Exclusions;
	/** Temporary rock density, independent of the mineral selection probabilities. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0", ClampMax = "100")) int32 RockPatchChance = 65;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "4", ClampMax = "64")) int32 RockPatchSize = 24;
};

/** Original membership remains saved even after individual blocks are mined. */
USTRUCT()
struct FPSOreVein
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FIntPoint Center = FIntPoint::ZeroValue;
	UPROPERTY(SaveGame) EPSMineralType MineralType = EPSMineralType::None;
	UPROPERTY(SaveGame) TArray<FIntPoint> Cells;
};

USTRUCT(BlueprintType)
struct FPSTileCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EPSTileType GroundType = EPSTileType::Grass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EPSWorldObjectType ObjectType = EPSWorldObjectType::None;

	/** Remaining durability for the object occupying this cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0"))
	int32 ObjectHealth = 0;

	/** Kept only so version 0-2 saves can migrate their old stone-tile health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "0"))
	int32 StoneHealth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EPSMineralType MineralType = EPSMineralType::None;

	/** Index into the owning chunk's original veins; INDEX_NONE denotes plain rock. */
	UPROPERTY(SaveGame)
	int32 VeinId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EPSCropType CropType = EPSCropType::None;

	/** ID from the crop data sheet. INDEX_NONE means that no crop is planted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 CropId = INDEX_NONE;

	// -1 denotes a legacy crop without a planting timestamp.
	UPROPERTY(SaveGame)
	int64 PlantedHalfHour = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (ClampMin = "1", ClampMax = "5"))
	uint8 GrowthStage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	uint8 Variant = 0;

	/** Authored stamps keep their exact source piece instead of running auto-tiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bFixedVariant = false;

	/** A grass-centered piece may still use the water artwork family. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bWaterArtwork = false;
};

USTRUCT(BlueprintType)
struct FPSTileStamp
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Origin = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "11"))
	int32 Set = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWater = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Set == 6"))
	bool bVertical = false;
};

USTRUCT()
struct FPSChunkData
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	UPROPERTY()
	TArray<FPSTileCell> Cells;

	UPROPERTY()
	TArray<FPSOreVein> Veins;
};

USTRUCT()
struct FPSChunkSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	UPROPERTY(SaveGame)
	TArray<FPSTileCell> Cells;

	UPROPERTY(SaveGame)
	TArray<FPSOreVein> Veins;
};

namespace PSGrid
{
	inline int32 FloorDivide(const int32 Value, const int32 Divisor)
	{
		check(Divisor > 0);
		return FMath::FloorToInt(static_cast<double>(Value) / static_cast<double>(Divisor));
	}

	inline int32 PositiveModulo(const int32 Value, const int32 Divisor)
	{
		check(Divisor > 0);
		const int32 Remainder = Value % Divisor;
		return Remainder < 0 ? Remainder + Divisor : Remainder;
	}

	inline FIntPoint WorldToCell(const FVector& WorldPosition, const float CellSize)
	{
		check(CellSize > 0.0f);
		return FIntPoint(
			FMath::FloorToInt(WorldPosition.X / CellSize),
			FMath::FloorToInt(WorldPosition.Y / CellSize));
	}

	inline FVector CellToWorldCenter(const FIntPoint Cell, const float CellSize, const float Z = 0.0f)
	{
		return FVector(
			(static_cast<float>(Cell.X) + 0.5f) * CellSize,
			(static_cast<float>(Cell.Y) + 0.5f) * CellSize,
			Z);
	}

	inline FIntPoint CellToChunk(const FIntPoint Cell, const int32 ChunkSize)
	{
		return FIntPoint(FloorDivide(Cell.X, ChunkSize), FloorDivide(Cell.Y, ChunkSize));
	}

	inline FIntPoint CellToLocal(const FIntPoint Cell, const int32 ChunkSize)
	{
		return FIntPoint(PositiveModulo(Cell.X, ChunkSize), PositiveModulo(Cell.Y, ChunkSize));
	}

	inline int32 LocalToIndex(const FIntPoint LocalCell, const int32 ChunkSize)
	{
		return LocalCell.Y * ChunkSize + LocalCell.X;
	}
}
