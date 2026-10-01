// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PSDirtAutoTileSet.generated.h"

class UTexture2D;

/** Dirt occupancy in the eight cells around one rendered cell. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EPSDirtConnection : uint8
{
	None = 0 UMETA(Hidden),
	North = 1 << 0,
	East = 1 << 1,
	South = 1 << 2,
	West = 1 << 3,
	NorthWest = 1 << 4,
	NorthEast = 1 << 5,
	SouthEast = 1 << 6,
	SouthWest = 1 << 7
};
ENUM_CLASS_FLAGS(EPSDirtConnection);

USTRUCT(BlueprintType)
struct FPSDirtAutoTileRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCenterIsDirt = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Bitmask, BitmaskEnum = "/Script/PeaceSign.EPSDirtConnection"))
	int32 NeighborMask = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Texture;

	/** Rules with the same mask are deterministic visual variations selected by this weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 Weight = 1;
};

/** Optional designer override. When unset or empty, the imported /Game/Art/Tiles/Dirt set is used. */
UCLASS(BlueprintType)
class PEACESIGN_API UPSDirtAutoTileSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dirt Auto Tile")
	TArray<FPSDirtAutoTileRule> Rules;

	uint8 SelectVariant(uint16 Mask, uint32 Seed) const;
	UTexture2D* GetVariantTexture(uint8 Variant) const;
};

namespace PSDirtAutoTile
{
	constexpr uint16 North = static_cast<uint16>(EPSDirtConnection::North);
	constexpr uint16 East = static_cast<uint16>(EPSDirtConnection::East);
	constexpr uint16 South = static_cast<uint16>(EPSDirtConnection::South);
	constexpr uint16 West = static_cast<uint16>(EPSDirtConnection::West);
	constexpr uint16 NorthWest = static_cast<uint16>(EPSDirtConnection::NorthWest);
	constexpr uint16 NorthEast = static_cast<uint16>(EPSDirtConnection::NorthEast);
	constexpr uint16 SouthEast = static_cast<uint16>(EPSDirtConnection::SouthEast);
	constexpr uint16 SouthWest = static_cast<uint16>(EPSDirtConnection::SouthWest);
	constexpr uint16 Center = 1 << 8;
	constexpr uint16 CardinalMask = North | East | South | West;
	constexpr uint8 NoVariant = 0;

	PEACESIGN_API uint8 SelectDefaultVariant(uint16 Mask, uint32 Seed);
	PEACESIGN_API uint16 GetDefaultVariantMask(uint8 Variant);
	PEACESIGN_API const TCHAR* GetDefaultVariantTexturePath(uint8 Variant);
}
