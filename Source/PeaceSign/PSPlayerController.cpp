// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSPlayerController.h"
#include "PSPlayerStatsComponent.h"
#include "UI/PSPlayerStatusWidget.h"
#include "UI/PSGameTimeWidget.h"
#include "UI/PSInventoryWidget.h"
#include "UI/PSHotbarWidget.h"
#include "UI/PSFishingWidget.h"
#include "Inventory/PSInventoryComponent.h"
#include "Inventory/PSWorldItemActor.h"
#include "PSPlayerCharacter.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
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

namespace
{
	TArray<FPSFishDefinition> GetFallbackFish()
	{
		TArray<FPSFishDefinition> Result;
		const auto Add = [&Result](const TCHAR* Name, const int32 Difficulty, const int32 MinSize,
			const int32 MaxSize, const float Weight)
		{
			FPSFishDefinition& Fish = Result.AddDefaulted_GetRef();
			Fish.DisplayName = FText::FromString(Name);
			Fish.Difficulty = Difficulty;
			Fish.MinSizeCm = MinSize;
			Fish.MaxSizeCm = MaxSize;
			Fish.Weight = Weight;
		};
		Add(TEXT("붕어"), 1, 8, 28, 50.0f);
		Add(TEXT("잉어"), 2, 25, 70, 30.0f);
		Add(TEXT("메기"), 3, 45, 110, 15.0f);
		Add(TEXT("황금 잉어"), 4, 35, 90, 5.0f);
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
		case EPSTileInteractionResult::Mined:
			return TEXT("Stone mined into dirt");
		case EPSTileInteractionResult::Planted:
			return TEXT("Seed planted");
		case EPSTileInteractionResult::Harvested:
			return TEXT("Crop harvested");
		case EPSTileInteractionResult::CropRemoved:
			return TEXT("Crop removed");
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
	InventoryComponent = CreateDefaultSubobject<UPSInventoryComponent>(TEXT("InventoryComponent"));
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
	}
	if (SpecialAttackAction)
	{
		EnhancedInputComponent->BindAction(SpecialAttackAction, ETriggerEvent::Started, this, &APSPlayerController::HandleSpecialAttack);
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
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &APSPlayerController::HandleAdvanceTime);
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &APSPlayerController::HandleFishingUp);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &APSPlayerController::HandleFishingLeft);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &APSPlayerController::HandleFishingDown);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &APSPlayerController::HandleFishingRight);
}

void APSPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateFishing(DeltaTime);
	UpdateStatusWidget();
	if (bInventoryOpen) return;
	HoveredCell = GetCursorCell();

	if (!HoveredCell.IsSet())
	{
		return;
	}
	const FIntPoint Cell = HoveredCell.GetValue();
	const EPSTileType TileType = GridWorld->GetGroundTile(Cell);
	const FVector CellCenter = GridWorld->CellToWorldCenter(Cell);
	const float HalfCell = GridWorld->GetCellSize() * 0.48f;
	DrawDebugBox(
		GetWorld(),
		CellCenter,
		FVector(HalfCell, HalfCell, 1.0f),
		Equipment == EPSEquipment::FishingRod
			? (GetPawn() && GridWorld->CanFishFrom(GridWorld->WorldToCell(GetPawn()->GetActorLocation()), Cell)
				? FColor::Green : FColor::Red)
			: GetTileDebugColor(TileType),
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
	if (Equipment == EPSEquipment::FishingRod)
	{
		HandleFishingRod();
		return;
	}
	OnInteractRequested();
}

void APSPlayerController::HandleSpecialAttack()
{
	if (Equipment == EPSEquipment::FishingRod)
	{
		HandleFishingRod();
		return;
	}
	// Resolve the cursor at click time, not from the previous frame's highlight.
	const TOptional<FIntPoint> TargetCell = GetCursorCell();
	if (TargetCell.IsSet())
	{
		const EPSTileInteractionResult Result = UseEquippedItemOnCell(TargetCell.GetValue());
		if (Equipment == EPSEquipment::BareHands)
		{
			if (Result == EPSTileInteractionResult::CropRemoved)
			{
				if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.0f, FColor::Yellow, GetInteractionResultText(Result));
				return;
			}
			OnSpecialAttackRequested();
			return;
		}
		if (Equipment != EPSEquipment::UnusableItem && GEngine)
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

EPSTileInteractionResult APSPlayerController::UseEquippedItemOnCell(const FIntPoint Cell)
{
	if (!GridWorld) return EPSTileInteractionResult::InvalidCell;
	switch (Equipment)
	{
	case EPSEquipment::BareHands:
		return GridWorld->RemoveCrop(Cell);
	case EPSEquipment::Hoe:
	{
		const bool bMatureCrop = GridWorld->GetCropType(Cell) != EPSCropType::None
			&& GridWorld->GetCropStage(Cell) >= PSCropGrowth::MaxStage;
		const int32 CropId = GridWorld->GetCropId(Cell);
		if (bMatureCrop && (!InventoryComponent
			|| !InventoryComponent->CanAddItem(EPSItemType::TestCrop, 1, CropId)))
			return EPSTileInteractionResult::InventoryFull;
		EPSTileInteractionResult Result = GridWorld->HarvestCrop(Cell);
		if (Result != EPSTileInteractionResult::Harvested) return GridWorld->TillCell(Cell);
		if (!InventoryComponent->AddItem(EPSItemType::TestCrop, 1, CropId))
		{
			ensureMsgf(false, TEXT("Harvest capacity changed after it was validated."));
			return EPSTileInteractionResult::NoEffect;
		}
		++HarvestedCropCount;
		if (StatusWidget) StatusWidget->SetHarvestedCropCount(HarvestedCropCount);
		return Result;
	}
	case EPSEquipment::Seed:
	{
		const FPSItemStack* Seed = InventoryComponent
			? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
		if (!Seed || Seed->ItemType != EPSItemType::TestSeed || Seed->Quantity <= 0)
			return EPSTileInteractionResult::NoEffect;
		const EPSTileInteractionResult Result = GridWorld->PlantSeed(Cell, Seed->CropId);
		if (Result == EPSTileInteractionResult::Planted
			&& !InventoryComponent->RemoveFromHotbarSlot(SelectedHotbarSlot, 1))
		{
			GridWorld->RemoveCrop(Cell);
			return EPSTileInteractionResult::NoEffect;
		}
		return Result;
	}
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
	if (AreFishingControlsLocked()) return;
	if (!InventoryComponent || !InventoryComponent->IsHotbarSlot(SlotIndex)) return;
	InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshEquipmentFromHotbar);
	SelectedHotbarSlot = SlotIndex;
	RefreshEquipmentFromHotbar();
}

void APSPlayerController::RefreshEquipmentFromHotbar()
{
	EPSEquipment NewEquipment = EPSEquipment::BareHands;
	const FPSItemStack* Stack = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	if (Stack && !Stack->IsEmpty())
	{
		switch (Stack->ItemType)
		{
		case EPSItemType::Hoe: NewEquipment = EPSEquipment::Hoe; break;
		case EPSItemType::TestSeed: NewEquipment = EPSEquipment::Seed; break;
		case EPSItemType::FishingRod: NewEquipment = EPSEquipment::FishingRod; break;
		default: NewEquipment = EPSEquipment::UnusableItem; break;
		}
	}
	SetEquipment(NewEquipment);
}

void APSPlayerController::HandleFishingRod()
{
	if (FishingState == EPSFishingState::WaitingForBite)
	{
		StopFishing(true);
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
		if (GEngine) GEngine->AddOnScreenDebugMessage(4, 2.0f, FColor::Yellow,
			TEXT("Stand still on land and select water within 2 cells straight or 1 cell diagonally."));
	}
}

bool APSPlayerController::TryUseFishingRod(const FIntPoint WaterCell)
{
	UpdateFishing(0.0f);
	if (IsFishing()) return false;
	const APawn* PlayerPawn = GetPawn();
	if (Equipment != EPSEquipment::FishingRod || !IsValid(GridWorld) || !PlayerPawn
		|| !PlayerPawn->GetVelocity().IsNearlyZero()
		|| !GridWorld->CanFishFrom(GridWorld->WorldToCell(PlayerPawn->GetActorLocation()), WaterCell)) return false;
	FishingCell = WaterCell;
	FishingPawn = GetPawn();
	FishingStartLocation = PlayerPawn->GetActorLocation();
	CurrentFish = SelectFishDefinition();
	CurrentFishSizeCm = 0;
	FishingSequence.Reset();
	FishingSequenceIndex = 0;
	FishingState = EPSFishingState::WaitingForBite;
	FishingStateDuration = FMath::FRandRange(FMath::Min(MinBiteDelay, MaxBiteDelay), FMath::Max(MinBiteDelay, MaxBiteDelay));
	FishingStateTimeRemaining = FishingStateDuration;
	SetFishingMovementLocked(true);
	if (GEngine) GEngine->AddOnScreenDebugMessage(4, 2.0f, FColor::Cyan, TEXT("Fishing rod cast"));
	OnFishingRodUsed(WaterCell);
	RefreshFishingWidget();
	return true;
}

void APSPlayerController::UpdateFishing(const float DeltaSeconds)
{
	if (!IsFishing()) return;
	const APawn* PlayerPawn = GetPawn();
	const bool bActiveCast = FishingState == EPSFishingState::WaitingForBite
		|| FishingState == EPSFishingState::BiteWindow || FishingState == EPSFishingState::Minigame;
	if (bActiveCast && (!PlayerPawn || FishingPawn.Get() != PlayerPawn || !IsValid(GridWorld)
		|| Equipment != EPSEquipment::FishingRod
		|| !PlayerPawn->GetActorLocation().Equals(FishingStartLocation, 0.01f)
		|| !GridWorld->CanFishFrom(GridWorld->WorldToCell(PlayerPawn->GetActorLocation()), FishingCell.GetValue())))
	{
		StopFishing(false);
		return;
	}
	if (bActiveCast)
	{
		const FVector Target = GridWorld->CellToWorldCenter(FishingCell.GetValue()) + FVector(0, 0, 8);
		// Draw for this frame only: stopping clears the visual without flushing other debug shapes.
		DrawDebugLine(GetWorld(), PlayerPawn->GetActorLocation() + FVector(0, 0, 12), Target,
			FColor::White, false, 0.0f, 0, 2.0f);
		DrawDebugSphere(GetWorld(), Target, 8.0f, 12, FColor::Red, false, 0.0f);
	}

	if (DeltaSeconds > 0.0f)
	{
		FishingStateTimeRemaining = FMath::Max(0.0f, FishingStateTimeRemaining - DeltaSeconds);
		if (FishingStateTimeRemaining <= 0.0f)
		{
			switch (FishingState)
			{
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
	if (GEngine) GEngine->AddOnScreenDebugMessage(4, 2.0f, FColor::Yellow, TEXT("! Bite ! Press F or Right Mouse"));
	OnFishingBite();
}

void APSPlayerController::BeginFishingMinigame()
{
	if (FishingState != EPSFishingState::BiteWindow) return;
	FishingState = EPSFishingState::Minigame;
	FishingStateDuration = MinigameDuration;
	FishingStateTimeRemaining = FishingStateDuration;
	GenerateFishingSequence();
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
	if (FishingSequence[FishingSequenceIndex] != Direction) return false;
	++FishingSequenceIndex;
	if (FishingSequenceIndex >= FishingSequence.Num()) CompleteFishing(true);
	else RefreshFishingWidget();
	return true;
}

void APSPlayerController::HandleFishingUp() { SubmitFishingDirection(EPSFishingDirection::Up); }
void APSPlayerController::HandleFishingLeft() { SubmitFishingDirection(EPSFishingDirection::Left); }
void APSPlayerController::HandleFishingDown() { SubmitFishingDirection(EPSFishingDirection::Down); }
void APSPlayerController::HandleFishingRight() { SubmitFishingDirection(EPSFishingDirection::Right); }

FPSFishDefinition APSPlayerController::SelectFishDefinition() const
{
	TArray<FPSFishDefinition> Fish;
	if (FishDataTable)
	{
		TArray<FPSFishDefinition*> Rows;
		FishDataTable->GetAllRows(TEXT("Fishing"), Rows);
		for (const FPSFishDefinition* Row : Rows) if (Row) Fish.Add(*Row);
	}
	if (Fish.IsEmpty()) Fish = GetFallbackFish();
	float TotalWeight = 0.0f;
	for (const FPSFishDefinition& Entry : Fish) TotalWeight += FMath::Max(0.0f, Entry.Weight);
	if (TotalWeight <= 0.0f) return Fish[0];
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FPSFishDefinition& Entry : Fish)
	{
		Roll -= FMath::Max(0.0f, Entry.Weight);
		if (Roll <= 0.0f) return Entry;
	}
	return Fish.Last();
}

void APSPlayerController::CompleteFishing(const bool bSuccess)
{
	if (FishingState != EPSFishingState::BiteWindow && FishingState != EPSFishingState::Minigame) return;
	if (bSuccess)
	{
		CurrentFishSizeCm = FMath::RandRange(FMath::Min(CurrentFish.MinSizeCm, CurrentFish.MaxSizeCm),
			FMath::Max(CurrentFish.MinSizeCm, CurrentFish.MaxSizeCm));
		if (!InventoryComponent || !InventoryComponent->AddItem(EPSItemType::Fish, 1)) DropFishingReward();
		FishingState = EPSFishingState::Success;
		FishingStateDuration = 1.5f;
		OnFishingSucceeded(CurrentFish.DisplayName, CurrentFishSizeCm);
	}
	else
	{
		FishingState = EPSFishingState::Failure;
		FishingStateDuration = 1.0f;
		OnFishingFailed();
	}
	FishingStateTimeRemaining = FishingStateDuration;
	// Success retains the lock through its motion; failure restores control immediately.
	if (!bSuccess) SetFishingMovementLocked(false);
	RefreshFishingWidget();
}

void APSPlayerController::DropFishingReward()
{
	if (!GetWorld() || !GetPawn()) return;
	const FTransform SpawnTransform(FRotator::ZeroRotator, GetPawn()->GetActorLocation() + FVector(45, 0, 20));
	APSWorldItemActor* Drop = GetWorld()->SpawnActorDeferred<APSWorldItemActor>(
		APSWorldItemActor::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Drop)
	{
		FPSItemStack Stack;
		Stack.ItemType = EPSItemType::Fish;
		Stack.Quantity = 1;
		Drop->InitializeItem(Stack);
		Drop->FinishSpawning(SpawnTransform);
	}
}

void APSPlayerController::SetFishingMovementLocked(const bool bLocked)
{
	APawn* PawnToUpdate = FishingPawn.Get();
	if (!PawnToUpdate) PawnToUpdate = GetPawn();
	if (APSPlayerCharacter* PlayerCharacter = Cast<APSPlayerCharacter>(PawnToUpdate)) PlayerCharacter->SetMovementLocked(bLocked);
}

bool APSPlayerController::AreFishingControlsLocked() const
{
	return FishingState == EPSFishingState::WaitingForBite || FishingState == EPSFishingState::BiteWindow
		|| FishingState == EPSFishingState::Minigame || FishingState == EPSFishingState::Success;
}

void APSPlayerController::RefreshFishingWidget()
{
	if (!FishingWidget) return;
	float Opacity = 1.0f;
	if (FishingState == EPSFishingState::Success && FishingStateTimeRemaining < 0.5f) Opacity = FishingStateTimeRemaining / 0.5f;
	FishingWidget->Refresh(FishingState, FishingStateTimeRemaining, FishingStateDuration,
		FishingSequence, FishingSequenceIndex, CurrentFish.DisplayName, CurrentFishSizeCm, Opacity);
}

void APSPlayerController::StopFishing(const bool bCancelled)
{
	const bool bWasFishing = IsFishing();
	SetFishingMovementLocked(false);
	FishingCell.Reset();
	FishingPawn.Reset();
	FishingSequence.Reset();
	FishingSequenceIndex = 0;
	FishingState = EPSFishingState::Idle;
	FishingStateDuration = 0.0f;
	FishingStateTimeRemaining = 0.0f;
	if (bWasFishing && bCancelled) OnFishingCancelled();
	RefreshFishingWidget();
}

void APSPlayerController::SetEquipment(const EPSEquipment InEquipment)
{
	if (Equipment != InEquipment) StopFishing();
	Equipment = InEquipment;
	const FPSItemStack* Stack = InventoryComponent ? InventoryComponent->FindHotbarSlot(SelectedHotbarSlot) : nullptr;
	if (StatusWidget) StatusWidget->SetEquipment(Equipment, SelectedHotbarSlot, Stack ? Stack->Quantity : 0);
}

void APSPlayerController::HandleInventory()
{
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
	UWidgetBlueprintLibrary::CancelDragDrop();
	if (InventoryWidget) InventoryWidget->RemoveFromParent();
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
	if (AreFishingControlsLocked()) return;
	UE_LOG(LogTemp, Log, TEXT("Quest requested"));
	OnQuestRequested();
}

void APSPlayerController::HandleAbility()
{
	if (AreFishingControlsLocked()) return;
	UE_LOG(LogTemp, Log, TEXT("Ability requested"));
	OnAbilityRequested();
}

void APSPlayerController::HandleCraft()
{
	if (AreFishingControlsLocked()) return;
	UE_LOG(LogTemp, Log, TEXT("Craft requested"));
	OnCraftRequested();
}

void APSPlayerController::HandleResetWorld()
{
	if (!GridWorld)
	{
		return;
	}

	const bool bResetSucceeded = GridWorld->ResetWorld();
	if (bResetSucceeded)
	{
		StopFishing();
		SelectedHotbarSlot = INDEX_NONE;
		if (InventoryComponent) InventoryComponent->ResetToDefaults();
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
		StatusWidget->SetEquipment(Equipment, SelectedHotbarSlot, Stack ? Stack->Quantity : 0);
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
	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshEquipmentFromHotbar);
	}
	CloseInventory();
	InventoryWidget = nullptr;
	if (HotbarWidget)
	{
		HotbarWidget->RemoveFromParent();
		HotbarWidget = nullptr;
	}
	StopFishing();
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
