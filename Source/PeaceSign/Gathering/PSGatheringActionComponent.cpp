#include "PSGatheringActionComponent.h"
#include "PSGatheringWorldComponent.h"
#include "../World/PSGridWorld.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "../Inventory/PSInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

UPSGatheringActionComponent::UPSGatheringActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPSGatheringActionComponent::Feedback(const FString& Message) const
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Yellow, Message);
}

bool UPSGatheringActionComponent::HandleTree(APSGridWorld* Grid, const FIntPoint Cell)
{
	if (!IsValid(Grid) || !Grid->Gathering->OccupiesTreeCell(Cell)) return false;
	if (bActive || !Grid->Gathering->HasTree(Cell)) return true;
	auto* Controller = Cast<APSPlayerController>(GetOwner());
	auto* Pawn = Controller ? Cast<APSPlayerCharacter>(Controller->GetPawn()) : nullptr;
	if (!Pawn || !Pawn->StatsComponent || Pawn->StatsComponent->Health <= 0 || Controller->IsPaused()
		|| Controller->IsFishing() || Controller->IsFarmActionActive() || Controller->IsFarmCharging()
		|| Pawn->IsRolling() || Pawn->IsMovementLocked() || UWidgetBlueprintLibrary::IsDragDropping()) return true;
	if (!Grid->Gathering->IsUnlocked()) { Feedback(TEXT("채집이 해금되지 않았습니다.")); return true; }
	if (!Grid->Gathering->CanReach(Grid->WorldToCell(Pawn->GetActorLocation()), Cell, Pawn))
		{ Feedback(TEXT("나무에 더 가까이 다가가세요.")); return true; }
	const auto Tree = Grid->Gathering->GetTree(Cell);
	Slot = Controller->GetSelectedHotbarSlot();
	Tool = Controller->GetInventoryComponent()->GetHotbarSlot(Slot);
	bChopping = !Tool.IsEmpty() && Tool.ItemType == EPSItemType::Axe;
	if (Tree.IsSapling() && !bChopping) { Feedback(TEXT("묘목이 자라는 중입니다.")); return true; }
	FString DataError;
	if (!Grid->Gathering->ValidateRewardData(Cell, bChopping, DataError))
	{ Feedback(TEXT("채집 데이터 설정을 확인하세요: ") + DataError); return true; }
	Axe = Grid->Gathering->AxeDefinition(Tool.ItemId);
	if (bChopping && Tool.CurrentDurability <= 0) { Feedback(TEXT("도끼의 내구도가 부족합니다.")); return true; }
	if (bChopping && Pawn->StatsComponent->Stamina < Axe.StaminaCost) { Feedback(TEXT("SP가 부족합니다.")); return true; }
	ActionGrid = Grid;
	ActionPawn = Pawn;
	Target = Cell;
	ExpectedHealth = Tree.Health;
	Elapsed = 0;
	bActive = true;
	bCommitted = false;
	bSuccess = false;
	Pawn->SetMovementLocked(true);
	Pawn->StatsComponent->SetActionActive(true);
	OnStarted.Broadcast(bChopping, Target, bChopping ? 0.6f : 0.8f);
	return true;
}

bool UPSGatheringActionComponent::IsContextValid() const
{
	const auto* Controller = Cast<APSPlayerController>(GetOwner());
	const auto* Pawn = ActionPawn.Get();
	const auto* Grid = ActionGrid.Get();
	if (!Controller || !IsValid(Pawn) || Controller->GetPawn() != Pawn || !IsValid(Grid)
		|| Pawn->StatsComponent->Health <= 0 || !Pawn->IsMovementLocked() || Controller->IsFishing()
		|| !Grid->Gathering->IsUnlocked()) return false;
	if (bCommitted) return true; // The hit may have removed the tree or broken the axe.
	if (!Grid->Gathering->CanReach(Grid->WorldToCell(Pawn->GetActorLocation()), Target, Pawn)) return false;
	const auto Current = Controller->GetInventoryComponent()->GetHotbarSlot(Slot);
	return Controller->GetSelectedHotbarSlot() == Slot && Current.ItemType == Tool.ItemType
		&& Current.ItemId == Tool.ItemId && Current.Quantity == Tool.Quantity && Current.CurrentDurability == Tool.CurrentDurability
		&& Grid->Gathering->GetTree(Target).Health == ExpectedHealth;
}

void UPSGatheringActionComponent::Advance(const float DeltaSeconds)
{
	if (!bActive) return;
	if (!IsContextValid()) { Cancel(); return; }
	const auto* Controller = CastChecked<APSPlayerController>(GetOwner());
	if (Controller->IsPaused() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	Elapsed += DeltaSeconds;
	const float Impact = bChopping ? 0.3f : 0.4f;
	if (!bCommitted && Elapsed >= Impact)
	{
		auto* Pawn = ActionPawn.Get();
		auto* World = ActionGrid->Gathering.Get();
		const float StaminaBefore = Pawn->StatsComponent->Stamina;
		if (bChopping && !Pawn->StatsComponent->TryConsumeStamina(Axe.StaminaCost)) { Cancel(); Feedback(TEXT("SP가 부족합니다.")); return; }
		bCommitted = true;
		bool bHadReward = false;
		bSuccess = bChopping ? World->Strike(Target, Axe, Pawn) : World->Shake(Target, &bHadReward, Pawn);
		if (bChopping && !bSuccess)
		{
			Pawn->StatsComponent->Stamina = StaminaBefore;
			Pawn->StatsComponent->OnStatsChanged.Broadcast();
			Feedback(TEXT("벌목 대상을 사용할 수 없거나 드롭 데이터가 잘못되었습니다."));
		}
		if (bChopping && bSuccess)
		{
			bool bDestroyed = false;
			Controller->GetInventoryComponent()->ConsumeHotbarDurability(Slot, 1, bDestroyed);
			const int32 HP = World->GetTree(Target).Health;
			Feedback(HP > 0 ? FString::Printf(TEXT("나무 HP %d"), HP) : TEXT("벌목을 완료했습니다."));
		}
		else if (!bChopping)
		{
			const int64 Remaining = FMath::Max<int64>(0, World->GetTree(Target).NextShakeHalfHour - World->Now());
			Feedback(bSuccess ? (bHadReward ? TEXT("아이템이 떨어졌습니다.") : TEXT("아무것도 나오지 않았습니다."))
				: Remaining > 0 ? FString::Printf(TEXT("다시 흔들기까지 %lld일 %lld시간"), Remaining / 48, (Remaining % 48 + 1) / 2)
				: TEXT("흔들기 드롭 데이터를 확인하세요."));
		}
	}
	if (Elapsed >= (bChopping ? 0.6f : 0.8f)) Cancel();
}

void UPSGatheringActionComponent::Cancel()
{
	if (!bActive) return;
	const bool bFinishedSuccessfully = bCommitted && bSuccess;
	bActive = false;
	if (auto* Pawn = ActionPawn.Get())
	{
		Pawn->SetMovementLocked(false);
		if (Pawn->StatsComponent) Pawn->StatsComponent->SetActionActive(false);
	}
	ActionPawn.Reset();
	ActionGrid.Reset();
	OnFinished.Broadcast(bFinishedSuccessfully);
}

void UPSGatheringActionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Advance(DeltaTime);
}

void UPSGatheringActionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Cancel();
	Super::EndPlay(Reason);
}
