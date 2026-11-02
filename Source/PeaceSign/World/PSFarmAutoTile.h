#pragma once
#include "CoreMinimal.h"

namespace PSFarmAutoTile
{
	constexpr uint8 VariantCount = 23;
	PEACESIGN_API const TCHAR* GetTexturePath(uint8 Variant);
	PEACESIGN_API uint16 GetMask(uint8 Variant);
	PEACESIGN_API uint16 NormalizeMask(uint16 Mask);
	PEACESIGN_API uint8 SelectExact(uint16 Mask);
	/** TL, TR, BL, BR: choose artwork with the same two edges and inner corner. */
	PEACESIGN_API uint8 SelectCorner(uint16 Mask, uint8 Corner);
}
