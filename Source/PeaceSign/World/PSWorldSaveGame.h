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
	int32 DataVersion = 0;

	UPROPERTY(SaveGame)
	int32 WorldSeed = 1337;

	UPROPERTY(SaveGame) int32 MiningGenerationVersion = 0;
	UPROPERTY(SaveGame) int32 SavedChunkSize = 0;
	UPROPERTY(SaveGame) int32 SavedWorldHalfExtent = 0;
	UPROPERTY(SaveGame) float SavedCellSize = 0.0f;
	UPROPERTY(SaveGame) FPSOreGenerationSettings OreSettings;
	UPROPERTY(SaveGame) double MinimumMiningDistance = 0.0;
	UPROPERTY(SaveGame) double MaximumMiningDistance = 0.0;
	UPROPERTY(SaveGame) TArray<FPSTileStamp> SavedTileStamps;
	UPROPERTY(SaveGame) int32 SavedPlainStoneWeight = 70;
	UPROPERTY(SaveGame) TArray<FPSMineralSpawnRule> SavedMineralRules;

	// Reference for restoring crop ages when a new session's clock starts at 06:00.
	UPROPERTY(SaveGame)
	int64 GrowthClockHalfHour = -1;

	UPROPERTY(SaveGame)
	int64 SavedClockHalfHour = -1;

	UPROPERTY(SaveGame)
	double SavedClockSecondsIntoStep = 0.0;

	UPROPERTY(SaveGame)
	TArray<FPSChunkSaveData> ModifiedChunks;
};
