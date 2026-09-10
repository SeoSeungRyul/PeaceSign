// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PSTileTypes.h"
#include "PSWorldSaveGame.generated.h"

UCLASS()
class PEACESIGN_API UPSWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 WorldSeed = 1337;

	// Reference for restoring crop ages when a new session's clock starts at 06:00.
	UPROPERTY(SaveGame)
	int64 GrowthClockHalfHour = -1;

	UPROPERTY(SaveGame)
	TArray<FPSChunkSaveData> ModifiedChunks;
};
