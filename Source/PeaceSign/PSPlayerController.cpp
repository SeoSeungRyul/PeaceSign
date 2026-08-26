// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSPlayerController.h"

#include "DrawDebugHelpers.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "World/PSGridWorld.h"

void APSPlayerController::BeginPlay()
{
	Super::BeginPlay();

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
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("PeaceSign mapping setup: Controller=%s Local=%s Subsystem=%s Context=%s"),
		*GetName(),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(InputSubsystem),
		*GetNameSafe(GameplayMappingContext));

	if (InputSubsystem && GameplayMappingContext)
	{
		InputSubsystem->AddMappingContext(GameplayMappingContext, 0);
	}
}

void APSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &APSPlayerController::HandlePrimaryAction);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &APSPlayerController::HandleResetWorld);
}

void APSPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	HoveredCell.Reset();

	if (!GridWorld)
	{
		return;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z))
	{
		return;
	}

	const float GridPlaneZ = GridWorld->GetActorLocation().Z;
	const float IntersectionDistance = (GridPlaneZ - RayOrigin.Z) / RayDirection.Z;
	if (IntersectionDistance < 0.0f)
	{
		return;
	}

	HoveredCell = GridWorld->WorldToCell(RayOrigin + RayDirection * IntersectionDistance);
	const FVector CellCenter = GridWorld->CellToWorldCenter(HoveredCell.GetValue());
	const float HalfCell = GridWorld->GetCellSize() * 0.48f;
	DrawDebugBox(
		GetWorld(),
		CellCenter,
		FVector(HalfCell, HalfCell, 1.0f),
		FColor::Yellow,
		false,
		0.0f,
		0,
		2.0f);
}

void APSPlayerController::HandlePrimaryAction()
{
	if (GridWorld && HoveredCell.IsSet())
	{
		GridWorld->ToggleGroundTile(HoveredCell.GetValue());
	}
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
