#include "PSWaterAutoTile.h"

namespace
{
	// Use only the unnumbered Set2 revision, never mix artwork revisions.
	constexpr const TCHAR* TexturePaths[] = {
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_LT.Water_Set1_LT"),
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_MT.Water_Set1_MT"),
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_RT.Water_Set1_RT"),
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_LM.Water_Set1_LM"),
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_MM.Water_Set1_MM"),
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_RM.Water_Set1_RM"),
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_LB.Water_Set1_LB"),
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_MB.Water_Set1_MB"),
		TEXT("/Game/Art/Tiles/Water/Set1/Water_Set1_RB.Water_Set1_RB"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_LT.Water_Set2_LT"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_MT.Water_Set2_MT"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_RT.Water_Set2_RT"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_LM.Water_Set2_LM"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_MM.Water_Set2_MM"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_RM.Water_Set2_RM"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_LB.Water_Set2_LB"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_MB.Water_Set2_MB"),
		TEXT("/Game/Art/Tiles/Water/Set2/Water_Set2_RB.Water_Set2_RB"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_LT.Water_Set3_LT"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_MT.Water_Set3_MT"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_RT.Water_Set3_RT"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_LM.Water_Set3_LM"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_MM.Water_Set3_MM"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_RM.Water_Set3_RM"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_LB.Water_Set3_LB"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_MB.Water_Set3_MB"),
		TEXT("/Game/Art/Tiles/Water/Set3/Water_Set3_RB.Water_Set3_RB"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_LT.Water_Set4_LT"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_MT.Water_Set4_MT"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_RT.Water_Set4_RT"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_LM.Water_Set4_LM"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_MM.Water_Set4_MM"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_RM.Water_Set4_RM"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_LB.Water_Set4_LB"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_MB.Water_Set4_MB"),
		TEXT("/Game/Art/Tiles/Water/Set4/Water_Set4_RB.Water_Set4_RB"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_LT.Water_Set5_LT"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_MT.Water_Set5_MT"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_RT.Water_Set5_RT"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_LM.Water_Set5_LM"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_MM.Water_Set5_MM"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_RM.Water_Set5_RM"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_LB.Water_Set5_LB"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_MB.Water_Set5_MB"),
		TEXT("/Game/Art/Tiles/Water/Set5/Water_Set5_RB.Water_Set5_RB"),
		TEXT("/Game/Art/Tiles/Water/Set6/Set6_H/Water_Set6_LH.Water_Set6_LH"),
		TEXT("/Game/Art/Tiles/Water/Set6/Set6_H/Water_Set6_stH.Water_Set6_stH"),
		TEXT("/Game/Art/Tiles/Water/Set6/Set6_H/Water_Set6_RH.Water_Set6_RH"),
		TEXT("/Game/Art/Tiles/Water/Set6/Set6_V/Water_Set6_TV.Water_Set6_TV"),
		TEXT("/Game/Art/Tiles/Water/Set6/Set6_V/Water_Set6_stV.Water_Set6_stV"),
		TEXT("/Game/Art/Tiles/Water/Set6/Set6_V/Water_Set6_BV.Water_Set6_BV"),
		TEXT("/Game/Art/Tiles/Water/Set7/Water_Set7_LT.Water_Set7_LT"),
		TEXT("/Game/Art/Tiles/Water/Set7/Water_Set7_RT.Water_Set7_RT"),
		TEXT("/Game/Art/Tiles/Water/Set7/Water_Set7_LB.Water_Set7_LB"),
		TEXT("/Game/Art/Tiles/Water/Set7/Water_Set7_RB.Water_Set7_RB"),
		TEXT("/Game/Art/Tiles/Water/Set8/Water_Set8_LT.Water_Set8_LT"),
		TEXT("/Game/Art/Tiles/Water/Set8/Water_Set8_RT.Water_Set8_RT"),
		TEXT("/Game/Art/Tiles/Water/Set8/Water_Set8_LB.Water_Set8_LB"),
		TEXT("/Game/Art/Tiles/Water/Set8/Water_Set8_RB.Water_Set8_RB"),
		TEXT("/Game/Art/Tiles/Water/Set9/Water_Set9_LT.Water_Set9_LT"),
		TEXT("/Game/Art/Tiles/Water/Set9/Water_Set9_RT.Water_Set9_RT"),
		TEXT("/Game/Art/Tiles/Water/Set9/Water_Set9_LB.Water_Set9_LB"),
		TEXT("/Game/Art/Tiles/Water/Set9/Water_Set9_RB.Water_Set9_RB"),
		TEXT("/Game/Art/Tiles/Water/Set10/Water_Set10_L.Water_Set10_L"),
		TEXT("/Game/Art/Tiles/Water/Set10/Water_Set10_R.Water_Set10_R"),
		TEXT("/Game/Art/Tiles/Water/Set11/Water_Set11_T.Water_Set11_T"),
		TEXT("/Game/Art/Tiles/Water/Set11/Water_Set11_L.Water_Set11_L"),
		TEXT("/Game/Art/Tiles/Water/Set11/Water_Set11_R.Water_Set11_R"),
		TEXT("/Game/Art/Tiles/Water/Set11/Water_Set11_B.Water_Set11_B")
	};
	static_assert(UE_ARRAY_COUNT(TexturePaths) == PSWaterAutoTile::VariantCount);
	constexpr uint16 AddedMasks[] = {398, 334, 283, 299, 295, 285, 327, 397, 287, 303, 399, 335, 351, 431, 267, 269, 263, 270};
	static_assert(UE_ARRAY_COUNT(AddedMasks) + 51 == PSWaterAutoTile::VariantCount);
}

uint16 PSWaterAutoTile::GetVariantMask(const uint8 Variant)
{
	if (Variant > 51 && Variant <= VariantCount) return AddedMasks[Variant - 52];
	return Variant <= 51 ? PSDirtAutoTile::GetDefaultVariantMask(Variant) : 0;
}

uint8 PSWaterAutoTile::SelectVariant(const uint16 Mask, const uint32 Seed)
{
	using namespace PSDirtAutoTile;
	const uint16 TextureMask = ToTextureMask(Mask);
	if ((TextureMask & Center) == 0) return 32;
	// Ignore diagonal occupancy where either adjacent cardinal is absent,
	// exactly as the shared Set1-6 selector does.
	uint16 Normalized = TextureMask & (Center | CardinalMask);
	if ((TextureMask & (North | West)) == (North | West)) Normalized |= TextureMask & NorthWest;
	if ((TextureMask & (North | East)) == (North | East)) Normalized |= TextureMask & NorthEast;
	if ((TextureMask & (South | East)) == (South | East)) Normalized |= TextureMask & SouthEast;
	if ((TextureMask & (South | West)) == (South | West)) Normalized |= TextureMask & SouthWest;
	for (uint8 Variant = 52; Variant <= VariantCount; ++Variant)
		if (GetVariantMask(Variant) == Normalized) return Variant;
	// Preserve old choices and fallback behavior for every other topology.
	return SelectDefaultVariant(TextureMask, Seed, 51);
}

const TCHAR* PSWaterAutoTile::GetVariantTexturePath(const uint8 Variant)
{
	return Variant > 0 && Variant <= VariantCount ? TexturePaths[Variant - 1] : nullptr;
}

uint16 PSWaterAutoTile::ToTextureMask(const uint16 WorldMask)
{
	using namespace PSDirtAutoTile;
	// Inverse of artwork yaw +90: local top (-Y) faces world +X.
	uint16 Result = WorldMask & Center;
	if (WorldMask & East) Result |= North;
	if (WorldMask & South) Result |= East;
	if (WorldMask & West) Result |= South;
	if (WorldMask & North) Result |= West;
	if (WorldMask & NorthEast) Result |= NorthWest;
	if (WorldMask & SouthEast) Result |= NorthEast;
	if (WorldMask & SouthWest) Result |= SouthEast;
	if (WorldMask & NorthWest) Result |= SouthWest;
	return Result;
}

uint8 PSWaterAutoTile::GetFixedTextureVariant(const uint8 Variant)
{
	if (Variant >= 1 && Variant <= 45)
	{
		const uint8 Base = ((Variant - 1) / 9) * 9;
		const uint8 Slot = (Variant - 1) % 9;
		const uint8 X = Slot % 3, Y = Slot / 3;
		return Base + (2 - X) * 3 + Y + 1;
	}
	// Preserve world H/V footprints using the perpendicular source strip.
	if (Variant >= 46 && Variant <= 48) return 51 - (Variant - 46);
	if (Variant >= 49 && Variant <= 51) return 46 + (Variant - 49);
	if (Variant >= 52 && Variant <= VariantCount)
	{
		const uint16 RotatedMask = ToTextureMask(GetVariantMask(Variant));
		for (uint8 Candidate = 52; Candidate <= VariantCount; ++Candidate)
			if (GetVariantMask(Candidate) == RotatedMask) return Candidate;
	}
	return 0;
}
