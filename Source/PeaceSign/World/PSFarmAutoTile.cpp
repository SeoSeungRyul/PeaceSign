#include "PSFarmAutoTile.h"
#include "PSDirtAutoTileSet.h"

namespace
{
	struct FPiece { uint16 Mask; const TCHAR* Path; };
	constexpr FPiece Pieces[] = {
		{326, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_LT.Farm_Set1_LT")},
		{462, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_MT.Farm_Set1_MT")},
		{396, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_RT.Farm_Set1_RT")},
		{359, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_LM.Farm_Set1_LM")},
		{511, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_MM.Farm_Set1_MM")},
		{413, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_RM.Farm_Set1_RM")},
		{291, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_LB.Farm_Set1_LB")},
		{315, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_MB.Farm_Set1_MB")},
		{281, TEXT("/Game/Art/Tiles/Farm/Set1/Farm_Set1_RB.Farm_Set1_RB")},
		{258, TEXT("/Game/Art/Tiles/Farm/Set2/Set2_H/Farm_Set2_LH.Farm_Set2_LH")},
		{266, TEXT("/Game/Art/Tiles/Farm/Set2/Set2_H/Farm_Set2_stH.Farm_Set2_stH")},
		{264, TEXT("/Game/Art/Tiles/Farm/Set2/Set2_H/Farm_Set2_RH.Farm_Set2_RH")},
		{260, TEXT("/Game/Art/Tiles/Farm/Set2/Set2_V/Farm_Set2_TV.Farm_Set2_TV")},
		{261, TEXT("/Game/Art/Tiles/Farm/Set2/Set2_V/Farm_Set2_stV.Farm_Set2_stV")},
		{257, TEXT("/Game/Art/Tiles/Farm/Set2/Set2_V/Farm_Set2_BV.Farm_Set2_BV")},
		{265, TEXT("/Game/Art/Tiles/Farm/Set3/Farm_Set3_T.Farm_Set3_T")},
		{259, TEXT("/Game/Art/Tiles/Farm/Set3/Farm_Set3_L.Farm_Set3_L")},
		{268, TEXT("/Game/Art/Tiles/Farm/Set3/Farm_Set3_R.Farm_Set3_R")},
		{262, TEXT("/Game/Art/Tiles/Farm/Set3/Farm_Set3_B.Farm_Set3_B")},
		{267, TEXT("/Game/Art/Tiles/Farm/Set4/Farm_Set4_T.Farm_Set4_T")},
		{269, TEXT("/Game/Art/Tiles/Farm/Set4/Farm_Set4_L.Farm_Set4_L")},
		{263, TEXT("/Game/Art/Tiles/Farm/Set4/Farm_Set4_R.Farm_Set4_R")},
		{270, TEXT("/Game/Art/Tiles/Farm/Set4/Farm_Set4_B.Farm_Set4_B")}
	};
	static_assert(UE_ARRAY_COUNT(Pieces) == PSFarmAutoTile::VariantCount);
}
const TCHAR* PSFarmAutoTile::GetTexturePath(uint8 Variant) { return Variant > 0 && Variant <= VariantCount ? Pieces[Variant - 1].Path : nullptr; }
uint16 PSFarmAutoTile::GetMask(uint8 Variant) { return Variant > 0 && Variant <= VariantCount ? Pieces[Variant - 1].Mask : 0; }
uint16 PSFarmAutoTile::NormalizeMask(uint16 Mask)
{
	uint16 Result = Mask & (PSDirtAutoTile::Center | PSDirtAutoTile::CardinalMask);
	const uint16 Edges[] = {9, 3, 12, 6};
	const uint16 Diagonals[] = {16, 32, 128, 64};
	for (int32 I = 0; I < 4; ++I) if ((Mask & Edges[I]) == Edges[I]) Result |= Mask & Diagonals[I];
	return Result;
}
uint8 PSFarmAutoTile::SelectExact(uint16 Mask)
{
	Mask = NormalizeMask(Mask);
	for (uint8 I = 1; I <= VariantCount; ++I) if (GetMask(I) == Mask) return I;
	return 0;
}
uint8 PSFarmAutoTile::SelectCorner(uint16 Mask, uint8 Corner)
{
	if (Corner > 3) return 0;
	Mask = NormalizeMask(Mask);
	const uint16 Bits[] = {25, 35, 140, 70};
	for (uint8 I = 1; I <= VariantCount; ++I) if ((GetMask(I) & Bits[Corner]) == (Mask & Bits[Corner])) return I;
	return 0;
}
