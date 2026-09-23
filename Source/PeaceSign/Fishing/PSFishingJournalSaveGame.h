#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PSFishingTypes.h"
#include "PSFishingJournalSaveGame.generated.h"

UCLASS()
class PEACESIGN_API UPSFishingJournalSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame)
	int32 DataVersion = 1;

	UPROPERTY(SaveGame)
	TMap<FName, FPSFishJournalRecord> Records;
};
