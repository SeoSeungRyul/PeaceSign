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

	UPROPERTY(SaveGame)
	TArray<FPSChunkSaveData> ModifiedChunks;
};
