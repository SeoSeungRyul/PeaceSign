#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PSItemTypes.h"
#include "PSInventorySaveGame.generated.h"

UCLASS()
class PEACESIGN_API UPSInventorySaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame)
	int32 DataVersion = 0;

	UPROPERTY(SaveGame)
	TArray<FPSItemStack> HotbarSlots;
	UPROPERTY(SaveGame)
	TArray<FPSItemStack> BagSlots;
	UPROPERTY(SaveGame)
	int32 UnlockedBagSlotCount = 10;
	UPROPERTY(SaveGame)
	bool bPendingUpgradedHoeGrant = false;
	UPROPERTY(SaveGame)
	bool bPendingSecondHoeGrant = false;
	UPROPERTY(SaveGame) bool bPendingAxeGrant = false;

	// Version 0/1 layout: its first ten slots become the hotbar during migration.
	UPROPERTY(SaveGame)
	TArray<FPSItemStack> Slots;
	UPROPERTY(SaveGame)
	int32 UnlockedSlotCount = 10;
};
