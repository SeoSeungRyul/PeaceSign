#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PSFishingTypes.generated.h"

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

USTRUCT(BlueprintType)
struct FPSFishDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName = NSLOCTEXT("Fishing", "DefaultFish", "물고기");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="4")) int32 Difficulty = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MinSizeCm = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MaxSizeCm = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0")) float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Season = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Zone = NAME_None;
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
