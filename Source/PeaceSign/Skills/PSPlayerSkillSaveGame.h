#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PSPlayerSkillTypes.h"
#include "PSPlayerSkillSaveGame.generated.h"

UCLASS()
class PEACESIGN_API UPSPlayerSkillSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame)
	int32 DataVersion = 1;
	UPROPERTY(SaveGame)
	TMap<EPSPlayerSkillField, FPSPlayerSkillState> Skills;
};
