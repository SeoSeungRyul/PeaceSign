// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSDirtAutoTileSet.h"

#include "Engine/Texture2D.h"

namespace
{
	struct FPSDefaultDirtTileRule
	{
		uint16 Mask;
		const TCHAR* TexturePath;
	};

	// Variant zero means no transition texture. Entries are one-based when stored in FPSTileCell::Variant.
	constexpr FPSDefaultDirtTileRule DefaultRules[] = {
		{447, TEXT("/Game/Art/Tiles/Dirt/Set1/1_Dirt_Set1_LT.1_Dirt_Set1_LT")},
		{315, TEXT("/Game/Art/Tiles/Dirt/Set1/2_Dirt_Set1_MT.2_Dirt_Set1_MT")},
		{383, TEXT("/Game/Art/Tiles/Dirt/Set1/3_Dirt_Set1_RT.3_Dirt_Set1_RT")},
		{413, TEXT("/Game/Art/Tiles/Dirt/Set1/4_Dirt_Set1_LM.4_Dirt_Set1_LM")},
		{256, TEXT("/Game/Art/Tiles/Dirt/Set1/5_Dirt_Set1_MM.5_Dirt_Set1_MM")},
		{359, TEXT("/Game/Art/Tiles/Dirt/Set1/6_Dirt_Set1_RM.6_Dirt_Set1_RM")},
		{479, TEXT("/Game/Art/Tiles/Dirt/Set1/7_Dirt_Set1_LB.7_Dirt_Set1_LB")},
		{462, TEXT("/Game/Art/Tiles/Dirt/Set1/8_Dirt_Set1_MB.8_Dirt_Set1_MB")},
		{495, TEXT("/Game/Art/Tiles/Dirt/Set1/9_Dirt_Set1_RB.9_Dirt_Set1_RB")},
		{326, TEXT("/Game/Art/Tiles/Dirt/Set2/1_Dirt_Set2_LT.1_Dirt_Set2_LT")},
		{462, TEXT("/Game/Art/Tiles/Dirt/Set2/2_Dirt_Set2_MT.2_Dirt_Set2_MT")},
		{396, TEXT("/Game/Art/Tiles/Dirt/Set2/3_Dirt_Set2_RT.3_Dirt_Set2_RT")},
		{359, TEXT("/Game/Art/Tiles/Dirt/Set2/4_Dirt_Set2_LM.4_Dirt_Set2_LM")},
		{255, TEXT("/Game/Art/Tiles/Dirt/Set2/5_Dirt_Set2_MM.5_Dirt_Set2_MM")},
		{413, TEXT("/Game/Art/Tiles/Dirt/Set2/6_Dirt_Set2_RM.6_Dirt_Set2_RM")},
		{291, TEXT("/Game/Art/Tiles/Dirt/Set2/7_Dirt_Set2_LB.7_Dirt_Set2_LB")},
		{315, TEXT("/Game/Art/Tiles/Dirt/Set2/8_Dirt_Set2_MB.8_Dirt_Set2_MB")},
		{281, TEXT("/Game/Art/Tiles/Dirt/Set2/9_Dirt_Set2_RB.9_Dirt_Set2_RB")},
		{326, TEXT("/Game/Art/Tiles/Dirt/Set3/1_Dirt_Set3_LT.1_Dirt_Set3_LT")},
		{463, TEXT("/Game/Art/Tiles/Dirt/Set3/2_Dirt_Set3_MT.2_Dirt_Set3_MT")},
		{396, TEXT("/Game/Art/Tiles/Dirt/Set3/3_Dirt_Set3_RT.3_Dirt_Set3_RT")},
		{367, TEXT("/Game/Art/Tiles/Dirt/Set3/4_Dirt_Set3_LM.4_Dirt_Set3_LM")},
		{511, TEXT("/Game/Art/Tiles/Dirt/Set3/5_Dirt_Set3_MM.5_Dirt_Set3_MM")},
		{415, TEXT("/Game/Art/Tiles/Dirt/Set3/6_Dirt_Set3_RM.6_Dirt_Set3_RM")},
		{291, TEXT("/Game/Art/Tiles/Dirt/Set3/7_Dirt_Set3_LB.7_Dirt_Set3_LB")},
		{319, TEXT("/Game/Art/Tiles/Dirt/Set3/8_Dirt_Set3_MB.8_Dirt_Set3_MB")},
		{281, TEXT("/Game/Art/Tiles/Dirt/Set3/9_Dirt_Set3_RB.9_Dirt_Set3_RB")},
		{6, TEXT("/Game/Art/Tiles/Dirt/Set4/1_Dirt_Set4_LT.1_Dirt_Set4_LT")},
		{266, TEXT("/Game/Art/Tiles/Dirt/Set4/2_Dirt_Set4_MT.2_Dirt_Set4_MT")},
		{12, TEXT("/Game/Art/Tiles/Dirt/Set4/3_Dirt_Set4_RT.3_Dirt_Set4_RT")},
		{261, TEXT("/Game/Art/Tiles/Dirt/Set4/4_Dirt_Set4_LM.4_Dirt_Set4_LM")},
		{0, TEXT("/Game/Art/Tiles/Dirt/Set4/5_Dirt_Set4_MM.5_Dirt_Set4_MM")},
		{261, TEXT("/Game/Art/Tiles/Dirt/Set4/6_Dirt_Set4_RM.6_Dirt_Set4_RM")},
		{3, TEXT("/Game/Art/Tiles/Dirt/Set4/7_Dirt_Set4_LB.7_Dirt_Set4_LB")},
		{266, TEXT("/Game/Art/Tiles/Dirt/Set4/8_Dirt_Set4_MB.8_Dirt_Set4_MB")},
		{9, TEXT("/Game/Art/Tiles/Dirt/Set4/9_Dirt_Set4_RB.9_Dirt_Set4_RB")},
		{262, TEXT("/Game/Art/Tiles/Dirt/Set5/1_Dirt_Set5_LT.1_Dirt_Set5_LT")},
		{270, TEXT("/Game/Art/Tiles/Dirt/Set5/2_Dirt_Set5_MT.2_Dirt_Set5_MT")},
		{268, TEXT("/Game/Art/Tiles/Dirt/Set5/3_Dirt_Set5_RT.3_Dirt_Set5_RT")},
		{263, TEXT("/Game/Art/Tiles/Dirt/Set5/4_Dirt_Set5_LM.4_Dirt_Set5_LM")},
		{271, TEXT("/Game/Art/Tiles/Dirt/Set5/5_Dirt_Set5_MM.5_Dirt_Set5_MM")},
		{269, TEXT("/Game/Art/Tiles/Dirt/Set5/6_Dirt_Set5_RM.6_Dirt_Set5_RM")},
		{259, TEXT("/Game/Art/Tiles/Dirt/Set5/7_Dirt_Set5_LB.7_Dirt_Set5_LB")},
		{267, TEXT("/Game/Art/Tiles/Dirt/Set5/8_Dirt_Set5_MB.8_Dirt_Set5_MB")},
		{265, TEXT("/Game/Art/Tiles/Dirt/Set5/9_Dirt_Set5_RB.9_Dirt_Set5_RB")},
		{258, TEXT("/Game/Art/Tiles/Dirt/Set6/Set_H/1_Dirt_Set6_LH.1_Dirt_Set6_LH")},
		{266, TEXT("/Game/Art/Tiles/Dirt/Set6/Set_H/2_Dirt_Set6_stH.2_Dirt_Set6_stH")},
		{264, TEXT("/Game/Art/Tiles/Dirt/Set6/Set_H/3_Dirt_Set6_RH.3_Dirt_Set6_RH")},
		{260, TEXT("/Game/Art/Tiles/Dirt/Set6/Set_V/1_Dirt_Set6_TV.1_Dirt_Set6_TV")},
		{261, TEXT("/Game/Art/Tiles/Dirt/Set6/Set_V/2_Dirt_Set6_stV.2_Dirt_Set6_stV")},
		{257, TEXT("/Game/Art/Tiles/Dirt/Set6/Set_V/3_Dirt_Set6_BV.3_Dirt_Set6_BV")}
	};

	template <typename GetMask, typename GetWeight>
	uint8 SelectMatchingVariant(const int32 Count, const uint16 Mask, const uint32 Seed,
		GetMask&& MaskAt, GetWeight&& WeightAt)
	{
		int32 TotalWeight = 0;
		for (int32 Index = 0; Index < Count; ++Index)
			if (MaskAt(Index) == Mask) TotalWeight += FMath::Max(1, WeightAt(Index));
		if (TotalWeight == 0) return PSDirtAutoTile::NoVariant;

		int32 Roll = static_cast<int32>(Seed % static_cast<uint32>(TotalWeight));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (MaskAt(Index) != Mask) continue;
			const int32 Weight = FMath::Max(1, WeightAt(Index));
			if (Roll < Weight) return static_cast<uint8>(Index + 1);
			Roll -= Weight;
		}
		return PSDirtAutoTile::NoVariant;
	}

	template <typename SelectExact>
	uint8 SelectWithFallback(const uint16 Mask, SelectExact&& Exact)
	{
		if (const uint8 ExactVariant = Exact(Mask)) return ExactVariant;
		if ((Mask & PSDirtAutoTile::Center) != 0)
		{
			// Set5 and Set6 provide every cardinal connection. Ignoring unsupported diagonals keeps seams closed.
			return Exact(PSDirtAutoTile::Center | (Mask & PSDirtAutoTile::CardinalMask));
		}
		// Unsupported grass-side combinations safely fall back to the plain grass tile (mask zero).
		return Exact(0);
	}
}

uint8 UPSDirtAutoTileSet::SelectVariant(const uint16 Mask, const uint32 Seed) const
{
	if (Rules.IsEmpty()) return PSDirtAutoTile::SelectDefaultVariant(Mask, Seed);
	return SelectWithFallback(Mask, [this, Seed](const uint16 Candidate)
	{
		return SelectMatchingVariant(FMath::Min(Rules.Num(), static_cast<int32>(MAX_uint8)), Candidate, Seed,
			[this](const int32 Index)
			{
				return static_cast<uint16>(Rules[Index].NeighborMask)
					| (Rules[Index].bCenterIsDirt ? PSDirtAutoTile::Center : 0);
			},
			[this](const int32 Index) { return Rules[Index].Weight; });
	});
}

UTexture2D* UPSDirtAutoTileSet::GetVariantTexture(const uint8 Variant) const
{
	return Variant > 0 && Rules.IsValidIndex(Variant - 1) ? Rules[Variant - 1].Texture : nullptr;
}

uint8 PSDirtAutoTile::SelectDefaultVariant(const uint16 Mask, const uint32 Seed)
{
	return SelectWithFallback(Mask, [Seed](const uint16 Candidate)
	{
		return SelectMatchingVariant(UE_ARRAY_COUNT(DefaultRules), Candidate, Seed,
			[](const int32 Index) { return DefaultRules[Index].Mask; },
			[](const int32) { return 1; });
	});
}

uint16 PSDirtAutoTile::GetDefaultVariantMask(const uint8 Variant)
{
	return Variant > 0 && Variant <= UE_ARRAY_COUNT(DefaultRules) ? DefaultRules[Variant - 1].Mask : 0;
}

const TCHAR* PSDirtAutoTile::GetDefaultVariantTexturePath(const uint8 Variant)
{
	return Variant > 0 && Variant <= UE_ARRAY_COUNT(DefaultRules) ? DefaultRules[Variant - 1].TexturePath : nullptr;
}
