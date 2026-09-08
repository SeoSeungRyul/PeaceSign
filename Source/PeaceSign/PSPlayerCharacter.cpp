// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSPlayerCharacter.h"
#include "PSPlayerStatsComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "SpriteEditorOnlyTypes.h"
#include "UObject/ConstructorHelpers.h"

APSPlayerCharacter::APSPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	StatsComponent = CreateDefaultSubobject<UPSPlayerStatsComponent>(TEXT("StatsComponent"));

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 48.0f);

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->MaxWalkSpeed = 300.0f;
	MovementComponent->bOrientRotationToMovement = false;
	MovementComponent->bConstrainToPlane = true;
	MovementComponent->SetPlaneConstraintNormal(FVector::UpVector);
	MovementComponent->bSnapToPlaneAtStart = false;
	MovementComponent->GravityScale = 0.0f;
	MovementComponent->DefaultLandMovementMode = MOVE_Flying;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 1000.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	// 직교 카메라 (거리와 관계없이 물체 크기가 일정합니다)
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
	// 여기서 카메라 거리 수정
	Camera->OrthoWidth = 512.0f; // 기본 1024

	static ConstructorHelpers::FObjectFinder<UTexture2D> FrontTextureFinder(
		TEXT("/Game/Art/Male/front.front"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> BackTextureFinder(
		TEXT("/Game/Art/Male/back.back"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> LeftTextureFinder(
		TEXT("/Game/Art/Male/left.left"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> RightTextureFinder(
		TEXT("/Game/Art/Male/right.right"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionFinder(
		TEXT("/Game/Inputs/IA_Move.IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> RunActionFinder(
		TEXT("/Game/Inputs/IA_Run.IA_Run"));
	static ConstructorHelpers::FObjectFinder<UInputAction> RollActionFinder(
		TEXT("/Game/Inputs/IA_Roll.IA_Roll"));

	FrontTexture = FrontTextureFinder.Object;
	BackTexture = BackTextureFinder.Object;
	LeftTexture = LeftTextureFinder.Object;
	RightTexture = RightTextureFinder.Object;
	MoveAction = MoveActionFinder.Object;
	RunAction = RunActionFinder.Object;
	RollAction = RollActionFinder.Object;
}

void APSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->SetPlaneConstraintOrigin(GetActorLocation());
	MovementComponent->SetMovementMode(MOVE_Flying);

	FrontIdle = CreateIdleFlipbook(FrontTexture, TEXT("FrontIdle"));
	BackIdle = CreateIdleFlipbook(BackTexture, TEXT("BackIdle"));
	LeftIdle = CreateIdleFlipbook(LeftTexture, TEXT("LeftIdle"));
	RightIdle = CreateIdleFlipbook(RightTexture, TEXT("RightIdle"));

	// 아래쪽을 기본 방향으로 사용하고, 이동이 멈추면 마지막 방향을 유지합니다.
	if (FrontIdle)
	{
		GetSprite()->SetFlipbook(FrontIdle);
	}
}

void APSPlayerCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector2D MovementInput = CurrentMovementInput;
	if (!MovementInput.IsNearlyZero())
	{
		MovementInput.Normalize();
		LastMovementInput = MovementInput;
	}

	StatsComponent->SetMovementActive(!MovementInput.IsNearlyZero() || bIsRolling);
	if (bIsRolling)
	{
		TickRoll(DeltaSeconds);
		return;
	}

	if (MovementInput.IsNearlyZero())
	{
		return;
	}

	SetFacingFromInput(MovementInput);
	const bool bIsRunning = bWantsToRun
		&& StatsComponent->TryConsumeStamina(RunStaminaCostPerSecond * DeltaSeconds);
	const float CurrentMoveSpeed = bIsRunning ? RunSpeed : TemporaryMoveSpeed;
	const FVector WorldDelta(MovementInput.Y, MovementInput.X, 0.0f);
	AddActorWorldOffset(WorldDelta * CurrentMoveSpeed * DeltaSeconds, true);

}

void APSPlayerCharacter::StartRoll(const FVector2D& MovementInput)
{
	if (!StatsComponent->TryConsumeStamina(RollStaminaCost))
	{
		return;
	}

	const FVector2D RollInput = MovementInput.IsNearlyZero() ? LastMovementInput : MovementInput.GetSafeNormal();
	LastMovementInput = RollInput;
	RollWorldDirection = FVector(RollInput.Y, RollInput.X, 0.0f).GetSafeNormal();
	RollTimeRemaining = RollDuration;
	bIsRolling = true;
	StatsComponent->SetActionActive(true);
	SetFacingFromInput(RollInput);
}

void APSPlayerCharacter::TickRoll(const float DeltaSeconds)
{
	if (!bIsRolling)
	{
		return;
	}

	const float StepTime = FMath::Min(DeltaSeconds, RollTimeRemaining);
	const float RollSpeed = RollDuration > 0.0f ? RollDistance / RollDuration : 0.0f;
	FHitResult Hit;
	AddActorWorldOffset(RollWorldDirection * RollSpeed * StepTime, true, &Hit);
	RollTimeRemaining -= StepTime;

	if (RollTimeRemaining <= 0.0f || Hit.IsValidBlockingHit())
	{
		bIsRolling = false;
		RollTimeRemaining = 0.0f;
		StatsComponent->SetActionActive(false);
	}
}

UPaperFlipbook* APSPlayerCharacter::CreateIdleFlipbook(UTexture2D* Texture, const FName& ObjectName)
{
	if (!Texture)
	{
		return nullptr;
	}

	UPaperSprite* IdleSprite = NewObject<UPaperSprite>(this, *(ObjectName.ToString() + TEXT("Sprite")));
	FSpriteAssetInitParameters SpriteInit;
	SpriteInit.SetTextureAndFill(Texture);
	SpriteInit.SetPixelsPerUnrealUnit(2.56f);
	IdleSprite->InitializeSprite(SpriteInit);

	UPaperFlipbook* Flipbook = NewObject<UPaperFlipbook>(this, ObjectName);
	{
		FScopedFlipbookMutator Mutator(Flipbook);
		Mutator.FramesPerSecond = 1.0f;
		FPaperFlipbookKeyFrame& Frame = Mutator.KeyFrames.AddDefaulted_GetRef();
		Frame.Sprite = IdleSprite;
		Frame.FrameRun = 1;
	}

	if (ObjectName == TEXT("FrontIdle"))
	{
		FrontSprite = IdleSprite;
	}
	else if (ObjectName == TEXT("BackIdle"))
	{
		BackSprite = IdleSprite;
	}
	else if (ObjectName == TEXT("LeftIdle"))
	{
		LeftSprite = IdleSprite;
	}
	else
	{
		RightSprite = IdleSprite;
	}

	return Flipbook;
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
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Completed,
			this,
			&APSPlayerCharacter::StopMoving);
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Canceled,
			this,
			&APSPlayerCharacter::StopMoving);
	}

	if (RunAction)
	{
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Started, this, &APSPlayerCharacter::StartRun);
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Completed, this, &APSPlayerCharacter::StopRun);
		EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Canceled, this, &APSPlayerCharacter::StopRun);
	}

	if (RollAction)
	{
		EnhancedInputComponent->BindAction(RollAction, ETriggerEvent::Started, this, &APSPlayerCharacter::Roll);
	}
}

void APSPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementInput = Value.Get<FVector2D>();
	CurrentMovementInput = MovementInput;
	if (MovementInput.IsNearlyZero())
	{
		return;
	}

	LastMovementInput = MovementInput.GetSafeNormal();
	SetFacingFromInput(LastMovementInput);
}

void APSPlayerCharacter::StopMoving()
{
	CurrentMovementInput = FVector2D::ZeroVector;
}

void APSPlayerCharacter::StartRun()
{
	bWantsToRun = true;
}

void APSPlayerCharacter::StopRun()
{
	bWantsToRun = false;
}

void APSPlayerCharacter::Roll()
{
	if (!bIsRolling)
	{
		StartRoll(CurrentMovementInput);
	}
}

void APSPlayerCharacter::SetFacingFromInput(const FVector2D& MovementInput)
{
	UPaperFlipbook* DesiredIdle = nullptr;
	if (FMath::Abs(MovementInput.X) > FMath::Abs(MovementInput.Y))
	{
		DesiredIdle = MovementInput.X > 0.0f ? RightIdle : LeftIdle;
	}
	else
	{
		DesiredIdle = MovementInput.Y > 0.0f ? BackIdle : FrontIdle;
	}

	if (DesiredIdle && GetSprite()->GetFlipbook() != DesiredIdle)
	{
		GetSprite()->SetFlipbook(DesiredIdle);
	}
}

float APSPlayerCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsRolling) return 0.0f;
	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f) return 0.0f;
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float PreviousHealth = StatsComponent->Health;
	StatsComponent->ApplyDamage(AppliedDamage);
	return PreviousHealth - StatsComponent->Health;
}
