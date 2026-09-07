// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PSPlayerController.generated.h"

class UPSPlayerStatusWidget;
class UInputMappingContext;
class APSGridWorld;

/** Owns local-player input modes and mapping contexts. */
UCLASS()
class PEACESIGN_API APSPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSPlayerStatusWidget> PlayerStatusWidgetClass;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

private:
	void UpdateStatusWidget();
	UPROPERTY(Transient) TObjectPtr<UPSPlayerStatusWidget> StatusWidget;
	TWeakObjectPtr<APawn> StatusPawn;
	void HandlePrimaryAction();
	void HandleResetWorld();

	UPROPERTY(Transient)
	TObjectPtr<APSGridWorld> GridWorld;

	TOptional<FIntPoint> HoveredCell;
};
