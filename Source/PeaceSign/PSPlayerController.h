// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PSEquipmentTypes.h"
#include "PSPlayerController.generated.h"

class UPSPlayerStatusWidget;
class UInputAction;
class UInputMappingContext;
class APSGridWorld;

/** Owns local-player input modes and mapping contexts. */
UCLASS()
class PEACESIGN_API APSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APSPlayerController();
	UFUNCTION(BlueprintPure, Category = "Equipment")
	EPSEquipment GetEquipment() const { return Equipment; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSPlayerStatusWidget> PlayerStatusWidgetClass;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SpecialAttackAction;

	UFUNCTION(BlueprintImplementableEvent, Category = "Input")
	void OnInteractRequested();

	UFUNCTION(BlueprintImplementableEvent, Category = "Input")
	void OnSpecialAttackRequested();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> QuestAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AbilityAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CraftAction;

	UFUNCTION(BlueprintImplementableEvent, Category = "Input|Menu")
	void OnInventoryRequested();

	UFUNCTION(BlueprintImplementableEvent, Category = "Input|Menu")
	void OnQuestRequested();

	UFUNCTION(BlueprintImplementableEvent, Category = "Input|Menu")
	void OnAbilityRequested();

	UFUNCTION(BlueprintImplementableEvent, Category = "Input|Menu")
	void OnCraftRequested();

private:
	void EquipBareHands();
	void EquipHoe();
	void EquipSeed();
	void SetEquipment(EPSEquipment InEquipment);
	UPROPERTY(Transient)
	EPSEquipment Equipment = EPSEquipment::BareHands;
	void UpdateStatusWidget();
	UPROPERTY(Transient) TObjectPtr<UPSPlayerStatusWidget> StatusWidget;
	TWeakObjectPtr<APawn> StatusPawn;
	void HandleInteract();
	void HandleSpecialAttack();
	TOptional<FIntPoint> GetCursorCell() const;
	void HandleInventory();
	void HandleQuest();
	void HandleAbility();
	void HandleCraft();
	void HandleResetWorld();

	UPROPERTY(Transient)
	TObjectPtr<APSGridWorld> GridWorld;

	TOptional<FIntPoint> HoveredCell;
};
