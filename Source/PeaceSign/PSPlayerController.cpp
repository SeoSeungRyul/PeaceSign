// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSPlayerController.h"
#include "Gathering/PSGatheringActionComponent.h"
#include "Gathering/PSBushInteractionComponent.h"
#include "Gathering/PSGatheringWorldComponent.h"
#include "Data/PSGameDataTables.h"
#include "PSPlayerStatsComponent.h"
#include "UI/PSPlayerStatusWidget.h"
#include "UI/PSGameTimeWidget.h"
#include "UI/PSInventoryWidget.h"
#include "UI/PSHotbarWidget.h"
#include "UI/PSFishingWidget.h"
#include "UI/PSFishingJournalWidget.h"
#include "Fishing/PSFishingBobber.h"
#include "Kismet/GameplayStatics.h"
#include "Inventory/PSInventoryComponent.h"
#include "Inventory/PSWorldItemActor.h"
#include "Skills/PSPlayerSkillComponent.h"
#include "Fishing/PSFishingJournalComponent.h"
#include "PSPlayerCharacter.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Time/PSGameTimeSubsystem.h"

#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"
#include "World/PSCropGrowth.h"
#include "World/PSGridWorld.h"
#include "World/PSFarmTargetMarker.h"

namespace
{
	TArray<FPSFishDefinition> GetFallbackFish(TArray<FName>& OutIds)
	{
		TArray<FPSFishDefinition> Result;
		OutIds.Reset();
		const auto Add = [&Result, &OutIds](const TCHAR* RowId, const TCHAR* Name, const int32 Difficulty, const int32 MinSize,
			const int32 MaxSize, const float Weight)
		{
			OutIds.Add(RowId);
			FPSFishDefinition& Fish = Result.AddDefaulted_GetRef();
			Fish.Name = FText::FromString(Name);
			Fish.Difficulty = Difficulty;
			Fish.MinSize = MinSize;
			Fish.MaxSize = MaxSize;
			Fish.Weight = Weight;
		};
		Add(TEXT("Fallback.CrucianCarp"), TEXT("붕어"), 1, 8, 28, 50.0f);
		Add(TEXT("Fallback.Carp"), TEXT("잉어"), 2, 25, 70, 30.0f);
		Add(TEXT("Fallback.Catfish"), TEXT("메기"), 3, 45, 110, 15.0f);
		Add(TEXT("Fallback.GoldenCarp"), TEXT("황금 잉어"), 4, 35, 90, 5.0f);
		return Result;
	}

	FColor GetTileDebugColor(const EPSTileType TileType)
	{
		switch (TileType)
		{
		case EPSTileType::Grass:
			return FColor::Green;
		case EPSTileType::Dirt:
			return FColor(150, 75, 20);
		case EPSTileType::TilledSoil:
			return FColor(105, 48, 15);
		case EPSTileType::Stone:
			return FColor::Silver;
		case EPSTileType::Water:
			return FColor::Cyan;
		case EPSTileType::Empty:
		default:
			return FColor::Red;
		}
	}

	FString GetInteractionResultText(const EPSTileInteractionResult Result)
	{
		switch (Result)
		{
		case EPSTileInteractionResult::Tilled:
			return TEXT("Soil tilled");
		case EPSTileInteractionResult::GroundChanged:
			return TEXT("풀 / 흙 타일 전환 완료");
		case EPSTileInteractionResult::StoneDamaged:
			return TEXT("Stone damaged");
		case EPSTileInteractionResult::Mined:
			return TEXT("Stone object mined; ground preserved");
		case EPSTileInteractionResult::Planted:
			return TEXT("Seed planted");
		case EPSTileInteractionResult::Harvested:
			return TEXT("Crop harvested");
		case EPSTileInteractionResult::CropRemoved:
			return TEXT("Crop removed");
		case EPSTileInteractionResult::Watered:
			return TEXT("Soil watered until next morning at 06:00");
		case EPSTileInteractionResult::InventoryFull:
			return TEXT("Inventory is full. Crop was not harvested.");
		case EPSTileInteractionResult::NoEffect:
			return TEXT("This tile has no interaction yet");
		case EPSTileInteractionResult::InvalidCell:
		default:
			return TEXT("Cannot interact with this cell");
		}
	}
}

APSPlayerController::APSPlayerController()
{
	GatheringAction = CreateDefaultSubobject<UPSGatheringActionComponent>(TEXT("GatheringAction"));
	BushInteraction = CreateDefaultSubobject<UPSBushInteractionComponent>(TEXT("BushInteraction"));
	InventoryComponent = CreateDefaultSubobject<UPSInventoryComponent>(TEXT("InventoryComponent"));
	SkillComponent = CreateDefaultSubobject<UPSPlayerSkillComponent>(TEXT("SkillComponent"));
	FishingJournalComponent = CreateDefaultSubobject<UPSFishingJournalComponent>(TEXT("FishingJournalComponent"));
	HotbarMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Hotbar"));
	HotbarSlotActions.Reset();
	const FKey HotbarKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(HotbarKeys); ++Index)
	{
		UInputAction* Action = CreateDefaultSubobject<UInputAction>(
			*FString::Printf(TEXT("IA_HotbarSlot%d"), Index + 1));
		Action->ValueType = EInputActionValueType::Boolean;
		HotbarSlotActions.Add(Action);
		const bool bAlreadyMapped = HotbarMappingContext->GetMappings().ContainsByPredicate(
			[Action, Key = HotbarKeys[Index]](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action && Mapping.Action->GetFName() == Action->GetFName() && Mapping.Key == Key;
			});
		if (!bAlreadyMapped) HotbarMappingContext->MapKey(Action, HotbarKeys[Index]);
	}
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> GameplayMappingContextFinder(
		TEXT("/Game/Inputs/IMC_Gameplay.IMC_Gameplay"));
	static ConstructorHelpers::FObjectFinder<UInputAction> InteractActionFinder(
		TEXT("/Game/Inputs/IA_Interact.IA_Interact"));
	static ConstructorHelpers::FObjectFinder<UInputAction> SpecialAttackActionFinder(
		TEXT("/Game/Inputs/IA_SpecialAttack.IA_SpecialAttack"));
	static ConstructorHelpers::FObjectFinder<UInputAction> InventoryActionFinder(
		TEXT("/Game/Inputs/IA_Inventory.IA_Inventory"));
	static ConstructorHelpers::FObjectFinder<UInputAction> QuestActionFinder(
		TEXT("/Game/Inputs/IA_Quest.IA_Quest"));
	static ConstructorHelpers::FObjectFinder<UInputAction> AbilityActionFinder(
		TEXT("/Game/Inputs/IA_Ability.IA_Ability"));
	static ConstructorHelpers::FObjectFinder<UInputAction> CraftActionFinder(
		TEXT("/Game/Inputs/IA_Craft.IA_Craft"));

	GameplayMappingContext = GameplayMappingContextFinder.Object;
	InteractAction = InteractActionFinder.Object;
	SpecialAttackAction = SpecialAttackActionFinder.Object;
	InventoryAction = InventoryActionFinder.Object;
	QuestAction = QuestActionFinder.Object;
	AbilityAction = AbilityActionFinder.Object;
	CraftAction = CraftActionFinder.Object;
}

void APSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshEquipmentFromHotbar);
	}
	RefreshEquipmentFromHotbar();
	if (IsLocalController())
	{
		TimeWidget = CreateWidget<UPSGameTimeWidget>(this, UPSGameTimeWidget::StaticClass());
		if (TimeWidget) TimeWidget->AddToPlayerScreen();
		UClass* Class = HotbarWidgetClass.Get();
		HotbarWidget = CreateWidget<UPSHotbarWidget>(this, Class ? Class : UPSHotbarWidget::StaticClass());
		if (HotbarWidget)
		{
			HotbarWidget->SetInventoryComponent(InventoryComponent);
			HotbarWidget->AddToViewport(300);
		}
		UClass* FishingClass = FishingWidgetClass.Get();
		FishingWidget = CreateWidget<UPSFishingWidget>(this, FishingClass ? FishingClass : UPSFishingWidget::StaticClass());
		if (FishingWidget) FishingWidget->AddToViewport(350);
	}

	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	for (TActorIterator<APSGridWorld> It(GetWorld()); It; ++It)
	{
		GridWorld = *It;
		break;
	}

	if (!GridWorld && IsLocalController())
	{
		GridWorld = GetWorld()->SpawnActor<APSGridWorld>(
			APSGridWorld::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator);
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	// UE_LOG(
	// 	LogTemp,
	// 	Warning,
	// 	TEXT("PeaceSign mapping setup: Controller=%s Local=%s Subsystem=%s Context=%s"),
	// 	*GetName(),
	// 	IsLocalController() ? TEXT("true") : TEXT("false"),
	// 	*GetNameSafe(InputSubsystem),
	// 	*GetNameSafe(GameplayMappingContext));

	if (InputSubsystem && GameplayMappingContext)
	{
		const TArray<FEnhancedActionKeyMapping>& Mappings = GameplayMappingContext->GetMappings();
		if (Mappings.IsEmpty())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Gameplay input context '%s' has no Default Key Mappings."),
				*GetNameSafe(GameplayMappingContext));
		}

		for (const FEnhancedActionKeyMapping& Mapping : Mappings)
		{
			if (!Mapping.Action || !Mapping.Key.IsValid())
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("Gameplay input context '%s' contains an invalid mapping: Action=%s Key=%s"),
					*GetNameSafe(GameplayMappingContext),
					*GetNameSafe(Mapping.Action),
					*Mapping.Key.ToString());
			}
		}

		InputSubsystem->AddMappingContext(GameplayMappingContext, 0);
	}
	if (InputSubsystem && HotbarMappingContext)
	{
		InputSubsystem->AddMappingContext(HotbarMappingContext, 1);
	}
}

void APSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
	if (InteractAction)
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APSPlayerController::HandleInteract);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &APSPlayerController::HandleInteractReleased);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &APSPlayerController::HandleInteractReleased);
	}
	if (SpecialAttackAction)
	{
		EnhancedInputComponent->BindAction(SpecialAttackAction, ETriggerEvent::Started, this, &APSPlayerController::HandleSpecialAttack);
		EnhancedInputComponent->BindAction(SpecialAttackAction, ETriggerEvent::Completed, this, &APSPlayerController::HandleSpecialAttackReleased);
		EnhancedInputComponent->BindAction(SpecialAttackAction, ETriggerEvent::Canceled, this, &APSPlayerController::HandleSpecialAttackCanceled);
	}
	if (InventoryAction)
	{
		EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &APSPlayerController::HandleInventory);
	}
	if (QuestAction)
	{
		EnhancedInputComponent->BindAction(QuestAction, ETriggerEvent::Started, this, &APSPlayerController::HandleQuest);
	}
	if (AbilityAction)
	{
		EnhancedInputComponent->BindAction(AbilityAction, ETriggerEvent::Started, this, &APSPlayerController::HandleAbility);
	}
	if (CraftAction)
	{
		EnhancedInputComponent->BindAction(CraftAction, ETriggerEvent::Started, this, &APSPlayerController::HandleCraft);
	}
	for (int32 Index = 0; Index < HotbarSlotActions.Num(); ++Index)
	{
		if (HotbarSlotActions[Index])
		{
			EnhancedInputComponent->BindAction(HotbarSlotActions[Index], ETriggerEvent::Started,
				this, &ThisClass::HandleHotbarSlot, Index);
		}
	}

	// R is a development-only world reset shortcut and is intentionally not part of the gameplay IA set.
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &APSPlayerController::HandleResetWorld);
	InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &APSPlayerController::HandleAdvanceSeason);
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &APSPlayerController::HandleAdvanceTime);
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &APSPlayerController::HandleFishingUp);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &APSPlayerController::HandleFishingLeft);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &APSPlayerController::HandleFishingDown);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &APSPlayerController::HandleFishingRight);
}

void APSPlayerController::UpdateCameraManager(const float DeltaSeconds)
{
	Super::UpdateCameraManager(DeltaSeconds);
	if (!PlayerCameraManager) return;
	const APSPlayerCharacter* ViewCharacter = Cast<APSPlayerCharacter>(PlayerCameraManager->GetViewTarget());
	FMinimalViewInfo View = PlayerCameraManager->GetCameraCacheView();
	if (!ViewCharacter || View.ProjectionMode != ECameraProjectionMode::Orthographic || !View.bAutoCalculateOrthoPlanes) return;
	// UE's orthographic plane correction adds the entire camera-to-actor vector to
	// the final view origin. A lateral pan would therefore be cancelled by the pawn.
	// Preserve the depth along the view direction, excluding that lateral vector.
	const FVector Forward = View.Rotation.Vector();
	const double Depth = FVector::DotProduct(ViewCharacter->GetActorLocation() - View.Location, Forward);
	View.SetCameraToViewTarget(View.Location + Forward * Depth);
	PlayerCameraManager->SetCameraCachePOV(View);
}

void APSPlayerController::PlayerTick(const float DeltaTime)
{
	// Advance existing actions before Super processes this frame's new input.
	// A newly pressed tool must not consume time elapsed before that press.
	UpdateFarmAction(DeltaTime);
	if (!IsPaused()) UpdateFishing(DeltaTime);
	const bool bWasHarvesting = CropHarvestTarget.IsSet();
	const uint32 HarvestSession = CropHarvestSession;
	Super::PlayerTick(DeltaTime);
	if (bWasHarvesting && HarvestSession == CropHarvestSession) AdvanceCropHarvest(DeltaTime);
	UpdateStatusWidget();
	UpdateFarmTargetMarker();
	if (bInventoryOpen) return;
	if (UsesFarmTool() || Equipment == EPSEquipment::TreeSeed) return;
	HoveredCell = GetCursorCell();

	if (!HoveredCell.IsSet())
	{
		return;
	}
	const FIntPoint Cell = HoveredCell.GetValue();
	const EPSTileType TileType = GridWorld->GetGroundTile(Cell);
	const FVector CellCenter = GridWorld->CellToWorldCenter(Cell);
	const float HalfCell = GridWorld->GetCellSize() * 0.48f;
	const bool bStoneOutOfRange = GridWorld->GetWorldObjectType(Cell) == EPSWorldObjectType::Stone && GetPawn()
		&& !GridWorld->CanMineFrom(GridWorld->WorldToCell(GetPawn()->GetActorLocation()), Cell);
	FColor HoverColor = GetTileDebugColor(TileType);
	if (Equipment == EPSEquipment::FishingRod)
	{
		HoverColor = GetPawn() && GridWorld->CanFishFrom(
			GridWorld->WorldToCell(GetPawn()->GetActorLocation()), Cell) ? FColor::Green : FColor::Red;
	}
	else if (bStoneOutOfRange)
	{
		HoverColor = FColor::Red;
	}
	DrawDebugBox(
		GetWorld(),
		CellCenter,
		FVector(HalfCell, HalfCell, 1.0f),
		HoverColor,
		false,
		0.0f,
		0,
		2.0f);

	if (GEngine)
	{
		const FString TileName = StaticEnum<EPSTileType>()->GetNameStringByValue(static_cast<int64>(TileType));
		GEngine->AddOnScreenDebugMessage(
			3,
			0.0f,
			GetTileDebugColor(TileType),
			FString::Printf(
				TEXT("Cell (%d, %d)  Tile: %s"),
				Cell.X,
				Cell.Y,
				*TileName));
	}
}

TOptional<FIntPoint> APSPlayerController::GetCursorCell() const
{
	if (!GridWorld) return {};
	FVector RayOrigin, RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z)) return {};
	const float Distance = (GridWorld->GetActorLocation().Z - RayOrigin.Z) / RayDirection.Z;
	if (Distance < 0.0f) return {};
	return GridWorld->WorldToCell(RayOrigin + RayDirection * Distance);
}

void APSPlayerController::HandleInteract()
{
	if (GatheringAction->IsGatheringActive()) return;
	if (bFarmActionActive || bFarmCharging || IsPaused()) return;
	if (AreFishingControlsLocked() && Equipment != EPSEquipment::FishingRod) return;
	if (BeginCropHarvest()) return;
	if (BushInteraction->Press(GridWorld, false)) return;
	if (Equipment == EPSEquipment::FishingRod)
	{
		HandleFishingRod();
		return;
	}
	OnInteractRequested();
}

void APSPlayerController::HandleSpecialAttack()
{
	if (GatheringAction->IsGatheringActive()) return;
	if (CropHarvestTarget.IsSet() || bFarmActionActive || bFarmCharging || bInventoryOpen || bJournalOpen || IsPaused()) return;
	if (AreFishingControlsLocked() && Equipment != EPSEquipment::FishingRod) return;
	const TOptional<FIntPoint> TargetCell = GetCursorCell();
	if (BushInteraction->Press(GridWorld, true, TargetCell)) return;
	if (Equipment == EPSEquipment::TreeSeed)
	{
		if (!TargetCell.IsSet()) return;
		const bool bPlanted = UseEquippedItemOnCell(TargetCell.GetValue()) == EPSTileInteractionResult::Planted;
		if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.0f, bPlanted ? FColor::Green : FColor::Red,
			bPlanted ? TEXT("나무 씨앗을 심었습니다.") : TEXT("가까운 빈 풀·물 타일에 나무 씨앗을 심을 수 있습니다."));
		return;
	}
	if (!IsFishing())
	{
		if (TargetCell.IsSet() && GatheringAction->HandleTree(GridWorld, TargetCell.GetValue())) return;
	}
	if (UsesFarmTool())
	{
		if (GetEquippedFarmChargeDepth() > 0) BeginFarmCharge();
		else BeginFarmAction();
		return;
	}
	if (Equipment == EPSEquipment::FishingRod)
	{
		if (FishingState == EPSFishingState::CastTiming) { ConfirmFishingCast(); return; }
		HandleFishingRod();
		return;
	}
	if (TargetCell.IsSet())
	{
		const EPSEquipment UsedEquipment = Equipment;
		if (UsedEquipment != EPSEquipment::Shovel && GridWorld && GetPawn() && GridWorld->GetWorldObjectType(TargetCell.GetValue()) == EPSWorldObjectType::Stone
			&& !GridWorld->CanMineFrom(GridWorld->WorldToCell(GetPawn()->GetActorLocation()), TargetCell.GetValue()))
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.0f, FColor::Red,
				TEXT("Stone is out of mining range"));
			return;
		}
		const EPSTileInteractionResult Result = UseEquippedItemOnCell(TargetCell.GetValue());
		if (UsedEquipment == EPSEquipment::BareHands)
		{
			if (Result == EPSTileInteractionResult::CropRemoved)
			{
				if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.0f, FColor::Yellow, GetInteractionResultText(Result));
				return;
			}
			OnSpecialAttackRequested();
			return;
		}
		if (UsedEquipment != EPSEquipment::UnusableItem && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				INDEX_NONE,
				2.0f,
				Result == EPSTileInteractionResult::InvalidCell ? FColor::Red : FColor::Yellow,
				GetInteractionResultText(Result));
		}
	}
	else if (Equipment == EPSEquipment::BareHands)
	{
		OnSpecialAttackRequested();
	}
}

void APSPlayerController::HandleInteractReleased()
{
	CancelCropHarvest();
	BushInteraction->Release(false);
}

const APSPlayerCharacter* APSPlayerController::GetTreeSeedPlanter() const
{
	const auto* Planter = Cast<APSPlayerCharacter>(GetPawn());
	if (!IsValid(Planter) || !Planter->StatsComponent || Planter->StatsComponent->Health <= 0
		|| Planter->IsRolling() || Planter->IsMovementLocked() || IsPaused() || IsFishing()
		|| bInventoryOpen || bJournalOpen || UWidgetBlueprintLibrary::IsDragDropping()) return nullptr;
	return Planter;
}

EPSTileInteractionResult APSPlayerController::UseEquippedItemOnCell(const FIntPoint Cell)
{
	if (!GridWorld) return EPSTileInteractionResult::InvalidCell;
	if (Equipment == EPSEquipment::TreeSeed)
	{
		return GridWorld->Gathering->PlantSeed(GetTreeSeedPlanter(), Cell, InventoryComponent, SelectedHotbarSlot)
			? EPSTileInteractionResult::Planted : EPSTileInteractionResult::InvalidCell;
	}
	if (UsesFarmTool()) return ApplyFarmTool(MakeArrayView(&Cell, 1));
	// Handle the shovel before the shared stone-mining path: it only changes ground.
	if (Equipment == EPSEquipment::Shovel)
	{
		const FPSItemStack* Shovel = InventoryComponent
			? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
		if (!Shovel || Shovel->IsEmpty() || Shovel->ItemType != EPSItemType::Shovel
			|| Shovel->CurrentDurability <= 0)
			return EPSTileInteractionResult::NoEffect;
		const EPSTileInteractionResult Result = GridWorld->ShovelCell(Cell);
		if (Result == EPSTileInteractionResult::GroundChanged)
		{
			bool bDestroyed = false;
			InventoryComponent->ConsumeHotbarDurability(SelectedHotbarSlot, 1, bDestroyed);
		}
		return Result;
	}
	if (GridWorld->GetWorldObjectType(Cell) == EPSWorldObjectType::Stone)
	{
		const FPSItemStack* EquippedStack = InventoryComponent
			? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
		const EPSMineralType MineralType = GridWorld->GetMineralType(Cell);
		const EPSTileInteractionResult Result = GridWorld->MineCell(Cell, PSItems::GetMiningPower(EquippedStack));
		if (Result == EPSTileInteractionResult::Mined
			&& (!InventoryComponent || !InventoryComponent->AddItem(EPSItemType::Stone, 1)))
		{
			DropStoneReward(Cell);
		}
		if (Result == EPSTileInteractionResult::Mined && MineralType != EPSMineralType::None
			&& EquippedStack && EquippedStack->ItemType == EPSItemType::Pickaxe)
		{
			FName OreItemId = NAME_None;
			switch (MineralType)
			{
			case EPSMineralType::Copper: OreItemId = PSItemIds::CopperOre; break;
			case EPSMineralType::Iron: OreItemId = PSItemIds::IronOre; break;
			case EPSMineralType::Silver: OreItemId = PSItemIds::SilverOre; break;
			case EPSMineralType::Gold: OreItemId = PSItemIds::GoldOre; break;
			case EPSMineralType::Titanium: OreItemId = PSItemIds::TitaniumOre; break;
			case EPSMineralType::Lumistone: OreItemId = PSItemIds::LumistoneOre; break;
			case EPSMineralType::Asterium: OreItemId = PSItemIds::AsteriumOre; break;
			default: break;
			}
			if (!OreItemId.IsNone()
				&& (!InventoryComponent || !InventoryComponent->AddItemVariant(EPSItemType::Ore, OreItemId, 1)))
			{
				FPSItemStack OreStack;
				OreStack.ItemType = EPSItemType::Ore;
				OreStack.ItemId = OreItemId;
				OreStack.Quantity = 1;
				DropItemReward(Cell, OreStack);
			}
		}
		return Result;
	}
	switch (Equipment)
	{
	case EPSEquipment::BareHands:
		return GridWorld->RemoveCrop(Cell);
	case EPSEquipment::Seed:
		return PlantEquippedCrop(Cell);
	case EPSEquipment::FishingRod:
	case EPSEquipment::UnusableItem:
	default:
		return EPSTileInteractionResult::NoEffect;
	}
}

void APSPlayerController::HandleHotbarSlot(const FInputActionValue& Value, const int32 SlotIndex)
{
	SelectHotbarSlot(SlotIndex);
}

void APSPlayerController::SelectHotbarSlot(const int32 SlotIndex)
{
	if (SlotIndex != SelectedHotbarSlot) CancelCropHarvest();
	if (GatheringAction->IsGatheringActive()) return;
	if (bFarmActionActive) return;
	if (bFarmCharging) CancelFarmAction();
	if (AreFishingControlsLocked()) return;
	if (!InventoryComponent || !InventoryComponent->IsHotbarSlot(SlotIndex)) return;
	InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshEquipmentFromHotbar);
	SelectedHotbarSlot = SlotIndex;
	RefreshEquipmentFromHotbar();
}

void APSPlayerController::RefreshEquipmentFromHotbar()
{
	if ((bFarmActionActive || bFarmCharging) && !bCompletingFarmAction)
	{
		const FPSItemStack* Current = InventoryComponent ? InventoryComponent->FindHotbarSlot(FarmActionSlot) : nullptr;
		if (!Current || Current->ItemType != FarmActionItem.ItemType || Current->ItemId != FarmActionItem.ItemId
			|| Current->Quantity != FarmActionItem.Quantity || Current->CurrentDurability != FarmActionItem.CurrentDurability)
			CancelFarmAction();
	}
	EPSEquipment NewEquipment = EPSEquipment::BareHands;
	const FPSItemStack* Stack = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	if (Stack && !Stack->IsEmpty())
	{
		switch (Stack->ItemType)
		{
		case EPSItemType::Hoe: NewEquipment = EPSEquipment::Hoe; break;
		case EPSItemType::TestSeed: NewEquipment = EPSEquipment::Seed; break;
		case EPSItemType::FishingRod: NewEquipment = EPSEquipment::FishingRod; break;
		case EPSItemType::Pickaxe: NewEquipment = EPSEquipment::Pickaxe; break;
		case EPSItemType::Shovel: NewEquipment = EPSEquipment::Shovel; break;
		case EPSItemType::WateringCan: NewEquipment = EPSEquipment::WateringCan; break;
		case EPSItemType::Axe: NewEquipment = EPSEquipment::Axe; break;
		case EPSItemType::TreeSeed: NewEquipment = EPSEquipment::TreeSeed; break;
		default: NewEquipment = EPSEquipment::UnusableItem; break;
		}
	}
	SetEquipment(NewEquipment);
}

void APSPlayerController::HandleFishingRod()
{
	if (GatheringAction->IsGatheringActive()) return;
	if (bFarmActionActive || IsPaused()) return;
	if (FishingState == EPSFishingState::Failure || (FishingState == EPSFishingState::Success && bFishingSuccessReleased)) StopFishing(false);
	if (FishingState == EPSFishingState::WaitingForBite)
	{
		BeginFishingRetrieve();
		return;
	}
	if (FishingState == EPSFishingState::BiteWindow)
	{
		BeginFishingMinigame();
		return;
	}
	if (IsFishing()) return;
	const TOptional<FIntPoint> Target = GetCursorCell();
	if (!Target.IsSet() || !TryUseFishingRod(Target.GetValue()))
	{
		ShowFishingUnavailable(NSLOCTEXT("Fishing", "InvalidTarget", "물가에서 가까운 물을 향해 사용해 주세요"), EPSFishingNoticeType::WaterRequired);
	}
}

void APSPlayerController::ShowFishingUnavailable(const FText& Reason, const EPSFishingNoticeType Type)
{
	if (FishingWidget) FishingWidget->ShowNotice(Reason, Type);
	PlayFishingSound(FishingUnavailableSound);
	if (APSPlayerCharacter* FishingCharacter = Cast<APSPlayerCharacter>(GetPawn())) FishingCharacter->PlayFishingStateAnimation(EPSFishingState::Failure);
	OnFishingUnavailable(Reason);
}

bool APSPlayerController::TryUseFishingRod(const FIntPoint WaterCell)
{
	if (GatheringAction->IsGatheringActive()) return false;
	UpdateFishing(0.0f);
	if (FishingState == EPSFishingState::Failure || (FishingState == EPSFishingState::Success && bFishingSuccessReleased)) StopFishing(false);
	if (IsFishing() || IsPaused() || bInventoryOpen || bJournalOpen) return false;
	const APawn* PlayerPawn = GetPawn();
	if (Equipment != EPSEquipment::FishingRod || !IsValid(GridWorld) || !PlayerPawn
		|| !PlayerPawn->GetVelocity().IsNearlyZero()
		|| !GridWorld->CanFishFrom(GridWorld->WorldToCell(PlayerPawn->GetActorLocation()), WaterCell)) return false;
	const FPSItemStack* Rod = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	if (!Rod || Rod->ItemType != EPSItemType::FishingRod) return false;
	if (PSItems::UsesDurability(*Rod) && Rod->CurrentDurability <= 0) return false;
	const UPSGameTimeSubsystem* GameTime = GetWorld() ? GetWorld()->GetSubsystem<UPSGameTimeSubsystem>() : nullptr;
	FishingSeason = GameTime ? GameTime->GetSeasonIndex() : 0;
	const FPSItemDefinition& RodDefinition = PSItems::GetDefinition(*Rod);
	FishingMinigameDuration = FMath::Max(0.1f, MinigameDuration) * (1.0f + RodDefinition.FishingTimeBonusRatio);
	FishingRodSnapshot = *Rod;
	FishingRodSlot = SelectedHotbarSlot;
	bFishingAutoHook = InventoryComponent->HasItem(EPSItemType::FishingBobber);
	bFishingDurabilityConsumed = false;
	bFishingBait = InventoryComponent->HasItem(EPSItemType::FishingBait);
	const float MaxDelay = FMath::Max(0.1f, MaxBiteDelay) * (bFishingBait ? 0.95f : 1.0f);
	FishingBiteDelay = FMath::FRandRange(FMath::Clamp(MinBiteDelay, 0.1f, MaxDelay), MaxDelay);
	FishingCastTravelSeconds = FMath::Max(0.01f, CastTravelSeconds);
	FishingBands = CastBands;
	FishingCastDefinitions.Reset();
	for (const EPSFishingCastGrade Grade : {EPSFishingCastGrade::Green, EPSFishingCastGrade::Yellow, EPSFishingCastGrade::Red})
		FishingCastDefinitions.Add(GetFishingCastDefinition(Grade));
	FishingAimCell = WaterCell;
	FishingCell = WaterCell;
	FishingPawn = GetPawn();
	FishingStartLocation = PlayerPawn->GetActorLocation();
	CurrentFishIcon = nullptr;
	CurrentFishId = NAME_None;
	CurrentFishSizeCm = 0;
	FishingSequence.Reset();
	FishingSequenceIndex = 0;
	FishingCastElapsed = 0.0f;
	FishingState = EPSFishingState::CastWindup;
	FishingStateDuration = FMath::Max(0.01f, CastWindupDuration);
	SetFishingMovementLocked(true);
	const FVector Facing = GridWorld->CellToWorldCenter(WaterCell) - PlayerPawn->GetActorLocation();
	FishingCastDirection = Facing.GetSafeNormal2D();
	if (APSPlayerCharacter* PlayerCharacter = Cast<APSPlayerCharacter>(GetPawn()))
	{
		const float AnimationDuration = PlayerCharacter->BeginFishingCastPreparation(Facing);
		if (AnimationDuration > 0) FishingStateDuration = AnimationDuration;
	}
	else GetPawn()->SetActorRotation(FRotator(0, Facing.Rotation().Yaw, 0));
	FishingStateTimeRemaining = FishingStateDuration;
	OnFishingCastStarted();
	RefreshFishingWidget();
	return true;
}

float APSPlayerController::GetFishingCastPosition() const
{
	return PSFishing::GetCastPosition(FishingCastElapsed, FishingCastTravelSeconds);
}

FPSFishingCastDefinition APSPlayerController::GetFishingCastDefinition(const EPSFishingCastGrade Grade) const
{
	if (FishingCastDataTable && FishingCastDataTable->GetRowStruct() == FPSFishingCastDefinition::StaticStruct())
		if (const FPSFishingCastDefinition* Row = FishingCastDataTable->FindRow<FPSFishingCastDefinition>(
			PSFishing::GetCastRowName(Grade), TEXT("Fishing cast"), false))
			if (Row->IsUsable()) return *Row;
	return PSFishing::GetDefaultCastDefinition(Grade);
}

TOptional<FIntPoint> APSPlayerController::ResolveFishingLandingCell(const int32 DistanceInCells) const
{
	if (!IsValid(GridWorld) || !GetPawn() || DistanceInCells < 1) return {};
	const FVector Origin = GetPawn()->GetActorLocation();
	const FVector Direction = FishingCastDirection;
	if (Direction.IsNearlyZero()) return {};
	const double CellSize = GridWorld->GetCellSize();
	const FVector Target = Origin + Direction * (CellSize * DistanceInCells);
	const FIntPoint TargetCell = GridWorld->WorldToCell(Target);
	// The bobber ignores everything along its path. Only the destination needs water.
	if (GridWorld->GetGroundTile(TargetCell) == EPSTileType::Water) return TargetCell;
	constexpr int32 SearchRadiusInCells = 6;
	const double RadiusSquared = FMath::Square(CellSize * SearchRadiusInCells);
	const FIntPoint PlayerCell = GridWorld->WorldToCell(Origin);
	TOptional<FIntPoint> BestWater;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (int32 Y = -SearchRadiusInCells; Y <= SearchRadiusInCells; ++Y)
		for (int32 X = -SearchRadiusInCells; X <= SearchRadiusInCells; ++X)
	{
		const FIntPoint Cell = PlayerCell + FIntPoint(X, Y);
		const FVector Center = GridWorld->CellToWorldCenter(Cell);
		if (FVector::DistSquared2D(Origin, Center) > RadiusSquared
			|| GridWorld->GetGroundTile(Cell) != EPSTileType::Water) continue;
		const double DistanceSquared = FVector::DistSquared2D(Target, Center);
		// Stable row-major iteration resolves ties without randomness or map iteration order.
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestWater = Cell;
		}
	}
	return BestWater;
}

bool APSPlayerController::ConfirmFishingCast()
{
	if (IsPaused() || FishingState != EPSFishingState::CastTiming) return false;
	UpdateFishing(0.0f);
	if (FishingState != EPSFishingState::CastTiming) return false;
	FishingCastGrade = PSFishing::GetCastGrade(GetFishingCastPosition(), FishingBands.GetEdges());
	const int32 GradeIndex = static_cast<int32>(FishingCastGrade);
	const FPSFishingCastDefinition& Definition = FishingCastDefinitions[GradeIndex];
	const TOptional<FIntPoint> Landing = ResolveFishingLandingCell(Definition.DistanceInCells);
	if (!Landing.IsSet())
	{
		StopFishing(true, false);
		ShowFishingUnavailable(NSLOCTEXT("Fishing", "NoLanding", "찌가 닿을 곳에 물이 없어요"));
		return false;
	}
	CurrentFish = SelectFishDefinition(CurrentFishId, FishingSeason, GridWorld->GetFishingLocationId(Landing.GetValue()), FishingCastGrade);
	if (CurrentFishId.IsNone())
	{
		StopFishing(true, false);
		ShowFishingUnavailable(NSLOCTEXT("Fishing", "NoFish", "지금 이곳에서 잡을 수 있는 물고기가 없어요"));
		return false;
	}
	if (bFishingBait && !InventoryComponent->RemoveItem(EPSItemType::FishingBait, 1))
	{
		StopFishing(true, false);
		ShowFishingUnavailable(NSLOCTEXT("Fishing", "MissingBait", "선택한 미끼가 없어 다시 준비해야 해요"));
		return false;
	}
	FishingCell = Landing;
	FishingState = EPSFishingState::WaitingForBite;
	FishingStateDuration = FishingBiteDelay;
	FishingStateTimeRemaining = FishingStateDuration;
	if (APSPlayerCharacter* PlayerCharacter = Cast<APSPlayerCharacter>(GetPawn())) PlayerCharacter->PlayFishingCastThrow();
	FishingVisualElapsed = 0;
	FishingFlightDuration = FMath::IsFinite(CastFlightSeconds) ? FMath::Max(.01f, CastFlightSeconds) : .6f;
	if (IsLocalController() && GetWorld())
	{
		UClass* VisualClass = FishingBobberClass.Get();
		FishingBobber = GetWorld()->SpawnActor<APSFishingBobber>(VisualClass ? VisualClass : APSFishingBobber::StaticClass());
		UpdateFishingVisual(0);
	}
	PlayFishingSound(FishingCastSound);
	OnFishingRodUsed(Landing.GetValue());
	RefreshFishingWidget();
	return true;
}

bool APSPlayerController::GetFishingFocusLocation(FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	const bool bFocus = FishingState == EPSFishingState::WaitingForBite
		|| FishingState == EPSFishingState::BiteWindow || FishingState == EPSFishingState::Minigame;
	if (!bFocus || !FishingCell.IsSet() || !IsValid(GridWorld) || !GetPawn() || FishingPawn.Get() != GetPawn()) return false;
	OutLocation = GridWorld->CellToWorldCenter(FishingCell.GetValue()) + FVector(0, 0, 8);
	return true;
}

void APSPlayerController::UpdateFishing(const float DeltaSeconds)
{
	if (!IsFishing()) return;
	if (IsPaused()) return;
	const APawn* PlayerPawn = GetPawn();
	if (FishingState == EPSFishingState::Retrieving && (!PlayerPawn || FishingPawn.Get() != PlayerPawn || !IsValid(GridWorld)))
	{
		StopFishing(false, false);
		return;
	}
	const bool bPreparing = FishingState == EPSFishingState::CastWindup || FishingState == EPSFishingState::CastTiming;
	const bool bActiveCast = bPreparing || FishingState == EPSFishingState::WaitingForBite
		|| FishingState == EPSFishingState::BiteWindow || FishingState == EPSFishingState::Minigame;
	const FPSItemStack* Rod = InventoryComponent ? InventoryComponent->FindHotbarSlot(FishingRodSlot) : nullptr;
	const EPSTileType PlayerGround = IsValid(GridWorld) && PlayerPawn
		? GridWorld->GetGroundTile(GridWorld->WorldToCell(PlayerPawn->GetActorLocation())) : EPSTileType::Empty;
	if (bActiveCast && (!PlayerPawn || FishingPawn.Get() != PlayerPawn || !IsValid(GridWorld)
		|| Equipment != EPSEquipment::FishingRod
		|| !Rod || Rod->ItemType != FishingRodSnapshot.ItemType || Rod->ItemId != FishingRodSnapshot.ItemId
		|| Rod->CurrentDurability != FishingRodSnapshot.CurrentDurability || SelectedHotbarSlot != FishingRodSlot
		|| !PlayerPawn->GetActorLocation().Equals(FishingStartLocation, 0.01f)
		|| PlayerGround == EPSTileType::Water || PlayerGround == EPSTileType::Empty
		|| (!bPreparing && GridWorld->GetGroundTile(FishingCell.GetValue()) != EPSTileType::Water)))
	{
		StopFishing(false);
		return;
	}
	// The bite wait starts at impact; carry only the remainder of a landing frame.
	const float FlightRemaining = FishingState == EPSFishingState::WaitingForBite
		? FMath::Max(0.f, FishingFlightDuration - FishingVisualElapsed) : 0.f;
	const float TimerDeltaSeconds = FMath::Max(0.f, DeltaSeconds - FlightRemaining);
	UpdateFishingVisual(DeltaSeconds);
	if (FishingState == EPSFishingState::Success && DeltaSeconds > 0)
	{
		FishingResultElapsed += DeltaSeconds;
		FishingStateTimeRemaining = FMath::Max(0.f, FishingStateTimeRemaining - DeltaSeconds);
		const APSPlayerCharacter* FishingCharacter = Cast<APSPlayerCharacter>(GetPawn());
		const bool bMotionEnded = bFishingTerminalAnimation
			? !FishingCharacter || !FishingCharacter->IsFishingStateAnimationPlaying() : FishingStateTimeRemaining <= 0;
		if (!bFishingSuccessReleased && bMotionEnded)
		{
			SetFishingMovementLocked(false);
			bFishingSuccessReleased = true;
		}
		if (bFishingSuccessReleased && FishingResultElapsed >= FMath::Max(.1f, FishingResultSeconds)) StopFishing(false);
		else RefreshFishingWidget();
		return;
	}
	if (FishingState == EPSFishingState::Retrieving)
	{
		if (DeltaSeconds > 0)
		{
			FishingStateTimeRemaining = FMath::Max(0.f, FishingStateTimeRemaining - DeltaSeconds);
			const APSPlayerCharacter* FishingCharacter = Cast<APSPlayerCharacter>(GetPawn());
			const bool bMotionEnded = bFishingTerminalAnimation
				? !FishingCharacter || !FishingCharacter->IsFishingStateAnimationPlaying() : FishingStateTimeRemaining <= 0;
			if (bMotionEnded) StopFishing(true, false);
		}
		RefreshFishingWidget();
		return;
	}

	if (DeltaSeconds > 0.0f)
	{
		if (FishingState == EPSFishingState::CastTiming)
		{
			FishingCastElapsed = FMath::Fmod(FishingCastElapsed + DeltaSeconds, FishingCastTravelSeconds * 2.0f);
			RefreshFishingWidget();
			return;
		}
		FishingStateTimeRemaining = FMath::Max(0.0f, FishingStateTimeRemaining - TimerDeltaSeconds);
		if (FishingState == EPSFishingState::CastWindup)
			if (APSPlayerCharacter* PlayerCharacter = Cast<APSPlayerCharacter>(GetPawn()))
				PlayerCharacter->UpdateFishingCastPreparation(FishingStateDuration - FishingStateTimeRemaining);
		if (FishingStateTimeRemaining <= 0.0f)
		{
			switch (FishingState)
			{
			case EPSFishingState::CastWindup: FishingState = EPSFishingState::CastTiming; FishingCastElapsed = 0.0f; break;
			case EPSFishingState::WaitingForBite: BeginFishingBite(); break;
			case EPSFishingState::BiteWindow:
			case EPSFishingState::Minigame: CompleteFishing(false); break;
			case EPSFishingState::Success:
			case EPSFishingState::Failure: StopFishing(false); break;
			default: break;
			}
		}
	}
	RefreshFishingWidget();
}

void APSPlayerController::BeginFishingBite()
{
	if (FishingState != EPSFishingState::WaitingForBite) return;
	FishingState = EPSFishingState::BiteWindow;
	FishingStateDuration = BiteWindowDuration;
	FishingStateTimeRemaining = FishingStateDuration;
	PlayFishingSound(FishingBiteSound);
	OnFishingBite();
	if (bFishingAutoHook)
	{
		BeginFishingMinigame();
		if (InventoryComponent)
		{
			bool bDestroyed = false;
			InventoryComponent->ConsumeItemDurability(EPSItemType::FishingBobber, 1, bDestroyed);
		}
	}
}

void APSPlayerController::BeginFishingMinigame()
{
	if (FishingState != EPSFishingState::BiteWindow) return;
	FishingState = EPSFishingState::Minigame;
	FishingStateDuration = FishingMinigameDuration;
	FishingStateTimeRemaining = FishingStateDuration;
	GenerateFishingSequence();
	if (APSPlayerCharacter* FishingCharacter = Cast<APSPlayerCharacter>(GetPawn())) FishingCharacter->PlayFishingStateAnimation(EPSFishingState::Minigame);
	OnFishingMinigameStarted();
}

void APSPlayerController::GenerateFishingSequence()
{
	FishingSequence.Reset();
	FishingSequenceIndex = 0;
	const FIntPoint Range = PSFishing::GetInputCountRange(CurrentFish.Difficulty);
	const int32 Count = FMath::RandRange(Range.X, Range.Y);
	FishingSequence.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
		FishingSequence.Add(static_cast<EPSFishingDirection>(FMath::RandRange(0, 3)));
}

bool APSPlayerController::SubmitFishingDirection(const EPSFishingDirection Direction)
{
	if (FishingState != EPSFishingState::Minigame || !FishingSequence.IsValidIndex(FishingSequenceIndex)) return false;
	if (FishingSequence[FishingSequenceIndex] != Direction)
	{
		FishingStateTimeRemaining = FMath::Max(0.0f, FishingStateTimeRemaining - WrongInputTimePenalty);
		if (FishingStateTimeRemaining <= 0.0f) CompleteFishing(false);
		else
		{
			RefreshFishingWidget();
			if (FishingWidget) FishingWidget->ShowWrongInput();
		}
		return false;
	}
	++FishingSequenceIndex;
	if (FishingSequenceIndex >= FishingSequence.Num()) CompleteFishing(true);
	else RefreshFishingWidget();
	return true;
}

void APSPlayerController::HandleFishingUp() { SubmitFishingDirection(EPSFishingDirection::Up); }
void APSPlayerController::HandleFishingLeft() { SubmitFishingDirection(EPSFishingDirection::Left); }
void APSPlayerController::HandleFishingDown() { SubmitFishingDirection(EPSFishingDirection::Down); }
void APSPlayerController::HandleFishingRight() { SubmitFishingDirection(EPSFishingDirection::Right); }

FPSFishDefinition APSPlayerController::SelectFishDefinition(FName& OutFishId, const int32 Season, const int32 LocationId,
	const EPSFishingCastGrade Grade) const
{
	OutFishId = NAME_None;
	TArray<FPSFishDefinition> Fish;
	TArray<FName> FishIds;
	bool bHasUsableTableRows = false;
	if (FishDataTable)
	{
		if (FishDataTable->GetRowStruct() != FPSFishDefinition::StaticStruct())
		{
			UE_LOG(LogTemp, Error, TEXT("FishDataTable must use FPSFishDefinition rows."));
		}
		else
		{
			for (const TPair<FName, uint8*>& Pair : FishDataTable->GetRowMap())
			{
				if (!Pair.Value) continue;
				FPSFishDefinition Entry = *reinterpret_cast<const FPSFishDefinition*>(Pair.Value);
				const bool bAvailableNow = Entry.Season == Season
					&& (Entry.Location.IsEmpty() || Entry.Location.Contains(LocationId));
				bHasUsableTableRows |= Entry.IsUsable();
				if (Entry.IsUsable() && bAvailableNow)
				{
					FishIds.Add(Pair.Key);
					Fish.Add(MoveTemp(Entry));
				}
				else if (!Entry.IsUsable()) UE_LOG(LogTemp, Warning, TEXT("Ignoring invalid fish row: %s"), *Pair.Key.ToString());
			}
		}
	}
	if (Fish.IsEmpty())
	{
		if (bHasUsableTableRows) return FPSFishDefinition();
		Fish = GetFallbackFish(FishIds);
	}
	const FPSFishingCastDefinition Definition = FishingCastDefinitions.IsValidIndex(static_cast<int32>(Grade))
		? FishingCastDefinitions[static_cast<int32>(Grade)] : GetFishingCastDefinition(Grade);
	double TierTotals[4] = {};
	for (const FPSFishDefinition& Entry : Fish)
		if (FMath::IsFinite(Entry.Weight) && Entry.Weight > 0) TierTotals[Entry.Difficulty - 1] += Entry.Weight;
	double TotalTierWeight = 0;
	for (int32 Tier = 0; Tier < 4; ++Tier)
		if (TierTotals[Tier] > 0) TotalTierWeight += Definition.DifficultyWeights[Tier];
	if (TotalTierWeight <= 0) return FPSFishDefinition();
	double TierRoll = FMath::FRand() * TotalTierWeight;
	int32 ChosenTier = INDEX_NONE;
	for (int32 Tier = 0; Tier < 4; ++Tier)
	{
		if (TierTotals[Tier] <= 0 || Definition.DifficultyWeights[Tier] <= 0) continue;
		ChosenTier = Tier;
		TierRoll -= Definition.DifficultyWeights[Tier];
		if (TierRoll < 0) break;
	}
	double Roll = FMath::FRand() * TierTotals[ChosenTier];
	int32 LastEligible = INDEX_NONE;
	for (int32 Index = 0; Index < Fish.Num(); ++Index)
	{
		const FPSFishDefinition& Entry = Fish[Index];
		if (Entry.Difficulty != ChosenTier + 1 || !FMath::IsFinite(Entry.Weight) || Entry.Weight <= 0) continue;
		LastEligible = Index;
		Roll -= Entry.Weight;
		if (Roll < 0)
		{
			OutFishId = FishIds[Index];
			return Entry;
		}
	}
	OutFishId = FishIds[LastEligible];
	return Fish[LastEligible];
}

UTexture2D* APSPlayerController::ResolveFishIcon(const FName IconID) const
{
	if (!FishIconDataTable || IconID.IsNone()
		|| FishIconDataTable->GetRowStruct() != FPSIconDefinition::StaticStruct()) return nullptr;
	if (const TWeakObjectPtr<UTexture2D>* Cached = FishIconCache.Find(IconID))
		if (Cached->IsValid()) return Cached->Get();
	const FPSIconDefinition* Icon = FishIconDataTable->FindRow<FPSIconDefinition>(
		IconID, TEXT("Fishing icon lookup"), false);
	UTexture2D* Texture = Icon ? Icon->Image.LoadSynchronous() : nullptr;
	if (Texture) FishIconCache.Add(IconID, Texture);
	return Texture;
}

const FPSFishDefinition* APSPlayerController::FindFishDefinition(const FName FishId) const
{
	if (!FishDataTable || FishId.IsNone() || FishDataTable->GetRowStruct() != FPSFishDefinition::StaticStruct()) return nullptr;
	return FishDataTable->FindRow<FPSFishDefinition>(FishId, TEXT("Fishing item lookup"), false);
}

FText APSPlayerController::GetItemDisplayName(const FPSItemStack& Item) const
{
	FPSItemTableRow Data;
	if (Item.ItemType != EPSItemType::Fish && GridWorld && GridWorld->Gathering && UPSGameDataLibrary::FindItem(GridWorld->Gathering->ItemTable, Item.ItemId, Data) && !Data.Name.IsEmpty()) return Data.Name;
	if (Item.ItemType == EPSItemType::Fish)
		if (const FPSFishDefinition* Fish = FindFishDefinition(Item.ItemId)) return Fish->Name;
	return PSItems::GetDefinition(Item).Name;
}

FText APSPlayerController::GetItemDescription(const FPSItemStack& Item) const
{
	FPSItemTableRow Data;
	if (Item.ItemType != EPSItemType::Fish && GridWorld && GridWorld->Gathering && UPSGameDataLibrary::FindItem(GridWorld->Gathering->ItemTable, Item.ItemId, Data) && !Data.Description.IsEmpty()) return Data.Description;
	if (Item.ItemType != EPSItemType::Fish) return PSItems::GetDefinition(Item).Description;
	const FPSFishDefinition* Fish = FindFishDefinition(Item.ItemId);
	if (!Fish) return PSItems::GetDefinition(Item).Description;
	FString Text = Fish->Description.ToString();
	if (HasMasteredFish(Item.ItemId) && !Fish->DescriptionPlus.IsEmpty())
	{
		if (!Text.IsEmpty()) Text += TEXT("\n\n");
		Text += Fish->DescriptionPlus.ToString();
	}
	if (FishingJournalComponent)
	{
		const FPSFishJournalRecord Record = FishingJournalComponent->GetRecord(Item.ItemId);
		if (Record.IsDiscovered())
		{
			Text += FString::Printf(TEXT("\n\n포획 기록  %d마리\n최대 크기  %dcm"), Record.TimesCaught, Record.LargestSizeCm);
		}
	}
	return FText::FromString(Text);
}

UTexture2D* APSPlayerController::GetItemIcon(const FPSItemStack& Item) const
{
	if (Item.ItemType == EPSItemType::Fish)
	{
		const FPSFishDefinition* Fish = FindFishDefinition(Item.ItemId);
		return Fish ? ResolveFishIcon(Fish->IconID) : nullptr;
	}
	const UPSGatheringWorldComponent* Gathering = GridWorld ? GridWorld->Gathering.Get() : nullptr;
	return Gathering ? Gathering->GetItemIcon(Item) : nullptr;
}

bool APSPlayerController::CancelFishingForMovementInput()
{
	if (FishingState != EPSFishingState::WaitingForBite && FishingState != EPSFishingState::BiteWindow
		&& FishingState != EPSFishingState::CastWindup && FishingState != EPSFishingState::CastTiming) return false;
	if (FishingState == EPSFishingState::CastWindup || FishingState == EPSFishingState::CastTiming) StopFishing(true);
	else BeginFishingRetrieve();
	return true;
}

void APSPlayerController::CompleteFishing(const bool bSuccess)
{
	if (FishingState != EPSFishingState::BiteWindow && FishingState != EPSFishingState::Minigame) return;
	if (bSuccess)
	{
		CurrentFishIcon = ResolveFishIcon(CurrentFish.IconID);
		CurrentFishSizeCm = FMath::RandRange(FMath::Min(CurrentFish.MinSize, CurrentFish.MaxSize),
			FMath::Max(CurrentFish.MinSize, CurrentFish.MaxSize));
		const int32 RewardQuantity = 1;
		if (!InventoryComponent || !InventoryComponent->AddItemVariant(
			EPSItemType::Fish, CurrentFishId, RewardQuantity)) DropFishingReward(RewardQuantity);
		if (FishingJournalComponent) FishingJournalComponent->RecordCatch(CurrentFishId, CurrentFishSizeCm, RewardQuantity, CurrentFish.MaxSize);
		if (SkillComponent) SkillComponent->AddExperience(EPSPlayerSkillField::Fishing, 2);
		FishingState = EPSFishingState::Success;
		FishingStateDuration = 1.5f;
		PlayFishingSound(FishingSuccessSound);
		OnFishingSucceeded(CurrentFish.Name, CurrentFishSizeCm);
	}
	else
	{
		FishingState = EPSFishingState::Failure;
		FishingStateDuration = FMath::Max(.1f, FishingResultSeconds);
		PlayFishingSound(FishingFailureSound);
		OnFishingFailed();
	}
	FishingResultElapsed = 0;
	bFishingSuccessReleased = false;
	bFishingTerminalAnimation = false;
	if (APSPlayerCharacter* FishingCharacter = Cast<APSPlayerCharacter>(GetPawn()))
	{
		const float MotionDuration = FishingCharacter->PlayFishingStateAnimation(FishingState);
		bFishingTerminalAnimation = bSuccess && MotionDuration > 0;
		if (bFishingTerminalAnimation) FishingStateDuration = MotionDuration;
	}
	if (IsValid(FishingBobber)) FishingBobber->Destroy();
	FishingBobber = nullptr;
	if (bSuccess && CurrentFishIcon && IsLocalController() && GetPawn())
	{
		FishingBobber = GetWorld()->SpawnActor<APSFishingBobber>();
		if (FishingBobber)
		{
			FishingBobber->ShowCatch(CurrentFishIcon);
			UpdateFishingVisual(0);
		}
	}
	FishingStateTimeRemaining = FishingStateDuration;
	ConsumeFishingRodDurability();
	// Success retains the lock through its motion; failure restores control immediately.
	if (!bSuccess)
	{
		if (APSPlayerCharacter* FishingCharacter = Cast<APSPlayerCharacter>(GetPawn())) FishingCharacter->SetMovementLocked(false);
	}
	RefreshFishingWidget();
}

void APSPlayerController::DropFishingReward(const int32 Quantity)
{
	if (!GetWorld() || !GetPawn()) return;
	const FTransform SpawnTransform(FRotator::ZeroRotator, GetPawn()->GetActorLocation() + FVector(45, 0, 20));
	APSWorldItemActor* Drop = GetWorld()->SpawnActorDeferred<APSWorldItemActor>(
		APSWorldItemActor::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Drop)
	{
		FPSItemStack Stack;
		Stack.ItemType = EPSItemType::Fish;
		Stack.ItemId = CurrentFishId;
		Stack.Quantity = FMath::Max(1, Quantity);
		Drop->InitializeItem(Stack, GridWorld);
		Drop->FinishSpawning(SpawnTransform);
	}
}

void APSPlayerController::SetFishingMovementLocked(const bool bLocked)
{
	APawn* PawnToUpdate = FishingPawn.Get();
	if (!PawnToUpdate) PawnToUpdate = GetPawn();
	if (APSPlayerCharacter* PlayerCharacter = Cast<APSPlayerCharacter>(PawnToUpdate))
	{
		if (!bLocked) PlayerCharacter->EndFishingCastAnimation();
		PlayerCharacter->SetMovementLocked(bLocked);
	}
}

bool APSPlayerController::AreFishingControlsLocked() const
{
	return FishingState == EPSFishingState::WaitingForBite || FishingState == EPSFishingState::BiteWindow
		|| FishingState == EPSFishingState::Minigame || (FishingState == EPSFishingState::Success && !bFishingSuccessReleased)
		|| FishingState == EPSFishingState::CastWindup || FishingState == EPSFishingState::CastTiming
		|| FishingState == EPSFishingState::Retrieving;
}

void APSPlayerController::RefreshFishingWidget()
{
	if (!FishingWidget) return;
	float Opacity = 1.0f;
	if (FishingState == EPSFishingState::Success && FishingResultElapsed >= FMath::Max(.1f, FishingResultSeconds)) Opacity = 0;
	FishingWidget->Refresh(FishingState, FishingStateTimeRemaining, FishingStateDuration,
		FishingSequence, FishingSequenceIndex, CurrentFish.Name, CurrentFishSizeCm, GetCurrentFishIcon(), Opacity,
		GetFishingCastPosition(), -1, -1, FishingBands);
}

void APSPlayerController::ConsumeFishingRodDurability()
{
	if (bFishingDurabilityConsumed) return;
	bFishingDurabilityConsumed = true;
	if (!InventoryComponent) return;
	const FPSItemStack* Rod = InventoryComponent->FindHotbarSlot(FishingRodSlot);
	if (!Rod || Rod->ItemType != EPSItemType::FishingRod || Rod->ItemId != FishingRodSnapshot.ItemId
		|| Rod->CurrentDurability != FishingRodSnapshot.CurrentDurability) return;
	bool bDestroyed = false;
	bEndingFishing = true;
	InventoryComponent->ConsumeHotbarDurability(FishingRodSlot, 1, bDestroyed);
	bEndingFishing = false;
}

void APSPlayerController::StopFishing(const bool bCancelled, const bool bConsumeDurability)
{
	if (IsValid(FishingBobber)) FishingBobber->Destroy();
	FishingBobber = nullptr;
	bFishingTerminalAnimation = false;
	const bool bWasFishing = IsFishing();
	const bool bActiveCast = FishingState == EPSFishingState::WaitingForBite
		|| FishingState == EPSFishingState::BiteWindow || FishingState == EPSFishingState::Minigame;
	if (bWasFishing && bActiveCast && bConsumeDurability) ConsumeFishingRodDurability();
	SetFishingMovementLocked(false);
	FishingCell.Reset();
	FishingPawn.Reset();
	FishingSequence.Reset();
	FishingSequenceIndex = 0;
	FishingState = EPSFishingState::Idle;
	FishingStateDuration = 0.0f;
	FishingStateTimeRemaining = 0.0f;
	FishingMinigameDuration = 10.0f;
	FishingCastElapsed = 0.0f;
	FishingRodSlot = INDEX_NONE;
	bFishingAutoHook = false;
	CurrentFishIcon = nullptr;
	if (bWasFishing && bCancelled) OnFishingCancelled();
	RefreshFishingWidget();
}

void APSPlayerController::SetEquipment(const EPSEquipment InEquipment)
{
	if (Equipment != InEquipment && !bEndingFishing && IsFishing()) StopFishing();
	Equipment = InEquipment;
	const FPSItemStack* Stack = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	if (StatusWidget) StatusWidget->SetEquipment(Equipment, SelectedHotbarSlot, Stack ? Stack->Quantity : 0, GetEquippedFarmChargeDepth());
}

void APSPlayerController::HandleInventory()
{
	BushInteraction->Cancel();
	GatheringAction->Cancel();
	if (bJournalOpen) { CloseFishingJournal(); return; }
	if (bFarmActionActive) return;
	if (bFarmCharging) CancelFarmAction();
	if (AreFishingControlsLocked()) return;
	if (bInventoryOpen)
	{
		CloseInventory();
		return;
	}
	if (!IsLocalController()) return;
	if (!InventoryWidget)
	{
		UClass* Class = InventoryWidgetClass.Get();
		InventoryWidget = CreateWidget<UPSInventoryWidget>(this, Class ? Class : UPSInventoryWidget::StaticClass());
		if (InventoryWidget) InventoryWidget->SetInventoryComponent(InventoryComponent);
	}
	if (!InventoryWidget) return;
	if (HotbarWidget) InventoryWidget->AttachHotbar(HotbarWidget);
	InventoryWidget->AddToPlayerScreen(100);
	bInventoryOpen = true;
	// Temporary single-player menu policy; never unpause a pause owned by another system.
	bInventoryOwnsPause = !IsPaused() && GetNetMode() == NM_Standalone && SetPause(true);
	FlushPressedKeys();
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	OnInventoryRequested();
}

void APSPlayerController::CloseInventory()
{
	if (!bInventoryOpen) return;
	if (FishingJournalWidget) FishingJournalWidget->RemoveFromParent();
	bJournalOpen = false;
	if (InventoryWidget) InventoryWidget->SetVisibility(ESlateVisibility::Visible);
	UWidgetBlueprintLibrary::CancelDragDrop();
	if (InventoryWidget) InventoryWidget->DetachHotbar();
	if (InventoryWidget) InventoryWidget->RemoveFromParent();
	if (HotbarWidget && !HotbarWidget->IsInViewport()) HotbarWidget->AddToViewport(300);
	bInventoryOpen = false;
	if (bInventoryOwnsPause) SetPause(false);
	bInventoryOwnsPause = false;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	UWidgetBlueprintLibrary::SetFocusToGameViewport();
	FlushPressedKeys();
}

void APSPlayerController::HandleQuest()
{
	BushInteraction->Cancel();
	if (GatheringAction->IsGatheringActive()) return;
	if (bFarmActionActive) return;
	if (bFarmCharging) CancelFarmAction();
	if (AreFishingControlsLocked()) return;
	UE_LOG(LogTemp, Log, TEXT("Quest requested"));
	OnQuestRequested();
}

void APSPlayerController::HandleAbility()
{
	BushInteraction->Cancel();
	if (GatheringAction->IsGatheringActive()) return;
	if (bFarmActionActive) return;
	if (bFarmCharging) CancelFarmAction();
	if (AreFishingControlsLocked()) return;
	UE_LOG(LogTemp, Log, TEXT("Ability requested"));
	OnAbilityRequested();
}

void APSPlayerController::HandleCraft()
{
	BushInteraction->Cancel();
	if (GatheringAction->IsGatheringActive()) return;
	if (bFarmActionActive) return;
	if (bFarmCharging) CancelFarmAction();
	if (AreFishingControlsLocked()) return;
	UE_LOG(LogTemp, Log, TEXT("Craft requested"));
	OnCraftRequested();
}

void APSPlayerController::HandleResetWorld()
{
	CancelCropHarvest();
	GatheringAction->Cancel();
	CancelFarmAction();
	if (!GridWorld)
	{
		return;
	}

	const bool bResetSucceeded = GridWorld->ResetWorld();
	if (bResetSucceeded)
	{
		StopFishing();
		for (TActorIterator<APSWorldItemActor> It(GetWorld()); It; ++It)
		{
			It->Destroy();
		}
		if (InventoryComponent)
		{
			InventoryComponent->RestoreAllDurability();
			constexpr int32 ResetLettuceStock = 100;
			const int32 Owned = InventoryComponent->CountItem(EPSItemType::TestCrop, PSItemIds::LettuceCropId);
			if (Owned < ResetLettuceStock)
			{
				FPSItemStack Lettuce;
				Lettuce.ItemType = EPSItemType::TestCrop;
				Lettuce.ItemId = PSItemIds::Lettuce;
				Lettuce.CropId = PSItemIds::LettuceCropId;
				Lettuce.Quantity = ResetLettuceStock - Owned;
				if (!InventoryComponent->AddItemVariant(Lettuce.ItemType, Lettuce.ItemId, Lettuce.Quantity, Lettuce.CropId))
				{
					const FIntPoint Cell = GetPawn() ? GridWorld->WorldToCell(GetPawn()->GetActorLocation()) : FIntPoint::ZeroValue;
					GridWorld->Gathering->DropItemStack(Cell, Lettuce);
				}
			}
		}
		HarvestedCropCount = 0;
		if (StatusWidget) StatusWidget->SetHarvestedCropCount(HarvestedCropCount);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			INDEX_NONE,
			3.0f,
			bResetSucceeded ? FColor::Green : FColor::Red,
			bResetSucceeded ? TEXT("World reset complete") : TEXT("World reset failed"));
	}
}

void APSPlayerController::HandleAdvanceTime()
{
	if (UPSGameTimeSubsystem* GameTime = GetWorld() ? GetWorld()->GetSubsystem<UPSGameTimeSubsystem>() : nullptr)
	{
		GameTime->AdvanceGameHours(6);
		if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.0f, FColor::Cyan, TEXT("Game time advanced by 6 hours"));
	}
}

void APSPlayerController::DropStoneReward(const FIntPoint Cell)
{
	FPSItemStack Stack;
	Stack.ItemType = EPSItemType::Stone;
	Stack.Quantity = 1;
	DropItemReward(Cell, Stack);
}

void APSPlayerController::DropItemReward(const FIntPoint Cell, const FPSItemStack& Stack)
{
	if (!GetWorld() || !GridWorld || Stack.IsEmpty()) return;
	const FTransform SpawnTransform(FRotator::ZeroRotator, GridWorld->CellToWorldCenter(Cell) + FVector(0, 0, 20));
	APSWorldItemActor* Drop = GetWorld()->SpawnActorDeferred<APSWorldItemActor>(
		APSWorldItemActor::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Drop) return;
	Drop->InitializeItem(Stack, GridWorld);
	Drop->FinishSpawning(SpawnTransform);
}

void APSPlayerController::HandleAdvanceSeason()
{
	if (UPSGameTimeSubsystem* GameTime = GetWorld() ? GetWorld()->GetSubsystem<UPSGameTimeSubsystem>() : nullptr)
	{
		GameTime->AdvanceGameDays(28);
		if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.0f, FColor::Cyan, TEXT("Game season advanced by 28 days"));
	}
}

void APSPlayerController::UpdateStatusWidget()
{
	if (!IsLocalController()) return;
	if (!StatusWidget)
	{
		UClass* WidgetClass = PlayerStatusWidgetClass.Get();
		if (!WidgetClass) WidgetClass = UPSPlayerStatusWidget::StaticClass();
		StatusWidget = CreateWidget<UPSPlayerStatusWidget>(this, WidgetClass);
		if (!StatusWidget) return;
		StatusWidget->AddToPlayerScreen();
		StatusWidget->SetPositionInViewport(FVector2D(24.0f, 24.0f), false);
		StatusWidget->SetDesiredSizeInViewport(FVector2D(280.0f, 340.0f));
		const FPSItemStack* Stack = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
		StatusWidget->SetEquipment(Equipment, SelectedHotbarSlot, Stack ? Stack->Quantity : 0, GetEquippedFarmChargeDepth());
		StatusWidget->SetHarvestedCropCount(HarvestedCropCount);
		StatusPawn = GetPawn();
		StatusWidget->SetStatsComponent(GetPawn() ? GetPawn()->FindComponentByClass<UPSPlayerStatsComponent>() : nullptr);
	}
	if (StatusPawn.IsStale() || StatusPawn.Get() != GetPawn())
	{
		StatusPawn = GetPawn();
		StatusWidget->SetStatsComponent(GetPawn() ? GetPawn()->FindComponentByClass<UPSPlayerStatsComponent>() : nullptr);
	}
}

void APSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCropHarvest();
	GatheringAction->Cancel();
	CancelFarmAction();
	if (IsValid(FarmTargetMarker)) FarmTargetMarker->Destroy();
	FarmTargetMarker = nullptr;
	for (APSFarmTargetMarker* Marker : FarmSideMarkers) if (IsValid(Marker)) Marker->Destroy();
	FarmSideMarkers.Reset();
	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshEquipmentFromHotbar);
	}
	CloseFishingJournal();
	CloseInventory();
	InventoryWidget = nullptr;
	if (HotbarWidget)
	{
		HotbarWidget->RemoveFromParent();
		HotbarWidget = nullptr;
	}
	StopFishing(false, false);
	if (FishingWidget)
	{
		FishingWidget->RemoveFromParent();
		FishingWidget = nullptr;
	}
	if (TimeWidget)
	{
		TimeWidget->RemoveFromParent();
		TimeWidget = nullptr;
	}
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (GameplayMappingContext)
			{
				InputSubsystem->RemoveMappingContext(GameplayMappingContext);
			}
			if (HotbarMappingContext)
			{
				InputSubsystem->RemoveMappingContext(HotbarMappingContext);
			}
		}
	}

	if (StatusWidget)
	{
		StatusWidget->SetStatsComponent(nullptr);
		StatusWidget->RemoveFromParent();
		StatusWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
