#include "PSPlayerController.h"
#include "Gathering/PSGatheringActionComponent.h"
#include "Gathering/PSBushInteractionComponent.h"
#include "Gathering/PSGatheringWorldComponent.h"
#include "PSPlayerCharacter.h"
#include "PSPlayerStatsComponent.h"
#include "Inventory/PSInventoryComponent.h"
#include "World/PSFarmTargetMarker.h"
#include "World/PSGridWorld.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	bool IsFarmSuccess(const EPSTileInteractionResult Result)
	{
		return Result == EPSTileInteractionResult::Tilled || Result == EPSTileInteractionResult::Watered;
	}
	void ShowFarmResult(const EPSTileInteractionResult Result)
	{
		if (!GEngine) return;
		const TCHAR* Message = Result == EPSTileInteractionResult::Tilled ? TEXT("밭을 갈았습니다.")
			: Result == EPSTileInteractionResult::Watered ? TEXT("밭에 물을 주었습니다.")
			: TEXT("이 칸에는 도구를 사용할 수 없습니다.");
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, IsFarmSuccess(Result) ? FColor::Green : FColor::Red, Message);
	}

	void ShowInsufficientFarmStamina()
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Red, TEXT("농사 도구를 사용할 SP가 부족합니다."));
	}
}

bool APSPlayerController::UsesFarmTool() const
{
	return Equipment == EPSEquipment::Hoe || Equipment == EPSEquipment::WateringCan;
}

TOptional<FIntPoint> APSPlayerController::GetFarmFrontCell() const
{
	const APSPlayerCharacter* FarmCharacter = Cast<APSPlayerCharacter>(GetPawn());
	if (!IsValid(GridWorld) || !IsValid(FarmCharacter)) return {};
	return GridWorld->WorldToCell(FarmCharacter->GetActorLocation()) + FarmCharacter->GetFacingCellOffset();
}

EPSTileInteractionResult APSPlayerController::EvaluateFarmTarget(const FIntPoint Cell) const
{
	if (!IsValid(GridWorld)) return EPSTileInteractionResult::InvalidCell;
	const FPSItemStack* Item = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	if (!Item || Item->IsEmpty() || (PSItems::UsesDurability(*Item) && Item->CurrentDurability <= 0))
		return EPSTileInteractionResult::NoEffect;
	if (Equipment == EPSEquipment::WateringCan && Item->ItemType == EPSItemType::WateringCan)
		return GridWorld->CanWaterCell(Cell) ? EPSTileInteractionResult::Watered : EPSTileInteractionResult::NoEffect;
	if (Equipment != EPSEquipment::Hoe || Item->ItemType != EPSItemType::Hoe) return EPSTileInteractionResult::NoEffect;
	return GridWorld->CanTillCell(Cell) ? EPSTileInteractionResult::Tilled : EPSTileInteractionResult::NoEffect;
}

int32 APSPlayerController::GetEquippedFarmChargeDepth() const
{
	const FPSItemStack* Item = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	return UsesFarmTool() && Item && !Item->IsEmpty() ? PSItems::GetDefinition(*Item).FarmChargeDepth : 0;
}

TArray<FIntPoint> APSPlayerController::GetFarmTargetCells(const bool bCharged) const
{
	const auto Front = GetFarmFrontCell();
	if (!Front.IsSet()) return {};
	TArray<FIntPoint> Cells = {Front.GetValue()};
	if (bCharged)
	{
		const auto* Farmer = Cast<APSPlayerCharacter>(GetPawn());
		const FIntPoint Facing = Farmer->GetFacingCellOffset();
		const FIntPoint Side(-Facing.Y, Facing.X);
		const int32 Depth = GetEquippedFarmChargeDepth();
		for (int32 Row = 0; Row < Depth; ++Row)
		{
			const FIntPoint Center = Front.GetValue() + Facing * Row;
			if (Row > 0) Cells.Add(Center);
			Cells.Add(Center + Side);
			Cells.Add(Center - Side);
		}
	}
	return Cells;
}

EPSTileInteractionResult APSPlayerController::ApplyFarmTool(const TConstArrayView<FIntPoint> Cells)
{
	const auto* Farmer = Cast<APSPlayerCharacter>(GetPawn());
	if (!Farmer || Farmer->StatsComponent->Health <= 0) return EPSTileInteractionResult::NoEffect;
	if (Farmer->StatsComponent->Stamina < FarmToolStaminaCost)
	{
		ShowInsufficientFarmStamina();
		return EPSTileInteractionResult::NoEffect;
	}

	EPSTileInteractionResult Result = EPSTileInteractionResult::NoEffect;
	for (const FIntPoint Cell : Cells)
	{
		const auto TargetResult = EvaluateFarmTarget(Cell);
		if (!IsFarmSuccess(TargetResult)) continue;
		const auto Applied = TargetResult == EPSTileInteractionResult::Tilled ? GridWorld->TillCell(Cell) : GridWorld->WaterCell(Cell);
		if (IsFarmSuccess(Applied)) Result = Applied;
	}
	if (IsFarmSuccess(Result))
	{
		const int32 UsedSlot = SelectedHotbarSlot;
		Farmer->StatsComponent->TryConsumeStamina(FarmToolStaminaCost);
		bool bDestroyed = false;
		InventoryComponent->ConsumeHotbarDurability(UsedSlot, 1, bDestroyed);
		if (Result == EPSTileInteractionResult::Tilled) UnlockFarmingFromTilling();
	}
	return Result;
}

void APSPlayerController::CaptureFarmContext()
{
	FarmActionPawn = GetPawn();
	FarmActionEquipment = Equipment;
	FarmActionSlot = SelectedHotbarSlot;
	FarmActionItem = InventoryComponent->GetHotbarSlot(SelectedHotbarSlot);
}

bool APSPlayerController::IsFarmContextValid() const
{
	const auto* Farmer = Cast<APSPlayerCharacter>(FarmActionPawn.Get());
	const FPSItemStack* Current = InventoryComponent ? InventoryComponent->FindHotbarSlot(FarmActionSlot) : nullptr;
	return IsValid(Farmer) && GetPawn() == Farmer && Farmer->StatsComponent->Health > 0 && IsValid(GridWorld)
		&& !bInventoryOpen && !IsFishing() && Equipment == FarmActionEquipment && SelectedHotbarSlot == FarmActionSlot
		&& Current && !Current->IsEmpty() && Current->ItemType == FarmActionItem.ItemType
		&& Current->ItemId == FarmActionItem.ItemId && Current->CropId == FarmActionItem.CropId
		&& Current->Quantity == FarmActionItem.Quantity && Current->CurrentDurability == FarmActionItem.CurrentDurability;
}

bool APSPlayerController::BeginFarmCharge()
{
	auto* Farmer = Cast<APSPlayerCharacter>(GetPawn());
	if (bFarmActionActive || bFarmCharging || bInventoryOpen || IsPaused() || IsFishing() || GetEquippedFarmChargeDepth() <= 0
		|| !IsValid(Farmer) || Farmer->IsMovementLocked() || Farmer->IsRolling()
		|| Farmer->StatsComponent->Health <= 0 || UWidgetBlueprintLibrary::IsDragDropping()) return false;
	if (Farmer->StatsComponent->Stamina < FarmToolStaminaCost)
	{
		ShowInsufficientFarmStamina();
		return false;
	}
	CaptureFarmContext();
	bFarmCharging = true;
	FarmChargeElapsed = 0;
	return true;
}

void APSPlayerController::HandleSpecialAttackReleased()
{
	BushInteraction->Release(true);
	if (!bFarmCharging) return;
	UpdateFarmAction(0); // Validate even if the item/pawn changed since the last tick.
	if (!bFarmCharging) return;
	const bool bCharged = FarmChargeElapsed >= FarmChargeDuration;
	CancelFarmAction();
	if (!IsPaused()) BeginFarmAction(bCharged);
}

void APSPlayerController::HandleSpecialAttackCanceled()
{
	BushInteraction->Release(true);
	if (bFarmCharging) CancelFarmAction();
}

void APSPlayerController::FlushPressedKeys()
{
	CancelCropHarvest();
	BushInteraction->Cancel();
	GatheringAction->Cancel();
	// Losing focus or switching input modes can synthesize a Completed event.
	// Clear the pending charge before that event can be interpreted as a release.
	if (bFarmCharging) CancelFarmAction();
	Super::FlushPressedKeys();
}

bool APSPlayerController::BeginFarmAction(const bool bCharged)
{
	APSPlayerCharacter* FarmCharacter = Cast<APSPlayerCharacter>(GetPawn());
	if (bFarmActionActive || bFarmCharging || bInventoryOpen || IsPaused() || IsFishing() || !UsesFarmTool()
		|| !IsValid(FarmCharacter) || FarmCharacter->IsMovementLocked() || FarmCharacter->IsRolling()
		|| FarmCharacter->StatsComponent->Health <= 0 || UWidgetBlueprintLibrary::IsDragDropping()
		|| (bCharged && GetEquippedFarmChargeDepth() <= 0)) return false;
	if (FarmCharacter->StatsComponent->Stamina < FarmToolStaminaCost)
	{
		ShowInsufficientFarmStamina();
		return false;
	}
	const TArray<FIntPoint> Cells = GetFarmTargetCells(bCharged);
	if (Cells.IsEmpty()) return false;
	if (!Cells.ContainsByPredicate([&](const FIntPoint Cell) { return IsFarmSuccess(EvaluateFarmTarget(Cell)); }))
	{
		ShowFarmResult(EvaluateFarmTarget(Cells[0]));
		return false;
	}
	FarmActionCells = Cells;
	FarmActionCell = Cells[0];
	CaptureFarmContext();
	FarmActionTimeRemaining = FMath::IsFinite(FarmActionDuration) ? FMath::Max(0.01f, FarmActionDuration) : DefaultFarmActionDuration;
	bFarmActionActive = true;
	FarmCharacter->SetMovementLocked(true);
	FarmCharacter->StatsComponent->SetActionActive(true);
	OnFarmToolStarted(Equipment, FarmActionCell, FarmActionTimeRemaining);
	return bFarmActionActive;
}

void APSPlayerController::CancelFarmAction()
{
	if (!bFarmActionActive && !bFarmCharging) return;
	if (APSPlayerCharacter* FarmCharacter = Cast<APSPlayerCharacter>(FarmActionPawn.Get()); FarmCharacter && bFarmActionActive)
	{
		FarmCharacter->SetMovementLocked(false);
		FarmCharacter->StatsComponent->SetActionActive(false);
	}
	bFarmActionActive = false;
	bFarmCharging = false;
	FarmChargeElapsed = 0;
	FarmActionCells.Reset();
	FarmActionTimeRemaining = 0;
	FarmActionPawn.Reset();
}

void APSPlayerController::UpdateFarmAction(const float DeltaSeconds)
{
	if (!bFarmActionActive && !bFarmCharging) return;
	APSPlayerCharacter* FarmCharacter = Cast<APSPlayerCharacter>(FarmActionPawn.Get());
	const auto Front = GetFarmFrontCell();
	if (!IsFarmContextValid() || !Front.IsSet()
		|| (bFarmActionActive && (!FarmCharacter->IsMovementLocked() || Front.GetValue() != FarmActionCell))
		|| (bFarmCharging && (FarmCharacter->IsMovementLocked() || FarmCharacter->IsRolling())))
	{
		CancelFarmAction();
		return;
	}
	if (IsPaused() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	if (bFarmCharging)
	{
		FarmChargeElapsed = FMath::Min(FarmChargeDuration, FarmChargeElapsed + DeltaSeconds);
		return;
	}
	FarmActionTimeRemaining = FMath::Max(0.0f, FarmActionTimeRemaining - DeltaSeconds);
	if (FarmActionTimeRemaining > UE_KINDA_SMALL_NUMBER) return;
	const EPSEquipment Tool = FarmActionEquipment;
	const FIntPoint Cell = FarmActionCell;
	const bool bHadStamina = FarmCharacter->StatsComponent->Stamina >= FarmToolStaminaCost;
	EPSTileInteractionResult Result = EPSTileInteractionResult::NoEffect;
	{
		TGuardValue<bool> Completing(bCompletingFarmAction, true);
		Result = ApplyFarmTool(FarmActionCells);
	}
	CancelFarmAction();
	if (bHadStamina) ShowFarmResult(Result);
	OnFarmToolFinished(Tool, Cell, IsFarmSuccess(Result));
}

void APSPlayerController::UpdateFarmTargetMarker()
{
	const APSPlayerCharacter* FarmCharacter = Cast<APSPlayerCharacter>(GetPawn());
	const bool bTreeSeed = Equipment == EPSEquipment::TreeSeed;
	if (!IsLocalController() || (!UsesFarmTool() && !bTreeSeed) || bInventoryOpen || IsFishing()
		|| !IsValid(FarmCharacter) || FarmCharacter->StatsComponent->Health <= 0 || !IsValid(GridWorld))
	{
		if (IsValid(FarmTargetMarker)) FarmTargetMarker->SetActorHiddenInGame(true);
		for (APSFarmTargetMarker* Marker : FarmSideMarkers) if (IsValid(Marker)) Marker->SetActorHiddenInGame(true);
		return;
	}
	const bool bCharged = bFarmCharging && FarmChargeElapsed >= FarmChargeDuration;
	TArray<FIntPoint> Cells;
	if (bTreeSeed)
	{
		const auto Target = GetCursorCell();
		if (Target.IsSet() && !bJournalOpen && !IsPaused()) Cells.Add(Target.GetValue());
	}
	else Cells = bFarmActionActive ? FarmActionCells : GetFarmTargetCells(bCharged);
	// Keep the pool when switching from nine cells to one or three; hide unused actors.
	FarmSideMarkers.SetNum(FMath::Max(FarmSideMarkers.Num(), Cells.Num() - 1));
	for (int32 Index = 0; Index <= FarmSideMarkers.Num(); ++Index)
	{
		TObjectPtr<APSFarmTargetMarker>& Marker = Index == 0 ? FarmTargetMarker : FarmSideMarkers[Index - 1];
		if (!Cells.IsValidIndex(Index))
		{
			if (IsValid(Marker)) Marker->SetActorHiddenInGame(true);
			continue;
		}
		if (!IsValid(Marker))
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = this;
			SpawnParameters.ObjectFlags = RF_Transient;
			Marker = GetWorld()->SpawnActor<APSFarmTargetMarker>(APSFarmTargetMarker::StaticClass(), SpawnParameters);
		}
		if (!Marker) continue;
		const FIntPoint Cell = Cells[Index];
		const bool bUsable = bTreeSeed
			? GridWorld->Gathering->CanPlantSeed(GetTreeSeedPlanter(), Cell, InventoryComponent->GetHotbarSlot(SelectedHotbarSlot))
			: FarmCharacter->StatsComponent->Stamina >= FarmToolStaminaCost && IsFarmSuccess(EvaluateFarmTarget(Cell));
		Marker->ShowTarget(GridWorld->CellToWorldCenter(Cell), GridWorld->GetCellSize(), bUsable);
	}
}

void APSPlayerController::OnUnPossess()
{
	CancelCropHarvest();
	CancelFarmAction();
	if (IsValid(FarmTargetMarker)) FarmTargetMarker->SetActorHiddenInGame(true);
	for (APSFarmTargetMarker* Marker : FarmSideMarkers) if (IsValid(Marker)) Marker->SetActorHiddenInGame(true);
	Super::OnUnPossess();
}
