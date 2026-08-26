// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PSPlayerController.generated.h"

class UInputMappingContext;
class APSGridWorld;

/** Owns local-player input modes and mapping contexts. */
UCLASS()
class PEACESIGN_API APSPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

private:
	void HandlePrimaryAction();
	void HandleResetWorld();

	UPROPERTY(Transient)
	TObjectPtr<APSGridWorld> GridWorld;

	TOptional<FIntPoint> HoveredCell;
};
