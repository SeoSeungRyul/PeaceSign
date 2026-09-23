#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PSFishingTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EPSFishingState : uint8
{
	Idle,
	WaitingForBite,
	BiteWindow,
	Minigame,
	Success,
	Failure
};

UENUM(BlueprintType)
enum class EPSFishingDirection : uint8
{
	Up,
	Left,
	Down,
	Right
};

UENUM(BlueprintType)
enum class EPSFishingLocation : uint8
{
	Sea = 0,
	River = 1,
	Lake = 2
};

USTRUCT(BlueprintType)
struct FPSFishJournalRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 TimesCaught = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 LargestSizeCm = 0;

	bool IsDiscovered() const { return TimesCaught > 0; }
};

USTRUCT(BlueprintType)
struct FPSFishDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name = NSLOCTEXT("Fishing", "DefaultFish", "물고기");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="4")) int32 Difficulty = 1;
	/** 0=봄, 1=여름, 2=가을, 3=겨울. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="3")) int32 Season = 0;
	/** Fishing location IDs. CSV array syntax: "(0,1)". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<int32> Location;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MinSize = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MaxSize = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DescriptionPlus;
	/** Row ID in the Icon DataTable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName IconID = NAME_None;
	/** Optional CSV column. Missing values use equal catch weight 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", DataTableImportOptional)) float Weight = 1.0f;

	bool IsUsable() const
	{
		return !Name.IsEmpty() && Difficulty >= 1 && Difficulty <= 4
			&& Season >= 0 && Season <= 3 && MinSize > 0 && MaxSize >= MinSize;
	}
};

/** Row format for the Icon CSV referenced by FPSFishDefinition::IconID. */
USTRUCT(BlueprintType)
struct FPSIconDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Image;
};

namespace PSFishing
{
	inline FIntPoint GetInputCountRange(const int32 Difficulty)
	{
		switch (FMath::Clamp(Difficulty, 1, 4))
		{
		case 1: return FIntPoint(5, 10);
		case 2: return FIntPoint(11, 20);
		case 3: return FIntPoint(21, 30);
		default: return FIntPoint(31, 40);
		}
	}
}
