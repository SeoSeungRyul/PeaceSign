#include "PSPlayerController.h"
#include "Gathering/PSGatheringActionComponent.h"
#include "PSPlayerCharacter.h"
#include "PSPlayerStatsComponent.h"
#include "Inventory/PSInventoryComponent.h"
#include "World/PSFarmTargetMarker.h"
#include "World/PSGridWorld.h"
#include "World/PSCropGrowth.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	bool IsFarmSuccess(const EPSTileInteractionResult Result)
	{
		return Result == EPSTileInteractionResult::Tilled || Result == EPSTileInteractionResult::Watered
			|| Result == EPSTileInteractionResult::Harvested;
	}
	void ShowFarmResult(const EPSTileInteractionResult Result)
	{
		if (!GEngine) return;
		const TCHAR* Message = Result == EPSTileInteractionResult::Tilled ? TEXT("밭을 갈았습니다.")
			: Result == EPSTileInteractionResult::Watered ? TEXT("밭에 물을 주었습니다.")
			: Result == EPSTileInteractionResult::Harvested ? TEXT("작물을 수확했습니다.")
			: Result == EPSTileInteractionResult::InventoryFull ? TEXT("가방이 가득 찼습니다.")
			: TEXT("이 칸에는 도구를 사용할 수 없습니다.");
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, IsFarmSuccess(Result) ? FColor::Green : FColor::Red, Message);
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
	if (!Item || Item->IsEmpty()) return EPSTileInteractionResult::NoEffect;
	if (Equipment == EPSEquipment::WateringCan && Item->ItemType == EPSItemType::WateringCan)
		return GridWorld->CanWaterCell(Cell) ? EPSTileInteractionResult::Watered : EPSTileInteractionResult::NoEffect;
	if (Equipment != EPSEquipment::Hoe || Item->ItemType != EPSItemType::Hoe
		|| (PSItems::UsesDurability(*Item) && Item->CurrentDurability <= 0)) return EPSTileInteractionResult::NoEffect;
	if (GridWorld->GetWorldObjectType(Cell) != EPSWorldObjectType::None) return EPSTileInteractionResult::NoEffect;
	// Keep the existing temporary hoe harvest as an explicit exception to tilling.
	if (GridWorld->GetCropType(Cell) != EPSCropType::None && GridWorld->GetCropStage(Cell) >= PSCropGrowth::MaxStage)
		return InventoryComponent->CanAddItem(EPSItemType::TestCrop, 1, GridWorld->GetCropId(Cell))
			? EPSTileInteractionResult::Harvested : EPSTileInteractionResult::InventoryFull;
	return GridWorld->CanTillCell(Cell) ? EPSTileInteractionResult::Tilled : EPSTileInteractionResult::NoEffect;
}

bool APSPlayerController::CanChargeEquippedHoe() const
{
	const FPSItemStack* Item = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	return Equipment == EPSEquipment::Hoe && Item && PSItems::CanChargeHoe(*Item);
}

int32 APSPlayerController::GetEquippedHoeChargeDepth() const
{
	const FPSItemStack* Item = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	return CanChargeEquippedHoe() ? PSItems::GetDefinition(*Item).ChargedHoeDepth : 0;
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
		for (int32 Row = 0; Row < GetEquippedHoeChargeDepth(); ++Row)
		{
			const FIntPoint Center = Front.GetValue() + Facing * Row;
			if (Row > 0) Cells.Add(Center);
			Cells.Add(Center + Side);
			Cells.Add(Center - Side);
		}
	}
	return Cells;
}

EPSTileInteractionResult APSPlayerController::EvaluateFarmActionCell(const FIntPoint Cell, const bool bCharged) const
{
	// Charging expands tilling only. Temporary hoe harvesting stays single-cell.
	if (bCharged && IsValid(GridWorld) && GridWorld->GetCropType(Cell) != EPSCropType::None)
		return EPSTileInteractionResult::NoEffect;
	return EvaluateFarmTarget(Cell);
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
	if (bFarmActionActive || bFarmCharging || bInventoryOpen || IsPaused() || IsFishing() || !CanChargeEquippedHoe()
		|| !IsValid(Farmer) || Farmer->IsMovementLocked() || Farmer->IsRolling()
		|| Farmer->StatsComponent->Health <= 0 || UWidgetBlueprintLibrary::IsDragDropping()) return false;
	CaptureFarmContext();
	bFarmCharging = true;
	FarmChargeElapsed = 0;
	return true;
}

void APSPlayerController::HandleSpecialAttackReleased()
{
	if (!bFarmCharging) return;
	UpdateFarmAction(0); // Validate even if the item/pawn changed since the last tick.
	if (!bFarmCharging) return;
	const bool bCharged = FarmChargeElapsed >= 0.5f;
	CancelFarmAction();
	if (!IsPaused()) BeginFarmAction(bCharged);
}

void APSPlayerController::HandleSpecialAttackCanceled()
{
	if (bFarmCharging) CancelFarmAction();
}

void APSPlayerController::FlushPressedKeys()
{
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
		|| (bCharged && !CanChargeEquippedHoe())) return false;
	const TArray<FIntPoint> Cells = GetFarmTargetCells(bCharged);
	if (Cells.IsEmpty()) return false;
	if (!Cells.ContainsByPredicate([&](const FIntPoint Cell) { return IsFarmSuccess(EvaluateFarmActionCell(Cell, bCharged)); }))
	{
		ShowFarmResult(EvaluateFarmActionCell(Cells[0], bCharged));
		return false;
	}
	FarmActionCells = Cells;
	FarmActionCell = Cells[0];
	bFarmActionCharged = bCharged;
	CaptureFarmContext();
	FarmActionTimeRemaining = FMath::IsFinite(FarmActionDuration) ? FMath::Max(0.01f, FarmActionDuration) : 0.5f;
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
	bFarmActionCharged = false;
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
		FarmChargeElapsed = FMath::Min(0.5f, FarmChargeElapsed + DeltaSeconds);
		return;
	}
	FarmActionTimeRemaining = FMath::Max(0.0f, FarmActionTimeRemaining - DeltaSeconds);
	if (FarmActionTimeRemaining > UE_KINDA_SMALL_NUMBER) return;
	const EPSEquipment Tool = FarmActionEquipment;
	const FIntPoint Cell = FarmActionCell;
	EPSTileInteractionResult Result = EPSTileInteractionResult::NoEffect;
	{
		TGuardValue<bool> Completing(bCompletingFarmAction, true);
		if (bFarmActionCharged)
		{
			// One completed swing costs one durability, even when it tills multiple cells.
			// Apply all cells before consuming the last durability and unequipping.
			for (const FIntPoint Target : FarmActionCells)
				if (EvaluateFarmActionCell(Target, true) == EPSTileInteractionResult::Tilled
					&& GridWorld->TillCell(Target) == EPSTileInteractionResult::Tilled) Result = EPSTileInteractionResult::Tilled;
			if (IsFarmSuccess(Result))
			{
				bool bDestroyed = false;
				InventoryComponent->ConsumeHotbarDurability(FarmActionSlot, 1, bDestroyed);
			}
		}
		else
		{
			Result = EvaluateFarmTarget(Cell);
			if (IsFarmSuccess(Result)) Result = UseEquippedItemOnCell(Cell);
		}
	}
	CancelFarmAction();
	ShowFarmResult(Result);
	OnFarmToolFinished(Tool, Cell, IsFarmSuccess(Result));
}

void APSPlayerController::UpdateFarmTargetMarker()
{
	const APSPlayerCharacter* FarmCharacter = Cast<APSPlayerCharacter>(GetPawn());
	if (!IsLocalController() || !UsesFarmTool() || bInventoryOpen || IsFishing()
		|| !IsValid(FarmCharacter) || FarmCharacter->StatsComponent->Health <= 0 || !IsValid(GridWorld))
	{
		if (IsValid(FarmTargetMarker)) FarmTargetMarker->SetActorHiddenInGame(true);
		for (APSFarmTargetMarker* Marker : FarmSideMarkers) if (IsValid(Marker)) Marker->SetActorHiddenInGame(true);
		return;
	}
	const bool bCharged = bFarmActionActive ? bFarmActionCharged : bFarmCharging && FarmChargeElapsed >= 0.5f;
	const TArray<FIntPoint> Cells = bFarmActionActive ? FarmActionCells : GetFarmTargetCells(bCharged);
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
		if (Marker) Marker->ShowTarget(GridWorld->CellToWorldCenter(Cells[Index]), GridWorld->GetCellSize(),
			IsFarmSuccess(EvaluateFarmActionCell(Cells[Index], bCharged)));
	}
}

void APSPlayerController::OnUnPossess()
{
	CancelFarmAction();
	if (IsValid(FarmTargetMarker)) FarmTargetMarker->SetActorHiddenInGame(true);
	for (APSFarmTargetMarker* Marker : FarmSideMarkers) if (IsValid(Marker)) Marker->SetActorHiddenInGame(true);
	Super::OnUnPossess();
}
