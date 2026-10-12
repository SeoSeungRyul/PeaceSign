#pragma once

#include "PSDirtAutoTileSet.h"

/** Water shares dirt's topology, but artwork is rotated +90 degrees around Z
 * so source-image top points toward screen top with the fixed top-down camera.
 * Input masks use the unrotated grid/Plane UV convention.
 */
namespace PSWaterAutoTile
{
	constexpr uint8 VariantCount = 69;
	constexpr float ArtworkYaw = 90.0f;
	PEACESIGN_API uint16 ToTextureMask(uint16 WorldMask);
	/** Remap an authored source slot to retain its footprint when rotating artwork. */
	PEACESIGN_API uint8 GetFixedTextureVariant(uint8 Variant);
	PEACESIGN_API uint8 SelectVariant(uint16 Mask, uint32 Seed);
	/** Source-image occupancy; IDs 1-51 retain the existing shared topology. */
	PEACESIGN_API uint16 GetVariantMask(uint8 Variant);
	PEACESIGN_API const TCHAR* GetVariantTexturePath(uint8 Variant);
}
