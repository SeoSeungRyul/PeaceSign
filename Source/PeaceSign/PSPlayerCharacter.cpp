// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

APSPlayerCharacter::APSPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 48.0f);

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->MaxWalkSpeed = 300.0f;
	MovementComponent->bOrientRotationToMovement = false;
	MovementComponent->bConstrainToPlane = true;
	MovementComponent->SetPlaneConstraintNormal(FVector::UpVector);
	MovementComponent->bSnapToPlaneAtStart = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 1000.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 1024.0f;
}

void APSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("PeaceSign input binding: Pawn=%s Controller=%s MoveAction=%s"),
		*GetName(),
		*GetNameSafe(GetController()),
		*GetNameSafe(MoveAction));

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Triggered,
			this,
			&APSPlayerCharacter::Move);
	}
}

void APSPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementInput = Value.Get<FVector2D>();
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("PeaceSign move input: X=%.2f Y=%.2f"),
		MovementInput.X,
		MovementInput.Y);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			1,
			0.1f,
			FColor::Green,
			FString::Printf(
				TEXT("Move X=%.2f Y=%.2f"),
				MovementInput.X,
				MovementInput.Y));
	}

	if (MovementInput.IsNearlyZero())
	{
		return;
	}

	AddMovementInput(FVector::ForwardVector, MovementInput.Y);
	AddMovementInput(FVector::RightVector, MovementInput.X);
}
