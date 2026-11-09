#include "PSBushInteractionComponent.h"
#include "PSBushWorldComponent.h"
#include "PSGatheringActionComponent.h"
#include "PSGatheringWorldComponent.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "../World/PSGridWorld.h"
#include "../UI/PSBushPromptWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	constexpr float HarvestDuration = 0.8f;
}

UPSBushInteractionComponent::UPSBushInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

APSPlayerCharacter* UPSBushInteractionComponent::GetUsablePlayer() const
{
	const auto* Controller = Cast<APSPlayerController>(GetOwner());
	auto* Player = Controller ? Cast<APSPlayerCharacter>(Controller->GetPawn()) : nullptr;
	if (!IsValid(Player) || !Player->StatsComponent || Player->StatsComponent->Health <= 0
		|| Player->IsRolling() || Player->IsMovementLocked() || Controller->IsPaused() || Controller->IsFishing()
		|| Controller->bInventoryOpen || Controller->bJournalOpen || Controller->IsFarmActionActive()
		|| Controller->IsFarmCharging() || Controller->GatheringAction->IsGatheringActive()
		|| UWidgetBlueprintLibrary::IsDragDropping()) return nullptr;
	return Player;
}

bool UPSBushInteractionComponent::Press(APSGridWorld* Grid, const bool bRightClick, TOptional<FIntPoint> Cell)
{
	if (!IsValid(Grid)) return false;
	if (bActive) return true;
	auto* Player = GetUsablePlayer();
	if (!bRightClick) Cell = Grid->Bushes->FindNearest(Player);
	if (!Cell.IsSet() || !Grid->Bushes->HasBush(Cell.GetValue())) return false;
	ActionGrid = Grid;
	if (!Player || !Grid->Gathering->IsUnlocked() || !Grid->Bushes->CanReach(Player, Cell.GetValue())) return true;
	const auto* Bush = Grid->Bushes->Find(Cell.GetValue());
	if (!Bush || Grid->Gathering->Now() < Bush->ReadyAt) return true;
	ActionPlayer = Player;
	Target = Cell.GetValue();
	StartPosition = Player->GetActorLocation();
	StartHealth = Player->StatsComponent->Health;
	StartSlot = CastChecked<APSPlayerController>(GetOwner())->GetSelectedHotbarSlot();
	StartSequence = Bush->HarvestSequence;
	Elapsed = 0;
	bUsingRightClick = bRightClick;
	bActive = true;
	Player->StatsComponent->SetActionActive(true);
	return true;
}

void UPSBushInteractionComponent::Release(const bool bRightClick)
{
	if (bActive && bUsingRightClick == bRightClick) Cancel();
}

void UPSBushInteractionComponent::Cancel()
{
	if (auto* Grid = ActionGrid.Get(); bActive && Grid)
	{
		Grid->Bushes->SetHarvestShake(Target, 0);
	}
	if (auto* Player = ActionPlayer.Get()) Player->StatsComponent->SetActionActive(false);
	bActive = false;
	Elapsed = 0;
	ActionPlayer.Reset();
}

void UPSBushInteractionComponent::Advance(const float Delta)
{
	if (!bActive) return;
	auto* Player = ActionPlayer.Get();
	auto* Grid = ActionGrid.Get();
	auto* Controller = CastChecked<APSPlayerController>(GetOwner());
	const auto* Bush = Grid ? Grid->Bushes->Find(Target) : nullptr;
	if (!Player || GetUsablePlayer() != Player || !Grid || !Bush || Bush->HarvestSequence != StartSequence
		|| !Player->GetActorLocation().Equals(StartPosition, 0.1f) || Player->StatsComponent->Health < StartHealth
		|| Controller->GetSelectedHotbarSlot() != StartSlot || !Grid->Bushes->CanReach(Player, Target))
	{
		Cancel();
		return;
	}
	if (!FMath::IsFinite(Delta) || Delta <= 0) return;
	Elapsed += Delta;
	if (Elapsed < HarvestDuration)
	{
		Grid->Bushes->SetHarvestShake(Target, PSGathering::GetShakeAngle(Elapsed / HarvestDuration));
		return;
	}
	// Finish the input session before inventory delegates run. A held key never repeats.
	Cancel();
	const bool bSuccess = Grid->Bushes->Harvest(Player, Target, Controller->GetInventoryComponent());
	if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Green,
		bSuccess ? TEXT("덤불에서 열매를 채집했습니다.") : TEXT("지금은 채집할 수 없습니다."));
}

void UPSBushInteractionComponent::UpdatePrompt()
{
	auto* Controller = CastChecked<APSPlayerController>(GetOwner());
	auto* Player = GetUsablePlayer();
	auto* Grid = Controller->GridWorld.Get();
	const auto Crop = bActive ? TOptional<FIntPoint>() : Controller->FindHarvestableCrop();
	const auto Cell = Grid && Player ? (bActive ? TOptional<FIntPoint>(Target) : Grid->Bushes->FindNearest(Player)) : TOptional<FIntPoint>();
	if (!Controller->IsLocalController() || (!Cell.IsSet() && !Crop.IsSet()))
	{
		if (Prompt) Prompt->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (!Prompt)
	{
		Prompt = CreateWidget<UPSBushPromptWidget>(Controller);
		if (!Prompt) return;
		Prompt->AddToViewport(30);
		Prompt->SetDesiredSizeInViewport(FVector2D(330, 70));
		Prompt->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	}
	int32 Width, Height;
	Controller->GetViewportSize(Width, Height);
	Prompt->SetPositionInViewport(FVector2D(Width * 0.5f, Height * 0.8f));
	if (Crop.IsSet())
	{
		Prompt->ShowPrompt(TEXT("F 0.8초 누르기 · 작물 수확"),
			FMath::Clamp(Controller->CropHarvestElapsed / APSPlayerController::CropHarvestDuration, 0.f, 1.f));
		return;
	}
	const auto* Bush = Grid->Bushes->Find(Cell.GetValue());
	const int64 Remaining = Bush ? Bush->ReadyAt - Grid->Gathering->Now() : 0;
	const FString Message = Remaining > 0
		? FString::Printf(TEXT("열매가 다시 열리기까지 %lld시간"), (Remaining + 1) / 2)
		: TEXT("F / 우클릭 0.8초 누르기 · 덤불 채집");
	Prompt->ShowPrompt(Message, bActive ? FMath::Clamp(Elapsed / HarvestDuration, 0.f, 1.f) : 0.f);
}

void UPSBushInteractionComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(Delta, Type, Tick);
	Advance(Delta);
	UpdatePrompt();
}

void UPSBushInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Cancel();
	if (Prompt) Prompt->RemoveFromParent();
	Super::EndPlay(Reason);
}
