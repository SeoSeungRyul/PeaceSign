#include "PSPlayerController.h"
#include "PSPlayerCharacter.h"
#include "Fishing/PSFishingBobber.h"
#include "Fishing/PSFishingJournalComponent.h"
#include "UI/PSFishingJournalWidget.h"
#include "UI/PSInventoryWidget.h"
#include "World/PSGridWorld.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/DataTable.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

void APSPlayerController::PlayFishingSound(USoundBase* Sound) const
{
	if (Sound && IsLocalController()) UGameplayStatics::PlaySound2D(this, Sound);
}

void APSPlayerController::BeginFishingRetrieve()
{
	if (FishingState != EPSFishingState::WaitingForBite && FishingState != EPSFishingState::BiteWindow) return;
	ConsumeFishingRodDurability();
	FishingState = EPSFishingState::Retrieving;
	FishingStateDuration = .35f;
	bFishingTerminalAnimation = false;
	if (APSPlayerCharacter* FishingCharacter = Cast<APSPlayerCharacter>(GetPawn()))
	{
		const float Duration = FishingCharacter->PlayFishingStateAnimation(FishingState);
		bFishingTerminalAnimation = Duration > 0;
		if (bFishingTerminalAnimation) FishingStateDuration = Duration;
	}
	FishingStateTimeRemaining = FishingStateDuration;
	PlayFishingSound(FishingRetrieveSound);
	RefreshFishingWidget();
}

void APSPlayerController::UpdateFishingVisual(const float DeltaSeconds)
{
	// Advance through flight and floating. Retrieval freezes the phase so it
	// starts from the current point on the flight path or the water.
	if (FishingState == EPSFishingState::WaitingForBite || FishingState == EPSFishingState::BiteWindow
		|| FishingState == EPSFishingState::Minigame)
	{
		FishingVisualElapsed += FMath::Max(0.f, DeltaSeconds);
	}
	if (!IsValid(FishingBobber)) return;
	if (FishingState == EPSFishingState::Success && GetPawn())
	{
		const FVector ScreenUp = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation().RotateVector(FVector::UpVector) : FVector(1, 0, 0);
		FishingBobber->SetActorLocation(GetPawn()->GetActorLocation() + ScreenUp * 70 + FVector(0, 0, 25));
		return;
	}
	if (!IsValid(GridWorld) || !FishingCell.IsSet() || !GetPawn() || FishingPawn.Get() != GetPawn())
	{
		FishingBobber->Destroy();
		FishingBobber = nullptr;
		return;
	}
	const float RetrieveAlpha = FishingState == EPSFishingState::Retrieving
		? 1 - FMath::Clamp((FishingStateTimeRemaining - DeltaSeconds) / FMath::Max(.01f, FishingStateDuration), 0.f, 1.f) : 0;
	const float WaterElapsed = FMath::Max(0.f, FishingVisualElapsed - FishingFlightDuration);
	FishingBobber->UpdateVisual(GetPawn()->GetActorLocation() + FVector(0, 0, 25),
		GridWorld->WaterVisualLocation(FishingCell.GetValue()) + FVector(0, 0, 8),
		FishingState == EPSFishingState::BiteWindow, WaterElapsed, RetrieveAlpha,
		FishingVisualElapsed / FMath::Max(.01f, FishingFlightDuration),
		PlayerCameraManager ? PlayerCameraManager->GetCameraRotation().RotateVector(FVector::UpVector) : FVector::UpVector);
}

TArray<FName> APSPlayerController::GetFishIds() const
{
	TArray<FName> Ids;
	if (FishDataTable && FishDataTable->GetRowStruct() == FPSFishDefinition::StaticStruct())
	{
		for (const FName Id : FishDataTable->GetRowNames())
			if (const FPSFishDefinition* Fish = FindFishDefinition(Id); Fish && Fish->IsUsable()) Ids.Add(Id);
	}
	Ids.Sort(FNameLexicalLess());
	return Ids;
}

bool APSPlayerController::HasMasteredFish(const FName FishId) const
{
	const FPSFishDefinition* Fish = FindFishDefinition(FishId);
	if (!Fish || !FishingJournalComponent) return false;
	const FPSFishJournalRecord Record = FishingJournalComponent->GetRecord(FishId);
	return Record.IsDiscovered() && (Record.bMastered || Record.LargestSizeCm >= Fish->MaxSize);
}

UTexture2D* APSPlayerController::GetFishIcon(const FName FishId) const
{
	const FPSFishDefinition* Fish = FindFishDefinition(FishId);
	return Fish ? ResolveFishIcon(Fish->IconID) : nullptr;
}

void APSPlayerController::ToggleFishingJournal()
{
	// The journal is a tab of the book opened with I, never a standalone menu.
	if (!bInventoryOpen || bJournalOpen || !InventoryWidget || !IsLocalController()) return;
	if (!FishingJournalWidget)
	{
		UClass* Class = FishingJournalWidgetClass.Get();
		FishingJournalWidget = CreateWidget<UPSFishingJournalWidget>(this, Class ? Class : UPSFishingJournalWidget::StaticClass());
	}
	if (!FishingJournalWidget) return;
	InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	FishingJournalWidget->AddToPlayerScreen(500);
	FishingJournalWidget->RefreshJournal();
	bJournalOpen = true;
	FlushPressedKeys();
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(FishingJournalWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void APSPlayerController::CloseFishingJournal()
{
	if (!bJournalOpen) return;
	CloseInventory();
}

void APSPlayerController::ShowInventoryTab()
{
	if (!bInventoryOpen || !InventoryWidget) return;
	if (FishingJournalWidget) FishingJournalWidget->RemoveFromParent();
	bJournalOpen = false;
	InventoryWidget->SetVisibility(ESlateVisibility::Visible);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	FlushPressedKeys();
}
