// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PSEquipmentTypes.h"
#include "Fishing/PSFishingTypes.h"
#include "Inventory/PSItemTypes.h"
#include "PSPlayerController.generated.h"

class UPSPlayerStatusWidget;
class UPSGameTimeWidget;
class UPSInventoryWidget;
class UPSHotbarWidget;
class UPSInventoryComponent;
class UPSPlayerSkillComponent;
class UPSFishingJournalComponent;
class UPSFishingWidget;
class UPSFishingJournalWidget;
class APSFishingBobber;
class USoundBase;
class UDataTable;
class UInputAction;
class UInputMappingContext;
class UTexture2D;
class APSGridWorld;
class APSPlayerCharacter;
class APSFarmTargetMarker;
class UPSGatheringActionComponent;
class UPSBushInteractionComponent;
struct FInputActionValue;
struct FPSItemStack;
enum class EPSTileInteractionResult : uint8;

/** Owns local-player input modes and mapping contexts. */
UCLASS()
class PEACESIGN_API APSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void UpdateCameraManager(float DeltaSeconds) override;
	APSPlayerController();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gathering")
	TObjectPtr<UPSGatheringActionComponent> GatheringAction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gathering")
	TObjectPtr<UPSBushInteractionComponent> BushInteraction;
	virtual void FlushPressedKeys() override;
	UFUNCTION(BlueprintCallable, Category="UI|Inventory")
	void CloseInventory();
	UFUNCTION(BlueprintCallable, Category="Fishing|Journal") void ToggleFishingJournal();
	UFUNCTION(BlueprintCallable, Category="Fishing|Journal") void CloseFishingJournal();
	UFUNCTION(BlueprintCallable, Category="UI|Inventory") void ShowInventoryTab();
	UFUNCTION(BlueprintCallable, Category="UI|Inventory") void HandleInventory();
	UFUNCTION(BlueprintPure, Category="Fishing|Journal") TArray<FName> GetFishIds() const;
	UFUNCTION(BlueprintPure, Category="Fishing|Journal") bool HasMasteredFish(FName FishId) const;
	UTexture2D* GetFishIcon(FName FishId) const;
	UFUNCTION(BlueprintPure, Category = "Equipment")
	EPSEquipment GetEquipment() const { return Equipment; }
	UFUNCTION(BlueprintPure, Category = "Farming")
	int32 GetHarvestedCropCount() const { return HarvestedCropCount; }
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UPSInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	UFUNCTION(BlueprintPure, Category = "Skills")
	UPSPlayerSkillComponent* GetSkillComponent() const { return SkillComponent; }
	UFUNCTION(BlueprintPure, Category = "Fishing|Journal")
	UPSFishingJournalComponent* GetFishingJournalComponent() const { return FishingJournalComponent; }
	UFUNCTION(BlueprintCallable, Category = "Inventory|Hotbar")
	void SelectHotbarSlot(int32 SlotIndex);
	UFUNCTION(BlueprintPure, Category = "Inventory|Hotbar")
	int32 GetSelectedHotbarSlot() const { return SelectedHotbarSlot; }
	UFUNCTION(BlueprintPure, Category="Farming") bool IsFarmActionActive() const { return bFarmActionActive; }
	UFUNCTION(BlueprintPure, Category="Farming") float GetFarmActionTimeRemaining() const { return FarmActionTimeRemaining; }
	UFUNCTION(BlueprintPure, Category="Farming") bool IsFarmCharging() const { return bFarmCharging; }
	UFUNCTION(BlueprintPure, Category="Farming") float GetFarmChargeProgress() const { return FMath::Clamp(FarmChargeElapsed / FarmChargeDuration, 0.0f, 1.0f); }

	UFUNCTION(BlueprintCallable, Category = "Fishing")
	bool TryUseFishingRod(FIntPoint WaterCell);
	/** Only a fresh right-click during CastTiming confirms the throw. */
	UFUNCTION(BlueprintCallable, Category = "Fishing") bool ConfirmFishingCast();
	UFUNCTION(BlueprintPure, Category = "Fishing") float GetFishingCastPosition() const;
	UFUNCTION(BlueprintPure, Category = "Fishing") EPSFishingCastGrade GetFishingCastGrade() const { return FishingCastGrade; }

	UFUNCTION(BlueprintPure, Category = "Fishing")
	bool IsFishing() const { return FishingState != EPSFishingState::Idle; }
	UFUNCTION(BlueprintPure, Category = "Fishing")
	EPSFishingState GetFishingState() const { return FishingState; }
	/** Shared camera/UI anchor after a confirmed cast, until success, failure or cancellation. */
	UFUNCTION(BlueprintPure, Category = "Fishing")
	bool GetFishingFocusLocation(FVector& OutLocation) const;
	UFUNCTION(BlueprintCallable, Category = "Fishing")
	bool SubmitFishingDirection(EPSFishingDirection Direction);
	UFUNCTION(BlueprintPure, Category = "Fishing")
	UTexture2D* GetCurrentFishIcon() const { return CurrentFishIcon; }
	UFUNCTION(BlueprintPure, Category = "Fishing")
	FName GetCurrentFishId() const { return CurrentFishId; }
	/** Waiting/bite states end on movement input. Minigame direction input remains active. */
	bool CancelFishingForMovementInput();
	const FPSFishDefinition* FindFishDefinition(FName FishId) const;
	FText GetItemDisplayName(const FPSItemStack& Item) const;
	FText GetItemDescription(const FPSItemStack& Item) const;
	UTexture2D* GetItemIcon(const FPSItemStack& Item) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnPossess() override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Farming", meta=(ClampMin="0.01"))
	float FarmActionDuration = DefaultFarmActionDuration;
	UFUNCTION(BlueprintImplementableEvent, Category="Farming")
	void OnFarmToolStarted(EPSEquipment Tool, FIntPoint Cell, float Duration);
	UFUNCTION(BlueprintImplementableEvent, Category="Farming")
	void OnFarmToolFinished(EPSEquipment Tool, FIntPoint Cell, bool bSucceeded);
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSPlayerStatusWidget> PlayerStatusWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSInventoryWidget> InventoryWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSHotbarWidget> HotbarWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPSFishingWidget> FishingWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing")
	TObjectPtr<UDataTable> FishDataTable;
	/** Icon rows referenced by FPSFishDefinition::IconID. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing")
	TObjectPtr<UDataTable> FishIconDataTable;
	/** Green, Yellow and Red rows using FPSFishingCastDefinition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Casting") TObjectPtr<UDataTable> FishingCastDataTable;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Casting", meta=(ClampMin="0.01")) float CastWindupDuration = 0.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Casting", meta=(ClampMin="0.01")) float CastTravelSeconds = 0.7f;
	/** Flight of the bobber, independently of the casting gauge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Casting", meta=(ClampMin="0.01")) float CastFlightSeconds = 0.6f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Visual", meta=(ClampMin="0.1")) float FishingResultSeconds = 3.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Casting") FPSFishingCastBands CastBands;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Visual") TSubclassOf<APSFishingBobber> FishingBobberClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Audio") TObjectPtr<USoundBase> FishingCastSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Audio") TObjectPtr<USoundBase> FishingBiteSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Audio") TObjectPtr<USoundBase> FishingSuccessSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Audio") TObjectPtr<USoundBase> FishingFailureSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Audio") TObjectPtr<USoundBase> FishingUnavailableSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Audio") TObjectPtr<USoundBase> FishingRetrieveSound;
	UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UPSFishingJournalWidget> FishingJournalWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing", meta=(ClampMin="0.1"))
	float MinBiteDelay = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing", meta=(ClampMin="0.1"))
	float MaxBiteDelay = 10.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing", meta=(ClampMin="0.1"))
	float BiteWindowDuration = 5.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing", meta=(ClampMin="0.1"))
	float MinigameDuration = 10.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing", meta=(ClampMin="0.0"))
	float WrongInputTimePenalty = 0.5f;
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
	UFUNCTION(BlueprintImplementableEvent, Category = "Fishing") void OnFishingCastStarted();
	UFUNCTION(BlueprintImplementableEvent, Category = "Fishing") void OnFishingUnavailable(const FText& Reason);
	UFUNCTION(BlueprintImplementableEvent, Category = "Fishing") void OnFishingBite();
	UFUNCTION(BlueprintImplementableEvent, Category = "Fishing") void OnFishingMinigameStarted();
	UFUNCTION(BlueprintImplementableEvent, Category = "Fishing") void OnFishingSucceeded(const FText& FishName, int32 SizeCm);
	UFUNCTION(BlueprintImplementableEvent, Category = "Fishing") void OnFishingFailed();
	UFUNCTION(BlueprintImplementableEvent, Category = "Fishing") void OnFishingCancelled();

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
	friend class FPSFarmToolActionTest;
	bool CanHarvestCropHere(FIntPoint Cell) const;
	TOptional<FIntPoint> FindHarvestableCrop() const;
	bool BeginCropHarvest();
	void AdvanceCropHarvest(float DeltaTime);
	void CancelCropHarvest();
	bool CompleteCropHarvest(FIntPoint Cell);
	EPSTileInteractionResult PlantEquippedCrop(FIntPoint Cell);
	void UnlockFarmingFromTilling();
	static constexpr float CropHarvestDuration = 0.8f;
	TOptional<FIntPoint> CropHarvestTarget;
	TWeakObjectPtr<APSGridWorld> CropHarvestGrid;
	TWeakObjectPtr<APSPlayerCharacter> CropHarvestPawn;
	FVector CropHarvestStart = FVector::ZeroVector;
	float CropHarvestHealth = 0;
	float CropHarvestElapsed = 0;
	int32 CropHarvestId = INDEX_NONE;
	double CropHarvestPlanted = -1;
	uint32 CropHarvestSession = 0;
	friend class FPSChargedHoeTest;
	friend class FPSSecondHoeTest;
	bool UsesFarmTool() const;
	static constexpr float DefaultFarmActionDuration = 0.7f;
	static constexpr float FarmChargeDuration = 0.5f;
	static constexpr float FarmToolStaminaCost = 10.0f;
	int32 GetEquippedFarmChargeDepth() const;
	bool IsFarmContextValid() const;
	void CaptureFarmContext();
	bool BeginFarmCharge();
	void HandleSpecialAttackReleased();
	void HandleInteractReleased();
	void HandleSpecialAttackCanceled();
	TArray<FIntPoint> GetFarmTargetCells(bool bCharged) const;
	TOptional<FIntPoint> GetFarmFrontCell() const;
	EPSTileInteractionResult EvaluateFarmTarget(FIntPoint Cell) const;
	EPSTileInteractionResult ApplyFarmTool(TConstArrayView<FIntPoint> Cells);
	bool BeginFarmAction(bool bCharged = false);
	void UpdateFarmAction(float DeltaSeconds);
	void CancelFarmAction();
	void UpdateFarmTargetMarker();
	bool bFarmActionActive = false;
	bool bFarmCharging = false;
	float FarmChargeElapsed = 0;
	bool bCompletingFarmAction = false;
	float FarmActionTimeRemaining = 0;
	TWeakObjectPtr<APawn> FarmActionPawn;
	FIntPoint FarmActionCell = FIntPoint::ZeroValue;
	TArray<FIntPoint> FarmActionCells;
	int32 FarmActionSlot = INDEX_NONE;
	FPSItemStack FarmActionItem;
	EPSEquipment FarmActionEquipment = EPSEquipment::BareHands;
	UPROPERTY(Transient) TObjectPtr<APSFarmTargetMarker> FarmTargetMarker;
	UPROPERTY(Transient) TArray<TObjectPtr<APSFarmTargetMarker>> FarmSideMarkers;
	friend class FPSFishingTest;
	friend class FPSFishingOct26Test;
	friend class FPSBookJournalTest;
	friend class FPSGatheringInventoryTest;
	friend class FPSGatheringTest;
	friend class FPSBushTest;
	friend class UPSBushInteractionComponent;
	friend class FPSWorldResetInventoryTest;
	friend struct FPSBookJournalSmoke;
	friend class FPSFishingFlowTest;
	friend class FPSFarmingInventoryTest;
	friend class FPSMiningTest;
	friend class FPSShovelTest;
	friend class FPSGameplayInputMappingTest;
	void HandleFishingRod();
	bool AreFishingControlsLocked() const;
	void UpdateFishing(float DeltaSeconds = 0.0f);
	void BeginFishingBite();
	void BeginFishingRetrieve();
	void UpdateFishingVisual(float DeltaSeconds);
	void PlayFishingSound(USoundBase* Sound) const;
	void BeginFishingMinigame();
	void CompleteFishing(bool bSuccess);
	void StopFishing(bool bCancelled = true, bool bConsumeDurability = true);
	void ConsumeFishingRodDurability();
	void SetFishingMovementLocked(bool bLocked);
	void RefreshFishingWidget();
	void GenerateFishingSequence();
	FPSFishDefinition SelectFishDefinition(FName& OutFishId, int32 Season, int32 LocationId,
		EPSFishingCastGrade Grade = EPSFishingCastGrade::Green) const;
	FPSFishingCastDefinition GetFishingCastDefinition(EPSFishingCastGrade Grade) const;
	TOptional<FIntPoint> ResolveFishingLandingCell(int32 DistanceInCells) const;
	void ShowFishingUnavailable(const FText& Reason, EPSFishingNoticeType Type = EPSFishingNoticeType::General);
	UTexture2D* ResolveFishIcon(FName IconID) const;
	void DropFishingReward(int32 Quantity = 1);
	void DropStoneReward(FIntPoint Cell);
	void DropItemReward(FIntPoint Cell, const FPSItemStack& Stack);
	void HandleFishingUp();
	void HandleFishingLeft();
	void HandleFishingDown();
	void HandleFishingRight();
	TOptional<FIntPoint> FishingCell;
	TWeakObjectPtr<APawn> FishingPawn;
	FVector FishingStartLocation = FVector::ZeroVector;
	EPSFishingState FishingState = EPSFishingState::Idle;
	float FishingStateTimeRemaining = 0.0f;
	float FishingStateDuration = 0.0f;
	TArray<EPSFishingDirection> FishingSequence;
	int32 FishingSequenceIndex = 0;
	FPSFishDefinition CurrentFish;
	FName CurrentFishId = NAME_None;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentFishIcon;
	mutable TMap<FName, TWeakObjectPtr<UTexture2D>> FishIconCache;
	int32 CurrentFishSizeCm = 0;
	float FishingMinigameDuration = 10.0f;
	float FishingBiteDelay = 0.0f;
	float FishingCastElapsed = 0.0f;
	float FishingCastTravelSeconds = 0.7f;
	FPSFishingCastBands FishingBands;
	float FishingResultElapsed = 0;
	float FishingFlightDuration = .6f;
	float FishingVisualElapsed = 0;
	bool bFishingTerminalAnimation = false;
	bool bFishingSuccessReleased = false;
	bool bJournalOpen = false;
	UPROPERTY(Transient) TObjectPtr<APSFishingBobber> FishingBobber;
	UPROPERTY(Transient) TObjectPtr<UPSFishingJournalWidget> FishingJournalWidget;
	EPSFishingCastGrade FishingCastGrade = EPSFishingCastGrade::Green;
	TArray<FPSFishingCastDefinition> FishingCastDefinitions;
	FPSItemStack FishingRodSnapshot;
	int32 FishingRodSlot = INDEX_NONE;
	int32 FishingSeason = 0;
	FIntPoint FishingAimCell = FIntPoint::ZeroValue;
	FVector FishingCastDirection = FVector::ZeroVector;
	bool bFishingBait = false;
	bool bFishingAutoHook = false;
	bool bFishingDurabilityConsumed = false;
	bool bEndingFishing = false;
	void HandleHotbarSlot(const FInputActionValue& Value, int32 SlotIndex);
	UFUNCTION() void RefreshEquipmentFromHotbar();
	EPSTileInteractionResult UseEquippedItemOnCell(FIntPoint Cell);
	const APSPlayerCharacter* GetTreeSeedPlanter() const;
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
	UPROPERTY(Transient) TObjectPtr<UPSFishingWidget> FishingWidget;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPSInventoryComponent> InventoryComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skills", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPSPlayerSkillComponent> SkillComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fishing|Journal", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPSFishingJournalComponent> FishingJournalComponent;
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
	void HandleQuest();
	void HandleAbility();
	void HandleCraft();
	void HandleResetWorld();
	void HandleAdvanceTime();
	void HandleAdvanceSeason();

	UPROPERTY(Transient)
	TObjectPtr<APSGridWorld> GridWorld;

	TOptional<FIntPoint> HoveredCell;
};
