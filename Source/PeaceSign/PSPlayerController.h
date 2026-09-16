// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PSEquipmentTypes.h"
#include "PSPlayerController.generated.h"

class UPSPlayerStatusWidget;
class UPSGameTimeWidget;
class UPSInventoryWidget;
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
	UFUNCTION(BlueprintCallable, Category="UI|Inventory")
	void CloseInventory();
	UFUNCTION(BlueprintPure, Category = "Equipment")
	EPSEquipment GetEquipment() const { return Equipment; }
	UFUNCTION(BlueprintPure, Category = "Farming")
	int32 GetHarvestedCropCount() const { return HarvestedCropCount; }

	UFUNCTION(BlueprintCallable, Category = "Fishing")
	bool TryUseFishingRod(FIntPoint WaterCell);

	UFUNCTION(BlueprintPure, Category = "Fishing")
	bool IsFishing() const { return FishingCell.IsSet(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSPlayerStatusWidget> PlayerStatusWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSInventoryWidget> InventoryWidgetClass;
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

	UFUNCTION(BlueprintImplementableEvent, Category = "Fishing")
	void OnFishingRodUsed(FIntPoint WaterCell);

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
	friend class FPSFishingTest;
	void EquipFishingRod();
	void HandleFishingRod();
	void UpdateFishing();
	void StopFishing();
	TOptional<FIntPoint> FishingCell;
	TWeakObjectPtr<APawn> FishingPawn;
	FVector FishingStartLocation = FVector::ZeroVector;
	void EquipBareHands();
	void EquipHoe();
	void EquipSeed();
	void SetEquipment(EPSEquipment InEquipment);
	UPROPERTY(Transient)
	EPSEquipment Equipment = EPSEquipment::BareHands;
	UPROPERTY(Transient)
	int32 HarvestedCropCount = 0;
	void UpdateStatusWidget();
	UPROPERTY(Transient) TObjectPtr<UPSPlayerStatusWidget> StatusWidget;
	UPROPERTY(Transient) TObjectPtr<UPSGameTimeWidget> TimeWidget;
	UPROPERTY(Transient) TObjectPtr<UPSInventoryWidget> InventoryWidget;
	bool bInventoryOpen = false;
	bool bInventoryOwnsPause = false;
	TWeakObjectPtr<APawn> StatusPawn;
	void HandleInteract();
	void HandleSpecialAttack();
	TOptional<FIntPoint> GetCursorCell() const;
	void HandleInventory();
	void HandleQuest();
	void HandleAbility();
	void HandleCraft();
	void HandleResetWorld();
	void HandleAdvanceTime();

	UPROPERTY(Transient)
	TObjectPtr<APSGridWorld> GridWorld;

	TOptional<FIntPoint> HoveredCell;
};
