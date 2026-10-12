#pragma once

#include "CoreMinimal.h"
#include "PSTileTypes.h"

/** Version 1 implements the October 12 mining design. Keep this path for saved worlds. */
namespace PSOreGeneration
{
	constexpr int32 CurrentVersion = 1;
	constexpr int32 RockRegionSize = 8;
	uint32 SeedFor(FIntPoint Coordinate, int32 WorldSeed, uint32 Salt);
	int32 DistanceBand(double Distance, double Minimum, double Maximum);
	EPSMineralType SelectMineral(int32 Band, int32 PercentRoll);
	bool IsReserved(FIntPoint Cell, const FPSOreGenerationSettings& Settings);
	TArray<FIntPoint> BuildRockPatch(FIntPoint Region, int32 WorldSeed,
		const FPSOreGenerationSettings& Settings, TFunctionRef<bool(FIntPoint)> IsEligible);
	void GenerateVeins(FPSChunkData& Chunk, int32 ChunkSize, int32 WorldSeed,
		FIntPoint BaseCell, double MinimumDistance, double MaximumDistance);
}
