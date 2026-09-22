#pragma once

#include "CoreMinimal.h"
#include "PSPlayerSkillTypes.generated.h"

UENUM(BlueprintType)
enum class EPSPlayerSkillField : uint8
{
	Fishing,
	Farming,
	Mining,
	Combat
};

USTRUCT(BlueprintType)
struct FPSPlayerSkillState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 Level = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 Experience = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 UnspentPoints = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	TMap<FName, int32> LearnedSkills;
};
