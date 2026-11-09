#include "PSPlayerController.h"
#include "PSPlayerCharacter.h"
#include "PSPlayerStatsComponent.h"
#include "Data/PSCropDefinition.h"
#include "Gathering/PSBushInteractionComponent.h"
#include "Gathering/PSGatheringActionComponent.h"
#include "Gathering/PSGatheringWorldComponent.h"
#include "Inventory/PSInventoryComponent.h"
#include "Skills/PSPlayerSkillComponent.h"
#include "Time/PSGameTimeSubsystem.h"
#include "UI/PSPlayerStatusWidget.h"
#include "World/PSCropGrowth.h"
#include "World/PSGridWorld.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	int32 CellDistance(const FIntPoint A, const FIntPoint B)
	{
		return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
	}
}

bool APSPlayerController::CanHarvestCropHere(const FIntPoint Cell) const
{
	const auto* Farmer = Cast<APSPlayerCharacter>(GetPawn());
	return IsValid(GridWorld) && IsValid(Farmer) && Farmer->StatsComponent->Health > 0
		&& !Farmer->HasMovementInput() && !Farmer->IsRolling() && !Farmer->IsMovementLocked()
		&& !bInventoryOpen && !bJournalOpen && !bFarmActionActive && !bFarmCharging && !IsPaused() && !IsFishing()
		&& !GatheringAction->IsGatheringActive() && !BushInteraction->IsActive() && !UWidgetBlueprintLibrary::IsDragDropping()
		&& CellDistance(GridWorld->WorldToCell(Farmer->GetActorLocation()), Cell) <= 2
		&& GridWorld->GetCropStage(Cell) == PSCropGrowth::MaxStage;
}

TOptional<FIntPoint> APSPlayerController::FindHarvestableCrop() const
{
	if (!GridWorld || !GetPawn()) return {};
	if (CropHarvestTarget.IsSet() && CanHarvestCropHere(CropHarvestTarget.GetValue())) return CropHarvestTarget;
	const auto Cursor = GetCursorCell();
	if (Cursor.IsSet() && CanHarvestCropHere(Cursor.GetValue())) return Cursor;
	const FIntPoint Origin = GridWorld->WorldToCell(GetPawn()->GetActorLocation());
	for (int32 Distance = 0; Distance <= 2; ++Distance)
		for (int32 Y = -Distance; Y <= Distance; ++Y)
			for (int32 X = -Distance; X <= Distance; ++X)
				if (FMath::Abs(X) + FMath::Abs(Y) == Distance && CanHarvestCropHere(Origin + FIntPoint(X, Y)))
					return Origin + FIntPoint(X, Y);
	return {};
}

bool APSPlayerController::BeginCropHarvest()
{
	if (CropHarvestTarget.IsSet()) return true;
	const auto Cell = FindHarvestableCrop();
	if (!Cell.IsSet()) return false;
	auto* Farmer = CastChecked<APSPlayerCharacter>(GetPawn());
	CropHarvestTarget = Cell;
	CropHarvestGrid = GridWorld;
	CropHarvestPawn = Farmer;
	CropHarvestStart = Farmer->GetActorLocation();
	CropHarvestHealth = Farmer->StatsComponent->Health;
	CropHarvestId = GridWorld->GetCropId(Cell.GetValue());
	CropHarvestPlanted = GridWorld->GetCropPlantingMinute(Cell.GetValue());
	CropHarvestElapsed = 0;
	++CropHarvestSession;
	Farmer->StatsComponent->SetActionActive(true);
	return true;
}

void APSPlayerController::CancelCropHarvest()
{
	if (auto* Farmer = CropHarvestPawn.Get()) Farmer->StatsComponent->SetActionActive(false);
	CropHarvestTarget.Reset();
	CropHarvestGrid.Reset();
	CropHarvestPawn.Reset();
	CropHarvestElapsed = 0;
}

void APSPlayerController::AdvanceCropHarvest(const float DeltaTime)
{
	if (!CropHarvestTarget.IsSet()) return;
	const FIntPoint Cell = CropHarvestTarget.GetValue();
	const auto* Farmer = CropHarvestPawn.Get();
	if (!Farmer || GetPawn() != Farmer || GridWorld.Get() != CropHarvestGrid.Get() || !CanHarvestCropHere(Cell)
		|| !Farmer->GetActorLocation().Equals(CropHarvestStart, 0.1f) || Farmer->StatsComponent->Health < CropHarvestHealth
		|| GridWorld->GetCropId(Cell) != CropHarvestId || GridWorld->GetCropPlantingMinute(Cell) != CropHarvestPlanted)
	{
		CancelCropHarvest();
		return;
	}
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0) return;
	CropHarvestElapsed += DeltaTime;
	if (CropHarvestElapsed + UE_KINDA_SMALL_NUMBER < CropHarvestDuration) return;
	CancelCropHarvest();
	CompleteCropHarvest(Cell);
}

bool APSPlayerController::CompleteCropHarvest(const FIntPoint Cell)
{
	if (!InventoryComponent || !CanHarvestCropHere(Cell)) return false;
	const int32 CropId = GridWorld->GetCropId(Cell);
	const auto* Definition = GridWorld->FindCropDefinition(CropId);
	FPSItemStack Produce;
	Produce.ItemType = EPSItemType::TestCrop;
	Produce.ItemId = Definition ? FName(*FString::Printf(TEXT("%06d"), CropId)) : NAME_None;
	Produce.CropId = CropId;
	Produce.Quantity = 1 + (Definition && FMath::FRand() * 100.f < FMath::Clamp(Definition->Bonus, 0.f, 100.f) ? 1 : 0);
	FPSItemStack Seed;
	if (Definition && !Definition->SeedItemID.IsNone() && FMath::FRand() < 0.10f)
	{
		Seed.ItemType = EPSItemType::TestSeed;
		Seed.ItemId = Definition->SeedItemID;
		Seed.CropId = CropId;
		Seed.Quantity = 1;
	}
	const bool bCanStoreProduce = InventoryComponent->CanAddItemVariant(Produce.ItemType, Produce.ItemId, 1, Produce.CropId);
	if (GridWorld->HarvestCropForPlayer(Cell) != EPSTileInteractionResult::Harvested) return false;
	// Commit the claim before granting inventory items. If the bag is full,
	// creating the ground drop saves the claim and reward together.
	if (bCanStoreProduce) GridWorld->SaveGatheringState();
	const auto GiveOrDrop = [&](FPSItemStack Item)
	{
		int32 Accepted = Item.Quantity;
		while (Accepted > 0 && !InventoryComponent->CanAddItemVariant(Item.ItemType, Item.ItemId, Accepted, Item.CropId)) --Accepted;
		if (Accepted > 0 && !InventoryComponent->AddItemVariant(Item.ItemType, Item.ItemId, Accepted, Item.CropId)) Accepted = 0;
		Item.Quantity -= Accepted;
		if (Item.Quantity > 0) GridWorld->Gathering->DropItemStack(Cell, Item);
	};
	GiveOrDrop(Produce);
	if (!Seed.IsEmpty()) GiveOrDrop(Seed);
	++HarvestedCropCount;
	if (SkillComponent) SkillComponent->AddExperience(EPSPlayerSkillField::Farming, 2);
	if (StatusWidget) StatusWidget->SetHarvestedCropCount(HarvestedCropCount);
	if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Green, TEXT("작물을 수확했습니다."));
	return true;
}

EPSTileInteractionResult APSPlayerController::PlantEquippedCrop(const FIntPoint Cell)
{
	const auto* Farmer = Cast<APSPlayerCharacter>(GetPawn());
	if (!GridWorld || !Farmer || Farmer->StatsComponent->Health <= 0 || !InventoryComponent
		|| CellDistance(GridWorld->WorldToCell(Farmer->GetActorLocation()), Cell) > 2) return EPSTileInteractionResult::NoEffect;
	const FPSItemStack Seed = InventoryComponent->GetHotbarSlot(SelectedHotbarSlot);
	if (Seed.IsEmpty() || Seed.ItemType != EPSItemType::TestSeed) return EPSTileInteractionResult::NoEffect;
	if (Seed.CropId != 0)
	{
		const auto* Row = GridWorld->FindCropDefinition(Seed.CropId);
		if (!Row || Row->SeedItemID.IsNone() || Row->SeedItemID != Seed.ItemId) return EPSTileInteractionResult::NoEffect;
		const auto* Clock = GetWorld()->GetSubsystem<UPSGameTimeSubsystem>();
		if (!Clock || Clock->GetSeasonIndex() != Row->Season)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Yellow, TEXT("지금 계절에는 이 씨앗을 심을 수 없습니다."));
			return EPSTileInteractionResult::NoEffect;
		}
		if (!SkillComponent || SkillComponent->GetSkillLevel(EPSPlayerSkillField::Farming) < Row->LV)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Yellow,
				FString::Printf(TEXT("농사 레벨 %d이 필요합니다. 먼저 밭을 갈아 보세요."), Row->LV));
			return EPSTileInteractionResult::NoEffect;
		}
	}
	const auto Result = GridWorld->PlantSeed(Cell, Seed.CropId);
	if (Result == EPSTileInteractionResult::Planted && !InventoryComponent->RemoveFromHotbarSlot(SelectedHotbarSlot, 1))
	{
		GridWorld->RemoveCrop(Cell);
		return EPSTileInteractionResult::NoEffect;
	}
	return Result;
}

void APSPlayerController::UnlockFarmingFromTilling()
{
	if (!SkillComponent) return;
	const auto Farming = SkillComponent->GetSkillState(EPSPlayerSkillField::Farming);
	if (Farming.Level != 0) return;
	SkillComponent->AddExperience(EPSPlayerSkillField::Farming,
		FMath::Max(1, UPSPlayerSkillComponent::ExperiencePerLevel - Farming.Experience));
	if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 3, FColor::Green, TEXT("농사 레벨 1을 열었습니다."));
}
