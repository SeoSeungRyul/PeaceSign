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
	Stone,
	TilledSoil
};

UENUM(BlueprintType)
enum class EPSTileInteractionResult : uint8
{
	InvalidCell,
	NoEffect,
	Tilled,
	Mined,
	Planted
};

UENUM(BlueprintType)
enum class EPSCropType : uint8
{
	None,
	TestCrop
};

USTRUCT(BlueprintType)
struct FPSTileCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EPSTileType GroundType = EPSTileType::Grass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EPSCropType CropType = EPSCropType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	uint8 Variant = 0;
};

USTRUCT()
struct FPSChunkData
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	UPROPERTY()
	TArray<FPSTileCell> Cells;
};

USTRUCT()
struct FPSChunkSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	UPROPERTY(SaveGame)
	TArray<FPSTileCell> Cells;
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
