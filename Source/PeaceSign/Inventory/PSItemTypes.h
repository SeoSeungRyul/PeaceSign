#pragma once

#include "CoreMinimal.h"
#include "PSItemTypes.generated.h"

UENUM(BlueprintType)
enum class EPSItemType : uint8
{
	None,
	Hoe,
	TestSeed,
	TestCrop,
	Wood,
	Stone,
	Fish,
	FishingRod,
	FishingBait,
	FishingBobber,
	Pickaxe,
	Ore
};

USTRUCT(BlueprintType)
struct FPSItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EPSItemType ItemType = EPSItemType::None;

	/** Crop data-sheet ID for seed/produce stacks. Other item types keep INDEX_NONE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 CropId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Quantity = 0;

	/** Data-row identity for variants such as fishing-rod tiers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName ItemId = NAME_None;

	/** Per-instance durability. INDEX_NONE denotes an item without durability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 CurrentDurability = INDEX_NONE;

	bool IsEmpty() const { return ItemType == EPSItemType::None || Quantity <= 0; }
	void Clear() { ItemType = EPSItemType::None; CropId = INDEX_NONE; Quantity = 0; ItemId = NAME_None; CurrentDurability = INDEX_NONE; }
};

struct FPSItemDefinition
{
	FText Name;
	FText Description;
	int32 MaxStack = 1;
	FLinearColor Color = FLinearColor::White;
	int32 PlaceholderIcon = 0;
	int32 MaxDurability = 0;
	float FishingTimeBonus = 0.0f;
	float ExtraFishChance = 0.0f;
	bool bInfiniteDurability = false;
	int32 MiningPower = 0;
};

namespace PSItemIds
{
	inline const FName StonePickaxe(TEXT("Pickaxe.Stone"));
	inline const FName CopperPickaxe(TEXT("Pickaxe.Copper"));
	inline const FName IronPickaxe(TEXT("Pickaxe.Iron"));
	inline const FName SilverPickaxe(TEXT("Pickaxe.Silver"));
	inline const FName GoldPickaxe(TEXT("Pickaxe.Gold"));
	inline const FName TitaniumPickaxe(TEXT("Pickaxe.Titanium"));
	inline const FName LumistonePickaxe(TEXT("Pickaxe.Lumistone"));
	inline const FName AsteriumPickaxe(TEXT("Pickaxe.Asterium"));
	inline const FName CopperOre(TEXT("Ore.Copper"));
	inline const FName IronOre(TEXT("Ore.Iron"));
	inline const FName SilverOre(TEXT("Ore.Silver"));
	inline const FName GoldOre(TEXT("Ore.Gold"));
	inline const FName TitaniumOre(TEXT("Ore.Titanium"));
	inline const FName LumistoneOre(TEXT("Ore.Lumistone"));
	inline const FName AsteriumOre(TEXT("Ore.Asterium"));
	inline const FName WoodenFishingRod(TEXT("FishingRod.Wood"));
	inline const FName PlasticFishingRod(TEXT("FishingRod.Plastic"));
	inline const FName AluminumFishingRod(TEXT("FishingRod.Aluminum"));
	inline const FName FiberglassFishingRod(TEXT("FishingRod.Fiberglass"));
	inline const FName CarbonFishingRod(TEXT("FishingRod.Carbon"));
	inline const FName AsteriumFishingRod(TEXT("FishingRod.Asterium"));
}

namespace PSItems
{
	inline constexpr int32 DefaultMiningPower = 10;

	PEACESIGN_API const FPSItemDefinition& GetDefinition(EPSItemType ItemType);
	PEACESIGN_API const FPSItemDefinition& GetDefinition(EPSItemType ItemType, FName ItemId);
	PEACESIGN_API const FPSItemDefinition& GetDefinition(const FPSItemStack& Stack);
	PEACESIGN_API FName GetDefaultItemId(EPSItemType ItemType);
	/** Bare hands and non-pickaxe equipment use the design default of 10. */
	PEACESIGN_API int32 GetMiningPower(const FPSItemStack* EquippedStack);
	PEACESIGN_API bool UsesDurability(const FPSItemStack& Stack);
	PEACESIGN_API bool IsValid(EPSItemType ItemType);
}
