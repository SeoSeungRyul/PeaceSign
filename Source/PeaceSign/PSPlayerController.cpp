// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSPlayerController.h"
#include "PSPlayerStatsComponent.h"
#include "UI/PSPlayerStatusWidget.h"
#include "UI/PSGameTimeWidget.h"

#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"
#include "World/PSGridWorld.h"

namespace
{
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
	SetEquipment(EPSEquipment::BareHands);
	if (IsLocalController())
	{
		TimeWidget = CreateWidget<UPSGameTimeWidget>(this, UPSGameTimeWidget::StaticClass());
		if (TimeWidget) TimeWidget->AddToPlayerScreen();
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

	// R is a development-only world reset shortcut and is intentionally not part of the gameplay IA set.
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &APSPlayerController::HandleResetWorld);
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &APSPlayerController::EquipBareHands);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &APSPlayerController::EquipHoe);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &APSPlayerController::EquipSeed);
}

void APSPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateStatusWidget();
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
		GetTileDebugColor(TileType),
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
	OnInteractRequested();
}

void APSPlayerController::HandleSpecialAttack()
{
	if (Equipment == EPSEquipment::BareHands)
	{
		OnSpecialAttackRequested();
		return;
	}
	// Resolve the cursor at click time, not from the previous frame's highlight.
	const TOptional<FIntPoint> TargetCell = GetCursorCell();
	if (TargetCell.IsSet())
	{
		EPSTileInteractionResult Result = EPSTileInteractionResult::NoEffect;
		switch (Equipment)
		{
		case EPSEquipment::Hoe:
			Result = GridWorld->TillCell(TargetCell.GetValue());
			break;
		case EPSEquipment::Seed:
			Result = GridWorld->PlantSeed(TargetCell.GetValue());
			break;
		case EPSEquipment::BareHands:
		default:
			return;
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				INDEX_NONE,
				2.0f,
				Result == EPSTileInteractionResult::InvalidCell ? FColor::Red : FColor::Yellow,
				GetInteractionResultText(Result));
		}
	}
}

void APSPlayerController::EquipBareHands()
{
	SetEquipment(EPSEquipment::BareHands);
}

void APSPlayerController::EquipHoe()
{
	SetEquipment(EPSEquipment::Hoe);
}

void APSPlayerController::EquipSeed()
{
	SetEquipment(EPSEquipment::Seed);
}

void APSPlayerController::SetEquipment(const EPSEquipment InEquipment)
{
	Equipment = InEquipment;
	if (StatusWidget) StatusWidget->SetEquipment(Equipment);
}

void APSPlayerController::HandleInventory()
{
	UE_LOG(LogTemp, Log, TEXT("Inventory requested"));
	OnInventoryRequested();
}

void APSPlayerController::HandleQuest()
{
	UE_LOG(LogTemp, Log, TEXT("Quest requested"));
	OnQuestRequested();
}

void APSPlayerController::HandleAbility()
{
	UE_LOG(LogTemp, Log, TEXT("Ability requested"));
	OnAbilityRequested();
}

void APSPlayerController::HandleCraft()
{
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
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			INDEX_NONE,
			3.0f,
			bResetSucceeded ? FColor::Green : FColor::Red,
			bResetSucceeded ? TEXT("World reset complete") : TEXT("World reset failed"));
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
		StatusWidget->SetEquipment(Equipment);
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
