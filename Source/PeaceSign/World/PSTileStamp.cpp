#include "PSTileStamp.h"
#include "PSDirtAutoTileSet.h"
#include "PSWaterAutoTile.h"
#include "PSFarmAutoTile.h"

FIntPoint PSTileStamp::GetSize(const FPSTileStamp& Stamp)
{
	if (Stamp.bFarm)
	{
		if (Stamp.bWater || Stamp.Set < 1 || Stamp.Set > 4) return FIntPoint::ZeroValue;
		return Stamp.Set == 2 ? (Stamp.bVertical ? FIntPoint(1, 3) : FIntPoint(3, 1)) : FIntPoint(3, 3);
	}
	if (Stamp.Set < 1 || Stamp.Set > 12) return FIntPoint::ZeroValue;
	if (Stamp.Set == 10) return FIntPoint(2, 1);
	if (Stamp.Set >= 11) return FIntPoint(3, 3);
	if (Stamp.Set >= 7) return FIntPoint(2, 2);
	return Stamp.Set == 6
		? (Stamp.bVertical ? FIntPoint(1, 3) : FIntPoint(3, 1)) : FIntPoint(3, 3);
}

bool PSTileStamp::BuildCell(const FPSTileStamp& Stamp, const FIntPoint WorldCell, FPSTileCell& OutCell)
{
	const FIntPoint Size = GetSize(Stamp);
	const int64 X = static_cast<int64>(WorldCell.X) - Stamp.Origin.X;
	const int64 Y = static_cast<int64>(WorldCell.Y) - Stamp.Origin.Y;
	if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y) return false;
	if (Stamp.bFarm)
	{
		uint8 Variant = Stamp.Set == 1 ? static_cast<uint8>(1 + Y * 3 + X)
			: Stamp.Set == 2 ? static_cast<uint8>((Stamp.bVertical ? 13 : 10) + X + Y) : 0;
		if (Stamp.Set >= 3)
		{
			const uint8 First = Stamp.Set == 3 ? 16 : 20;
			if (X == 1 && Y == 0) Variant = First;
			else if (X == 0 && Y == 1) Variant = First + 1;
			else if (X == 2 && Y == 1) Variant = First + 2;
			else if (X == 1 && Y == 2) Variant = First + 3;
			else return false;
		}
		OutCell = FPSTileCell();
		OutCell.GroundType = EPSTileType::TilledSoil;
		OutCell.Variant = Variant;
		OutCell.FarmNeighborMask = PSFarmAutoTile::GetMask(Variant);
		OutCell.bFixedVariant = true;
		return true;
	}
	// Set11/12 are sparse crosses, not four-piece rectangles. Their empty slots
	// leave the existing/generated terrain untouched, including the center.
	uint8 CrossVariant = 0;
	if (Stamp.Set >= 11)
	{
		const uint8 Base = Stamp.Set == 11 ? 66 : 70;
		if (X == 1 && Y == 0) CrossVariant = Base;
		else if (X == 0 && Y == 1) CrossVariant = Base + 1;
		else if (X == 2 && Y == 1) CrossVariant = Base + 2;
		else if (X == 1 && Y == 2) CrossVariant = Base + 3;
		else return false;
	}
	const uint8 Variant = Stamp.Set >= 11 ? CrossVariant
		: Stamp.Set == 10 ? static_cast<uint8>(64 + X)
		: Stamp.Set >= 7
		? static_cast<uint8>(52 + (Stamp.Set - 7) * 4 + Y * 2 + X)
		: Stamp.Set == 6
		? static_cast<uint8>((Stamp.bVertical ? 49 : 46) + X + Y)
		: static_cast<uint8>((Stamp.Set - 1) * 9 + Y * 3 + X + 1);
	OutCell = FPSTileCell();
	const uint16 Mask = Stamp.bWater ? PSWaterAutoTile::GetVariantMask(Variant)
		: PSDirtAutoTile::GetDefaultVariantMask(Variant);
	OutCell.GroundType = (Mask & PSDirtAutoTile::Center)
		? (Stamp.bWater ? EPSTileType::Water : EPSTileType::Dirt) : EPSTileType::Grass;
	OutCell.Variant = Variant;
	OutCell.bFixedVariant = true;
	OutCell.bWaterArtwork = Stamp.bWater;
	return true;
}
