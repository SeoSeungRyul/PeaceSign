// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PSEquipmentTypes.h"
#include "PSPlayerController.generated.h"

class UPSPlayerStatusWidget;
class UPSGameTimeWidget;
class UPSInventoryWidget;
class UPSHotbarWidget;
class UPSInventoryComponent;
class UInputAction;
class UInputMappingContext;
class APSGridWorld;
struct FInputActionValue;
enum class EPSTileInteractionResult : uint8;

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
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UPSInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	UFUNCTION(BlueprintCallable, Category = "Inventory|Hotbar")
	void SelectHotbarSlot(int32 SlotIndex);
	UFUNCTION(BlueprintPure, Category = "Inventory|Hotbar")
	int32 GetSelectedHotbarSlot() const { return SelectedHotbarSlot; }

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
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSHotbarWidget> HotbarWidgetClass;
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
	friend class FPSFarmingInventoryTest;
	friend class FPSGameplayInputMappingTest;
	void HandleFishingRod();
	void UpdateFishing();
	void StopFishing();
	TOptional<FIntPoint> FishingCell;
	TWeakObjectPtr<APawn> FishingPawn;
	FVector FishingStartLocation = FVector::ZeroVector;
	void HandleHotbarSlot(const FInputActionValue& Value, int32 SlotIndex);
	UFUNCTION() void RefreshEquipmentFromHotbar();
	EPSTileInteractionResult UseEquippedItemOnCell(FIntPoint Cell);
	void SetEquipment(EPSEquipment InEquipment);
	UPROPERTY(Transient)
	EPSEquipment Equipment = EPSEquipment::BareHands;
	UPROPERTY(Transient)
	int32 HarvestedCropCount = 0;
	void UpdateStatusWidget();
	UPROPERTY(Transient) TObjectPtr<UPSPlayerStatusWidget> StatusWidget;
	UPROPERTY(Transient) TObjectPtr<UPSGameTimeWidget> TimeWidget;
	UPROPERTY(Transient) TObjectPtr<UPSInventoryWidget> InventoryWidget;
	UPROPERTY(Transient) TObjectPtr<UPSHotbarWidget> HotbarWidget;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPSInventoryComponent> InventoryComponent;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> HotbarMappingContext;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> HotbarSlotActions;
	UPROPERTY(Transient)
	int32 SelectedHotbarSlot = INDEX_NONE;
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
