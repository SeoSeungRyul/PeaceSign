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
	FishingRod
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

	bool IsEmpty() const { return ItemType == EPSItemType::None || Quantity <= 0; }
	void Clear() { ItemType = EPSItemType::None; CropId = INDEX_NONE; Quantity = 0; }
};

struct FPSItemDefinition
{
	FText Name;
	FText Description;
	int32 MaxStack = 1;
	FLinearColor Color = FLinearColor::White;
	int32 PlaceholderIcon = 0;
};

namespace PSItems
{
	PEACESIGN_API const FPSItemDefinition& GetDefinition(EPSItemType ItemType);
	PEACESIGN_API bool IsValid(EPSItemType ItemType);
}
