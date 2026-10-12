#include "PSOreVeinGeneration.h"

namespace
{
	const FIntPoint Directions[] = {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)};
	// Each row sums to 100. These select candidate kinds, not terrain density or loot.
	constexpr int32 MineralProbabilities[8][8] = {
		{70, 30, 0, 0, 0, 0, 0, 0}, {35, 45, 20, 0, 0, 0, 0, 0},
		{25, 20, 40, 15, 0, 0, 0, 0}, {20, 10, 15, 40, 15, 0, 0, 0},
		{20, 5, 10, 15, 35, 15, 0, 0}, {18, 4, 6, 10, 15, 32, 15, 0},
		{15, 3, 5, 7, 10, 15, 30, 15}, {15, 2, 3, 5, 10, 15, 20, 30}};
}

uint32 PSOreGeneration::SeedFor(const FIntPoint Coordinate, const int32 WorldSeed, const uint32 Salt)
{
	uint32 Hash = static_cast<uint32>(Coordinate.X) * 0x8da6b343u
		^ static_cast<uint32>(Coordinate.Y) * 0xd8163841u
		^ static_cast<uint32>(WorldSeed) * 0xcb1ab31fu ^ Salt;
	Hash ^= Hash >> 16; Hash *= 0x7feb352du;
	Hash ^= Hash >> 15; Hash *= 0x846ca68bu;
	return Hash ^ (Hash >> 16);
}

int32 PSOreGeneration::DistanceBand(const double Distance, const double Minimum, const double Maximum)
{
	if (Maximum <= Minimum) return 0;
	return FMath::Clamp(FMath::FloorToInt(FMath::Clamp((Distance - Minimum) / (Maximum - Minimum), 0.0, 1.0) * 8.0), 0, 7);
}

EPSMineralType PSOreGeneration::SelectMineral(const int32 Band, int32 PercentRoll)
{
	PercentRoll = FMath::Clamp(PercentRoll, 0, 99);
	for (int32 Mineral = 0; Mineral < 8; ++Mineral)
	{
		if (PercentRoll < MineralProbabilities[FMath::Clamp(Band, 0, 7)][Mineral])
			return static_cast<EPSMineralType>(Mineral);
		PercentRoll -= MineralProbabilities[FMath::Clamp(Band, 0, 7)][Mineral];
	}
	return EPSMineralType::None;
}

bool PSOreGeneration::IsReserved(const FIntPoint Cell, const FPSOreGenerationSettings& Settings)
{
	const FIntPoint Offset = Cell - Settings.BaseCell;
	if (FMath::Max(FMath::Abs(Offset.X), FMath::Abs(Offset.Y)) <= FMath::Max(0, Settings.SafeRadius)) return true;
	if (Settings.bReserveBaseRoutes && (FMath::Abs(Offset.X) <= 1 || FMath::Abs(Offset.Y) <= 1)) return true;
	for (const FPSMiningExclusionArea& Area : Settings.Exclusions)
		if (Cell.X >= Area.Min.X && Cell.X <= Area.Max.X && Cell.Y >= Area.Min.Y && Cell.Y <= Area.Max.Y) return true;
	return false;
}

TArray<FIntPoint> PSOreGeneration::BuildRockPatch(const FIntPoint Region, const int32 WorldSeed,
	const FPSOreGenerationSettings& Settings, TFunctionRef<bool(FIntPoint)> IsEligible)
{
	FRandomStream Random(static_cast<int32>(SeedFor(Region, WorldSeed, 0x726f636bu)));
	TArray<FIntPoint> Result;
	if (Random.RandRange(0, 99) >= FMath::Clamp(Settings.RockPatchChance, 0, 100)) return Result;
	TArray<FIntPoint> Eligible;
	const FIntPoint Origin = Region * RockRegionSize;
	for (int32 Y = 0; Y < RockRegionSize; ++Y)
		for (int32 X = 0; X < RockRegionSize; ++X)
			if (IsEligible(Origin + FIntPoint(X, Y))) Eligible.Add(Origin + FIntPoint(X, Y));
	if (Eligible.IsEmpty()) return Result;
	TArray<FIntPoint> Frontier = {Eligible[Random.RandRange(0, Eligible.Num() - 1)]};
	while (!Frontier.IsEmpty() && Result.Num() < FMath::Clamp(Settings.RockPatchSize, 4, 64))
	{
		const int32 Index = Random.RandRange(0, Frontier.Num() - 1);
		const FIntPoint Cell = Frontier[Index];
		Frontier.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		Result.Add(Cell);
		for (const FIntPoint Direction : Directions)
		{
			const FIntPoint Next = Cell + Direction;
			if (Eligible.Contains(Next) && !Result.Contains(Next)) Frontier.AddUnique(Next);
		}
	}
	return Result;
}

void PSOreGeneration::GenerateVeins(FPSChunkData& Chunk, const int32 ChunkSize, const int32 WorldSeed,
	const FIntPoint BaseCell, const double MinimumDistance, const double MaximumDistance)
{
	if (ChunkSize <= 0 || Chunk.Cells.Num() != ChunkSize * ChunkSize) return;
	FRandomStream Random(static_cast<int32>(SeedFor(Chunk.Coordinate, WorldSeed, 0x7665696eu)));
	TArray<int32> Eligible;
	for (int32 Index = 0; Index < Chunk.Cells.Num(); ++Index)
		if (Chunk.Cells[Index].ObjectType == EPSWorldObjectType::Stone) Eligible.Add(Index);
	TArray<FIntPoint> Centers;
	const FIntPoint Origin = Chunk.Coordinate * ChunkSize;
	const int32 CandidateCount = Random.RandRange(1, 3);
	const auto CanGrow = [&](const FIntPoint Local)
	{
		if (Local.X < 0 || Local.Y < 0 || Local.X >= ChunkSize || Local.Y >= ChunkSize) return false;
		const FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(Local, ChunkSize)];
		if (Tile.ObjectType != EPSWorldObjectType::Stone || Tile.VeinId != INDEX_NONE) return false;
		// Keep distinct veins from merging visually into an oversized same-kind vein.
		for (const FIntPoint Direction : Directions)
		{
			const FIntPoint Next = Local + Direction;
			if (Next.X >= 0 && Next.Y >= 0 && Next.X < ChunkSize && Next.Y < ChunkSize
				&& Chunk.Cells[PSGrid::LocalToIndex(Next, ChunkSize)].VeinId != INDEX_NONE) return false;
		}
		return true;
	};
	for (int32 Candidate = 0; Candidate < CandidateCount; ++Candidate)
	{
		if (Random.RandRange(0, Chunk.Cells.Num() - 1) >= Eligible.Num()) continue;
		TArray<FIntPoint> AvailableCenters;
		for (const int32 Index : Eligible)
		{
			const FIntPoint Local(Index % ChunkSize, Index / ChunkSize);
			if (!CanGrow(Local)) continue;
			bool bSpaced = true;
			for (const FIntPoint Center : Centers)
				if ((FVector2D(Local) - FVector2D(Center)).SizeSquared() < 36.0) { bSpaced = false; break; }
			if (bSpaced) AvailableCenters.Add(Local);
		}
		if (AvailableCenters.IsEmpty()) continue;
		const FIntPoint Center = AvailableCenters[Random.RandRange(0, AvailableCenters.Num() - 1)];
		Centers.Add(Center);
		const double Distance = (FVector2D(Origin + Center) - FVector2D(BaseCell)).Size();
		const EPSMineralType Mineral = SelectMineral(DistanceBand(Distance, MinimumDistance, MaximumDistance), Random.RandRange(0, 99));
		if (Mineral == EPSMineralType::None) continue;
		const int32 TargetCount = Random.RandRange(4, 10);
		TArray<FIntPoint> Selected;
		TArray<FIntPoint> Frontier = {Center};
		while (!Frontier.IsEmpty() && Selected.Num() < TargetCount)
		{
			const int32 Index = Random.RandRange(0, Frontier.Num() - 1);
			const FIntPoint Local = Frontier[Index];
			Frontier.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			Selected.Add(Local);
			for (const FIntPoint Direction : Directions)
			{
				const FIntPoint Next = Local + Direction;
				if (CanGrow(Next) && !Selected.Contains(Next)) Frontier.AddUnique(Next);
			}
		}
		if (Selected.Num() < 4) continue; // Do not leave partial minerals behind on cancellation.
		const int32 VeinId = Chunk.Veins.Num();
		FPSOreVein& Vein = Chunk.Veins.AddDefaulted_GetRef();
		Vein.Center = Origin + Center;
		Vein.MineralType = Mineral;
		for (const FIntPoint Local : Selected)
		{
			FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(Local, ChunkSize)];
			Tile.VeinId = VeinId;
			Tile.MineralType = Mineral;
			Vein.Cells.Add(Origin + Local);
		}
	}
}
