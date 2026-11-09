#include "PSGridWorld.h"
#include "PSCropGrowth.h"
#include "../Data/PSCropDefinition.h"
#include "../Time/PSGameTimeSubsystem.h"

namespace
{
	void ClearDeadCrop(FPSTileCell& Tile)
	{
		Tile.CropType = EPSCropType::None;
		Tile.CropId = INDEX_NONE;
		Tile.PlantedHalfHour = -1;
		Tile.PlantedFarmMinute = -1;
		Tile.EffectiveGrowthMinutes = 0;
		Tile.GrowthStage = 1;
		Tile.DryDays = 0;
	}
}

const FPSCropDefinition* APSGridWorld::FindCropDefinition(const int32 CropId) const
{
	if (CropId <= 0) return nullptr;
	const UDataTable* Table = CropDataTable.LoadSynchronous();
	const auto* Row = Table && Table->GetRowStruct() == FPSCropDefinition::StaticStruct()
		? Table->FindRow<FPSCropDefinition>(FName(*FString::Printf(TEXT("%06d"), CropId)), TEXT("Crop"), false) : nullptr;
	return Row && Row->Season >= 0 && Row->Season < 4 && Row->GRtime > 0
		&& Row->GRtime <= MAX_int32 / PSCropGrowth::MinutesPerDay && Row->LV >= 0 ? Row : nullptr;
}

const FPSTileCell* APSGridWorld::FindSavedFarmTile(const FIntPoint Cell) const
{
	if (!IsCellInsideWorld(Cell)) return nullptr;
	const auto* Saved = ModifiedChunks.Find(PSGrid::CellToChunk(Cell, ChunkSize));
	const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, ChunkSize), ChunkSize);
	return Saved && Saved->Cells.IsValidIndex(Index) ? &Saved->Cells[Index] : nullptr;
}

double APSGridWorld::GetCropPlantingMinute(const FIntPoint Cell) const
{
	const auto* Tile = FindSavedFarmTile(Cell);
	return Tile ? Tile->PlantedFarmMinute : -1;
}

double APSGridWorld::GetCropGrowthMinutes(const FIntPoint Cell) const
{
	const auto* Tile = FindSavedFarmTile(Cell);
	return Tile ? Tile->EffectiveGrowthMinutes : 0;
}

double APSGridWorld::GetFarmMinute() const
{
	return GrowthClock ? GrowthClock->GetAbsoluteGameMinute() : PSCropGrowth::DawnMinute;
}

void APSGridWorld::AdvanceFarmGrowth(const double ToMinute)
{
	using namespace PSCropGrowth;
	if (ToMinute <= FarmGrowthMinute) return;
	const double From = FarmGrowthMinute;
	FarmGrowthMinute = ToMinute;
	const int64 FirstDawn = (FMath::FloorToInt64((From - DawnMinute) / MinutesPerDay) + 1) * MinutesPerDay + DawnMinute;
	const bool bCrossedDawn = FirstDawn <= ToMinute;
	TArray<FIntPoint> VisualChanges;
	TMap<int32, const FPSCropDefinition*> Definitions;
	for (auto& Pair : ModifiedChunks)
	{
		bool bChanged = false;
		bool bVisual = false;
		for (FPSTileCell& Tile : Pair.Value.Cells)
		{
			if (Tile.GroundType != EPSTileType::TilledSoil) continue;
			const auto OldCrop = Tile.CropType;
			const uint8 OldStage = Tile.GrowthStage;
			const uint8 OldDryDays = Tile.DryDays;
			const double OldMinutes = Tile.EffectiveGrowthMinutes;
			const bool bWasWet = Tile.bWatered;
			if (Tile.CropType != EPSCropType::None)
			{
				if (!Definitions.Contains(Tile.CropId)) Definitions.Add(Tile.CropId, FindCropDefinition(Tile.CropId));
				const FPSCropDefinition* Definition = Definitions.FindChecked(Tile.CropId);
				const int32 Required = Definition ? Definition->GRtime * MinutesPerDay : LegacyGrowthMinutes;
				// With no automatic irrigation, the first dawn is the only boundary
				// before which a wet plot can grow during a multi-day time skip.
				if (Tile.bWatered && Tile.GrowthStage < MaxStage)
				{
					const double WetEnd = FMath::Min(ToMinute, static_cast<double>(FirstDawn));
					Tile.EffectiveGrowthMinutes = FMath::Min(static_cast<double>(Required),
						Tile.EffectiveGrowthMinutes + FMath::Max(0.0, WetEnd - From));
					Tile.GrowthStage = GetStageForMinutes(Tile.EffectiveGrowthMinutes, Required);
				}
				bool bWetAtDawn = Tile.bWatered;
				for (int64 Dawn = FirstDawn; Dawn <= ToMinute; Dawn += MinutesPerDay)
				{
					if (Definition && UPSGameTimeSubsystem::GetSeasonIndexForDay(Dawn / MinutesPerDay + 1) != Definition->Season)
					{
						ClearDeadCrop(Tile);
						break;
					}
					if (Tile.GrowthStage == MaxStage && !Definition) break;
					if (Tile.GrowthStage < MaxStage)
					{
						if (bWetAtDawn) Tile.DryDays = 0;
						// A crop's first dawn never counts as a missed watering day.
						else if (Tile.PlantedFarmMinute < Dawn - MinutesPerDay && ++Tile.DryDays >= 2)
						{
							ClearDeadCrop(Tile);
							break;
						}
					}
					bWetAtDawn = false;
				}
			}
			if (bCrossedDawn) Tile.bWatered = false;
			const bool bTileVisual = OldCrop != Tile.CropType || OldStage != Tile.GrowthStage || bWasWet != Tile.bWatered;
			bVisual |= bTileVisual;
			bChanged |= bTileVisual || OldDryDays != Tile.DryDays || OldMinutes != Tile.EffectiveGrowthMinutes;
		}
		if (!bChanged) continue;
		if (FPSChunkData* Loaded = LoadedChunks.Find(Pair.Key)) Loaded->Cells = Pair.Value.Cells;
		if (bVisual) VisualChanges.Add(Pair.Key);
	}
	for (const auto Chunk : VisualChanges) RebuildChunk(Chunk);
}
