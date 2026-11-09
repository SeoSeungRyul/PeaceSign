// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSPlayerCharacter.h"
#include "PSPlayerStatsComponent.h"
#include "PSPlayerController.h"

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
	if (ActiveFishingCastFlipbook && bFishingAnimationThrown && !bMovementLocked && !GetSprite()->IsPlaying()) EndFishingCastAnimation();
	if (bMovementLocked)
	{
		StatsComponent->SetMovementActive(false);
		return;
	}

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

void APSPlayerCharacter::CalcCamera(const float DeltaTime, FMinimalViewInfo& OutResult)
{
	// Apply the pan to the view that the player camera manager actually renders.
	// This does not depend on actor movement ticks or spring-arm socket refreshes.
	if (Camera) Camera->GetCameraView(DeltaTime, OutResult);
	else Super::CalcCamera(DeltaTime, OutResult);
	UpdateFishingCamera(DeltaTime);
	OutResult.Location += FishingCameraOffset;
}

void APSPlayerCharacter::UnPossessed()
{
	Super::UnPossessed();
	FishingCameraOffset = FVector::ZeroVector;
	FishingCameraBlendStart = FVector::ZeroVector;
	FishingCameraBlendElapsed = 0;
	bHasFishingCameraOffset = false;
	bWasFishingCameraFocused = false;
}

void APSPlayerCharacter::UpdateFishingCamera(const float DeltaSeconds)
{
	const APSPlayerController* PlayerController = Cast<APSPlayerController>(GetController());
	if (PlayerController && PlayerController->IsPaused()) return;
	FVector FocusLocation;
	const bool bFocus = PlayerController && PlayerController->GetFishingFocusLocation(FocusLocation);
	if (!bFocus && !bHasFishingCameraOffset) return;
	bHasFishingCameraOffset = true;
	if (bFocus != bWasFishingCameraFocused)
	{
		FishingCameraBlendStart = FishingCameraOffset;
		FishingCameraBlendElapsed = 0;
		bWasFishingCameraFocused = bFocus;
	}
	FVector DesiredOffset = FVector::ZeroVector;
	if (bFocus)
	{
		const FVector Delta = FocusLocation - GetActorLocation();
		// Pan on the ground plane; preserve the authored camera height, angle and zoom.
		DesiredOffset = FVector(Delta.X, Delta.Y, 0);
	}
	const float Duration = GetFishingCameraBlendSeconds();
	FishingCameraBlendElapsed += FMath::Max(0.f, DeltaSeconds);
	const float Alpha = Duration > 0 ? FMath::Clamp(FishingCameraBlendElapsed / Duration, 0.f, 1.f) : 1.f;
	// Quintic easing starts/ends at zero velocity and acceleration, so the slower
	// pan accelerates gradually without a sudden push at either end.
	const float SmoothAlpha = FMath::Clamp(Alpha * Alpha * Alpha * (Alpha * (6 * Alpha - 15) + 10), 0.f, 1.f);
	FishingCameraOffset = FMath::Lerp(FishingCameraBlendStart, DesiredOffset, SmoothAlpha);
	if (!bFocus && Alpha >= 1)
	{
		FishingCameraOffset = FVector::ZeroVector;
		bHasFishingCameraOffset = false;
	}
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
	if (bMovementLocked)
	{
		if (!MovementInput.IsNearlyZero())
		{
			if (APSPlayerController* PlayerController = Cast<APSPlayerController>(GetController()))
				PlayerController->CancelFishingForMovementInput();
		}
		if (bMovementLocked) return;
	}
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
	if (bMovementLocked) return;
	bWantsToRun = true;
}

void APSPlayerCharacter::StopRun()
{
	bWantsToRun = false;
}

void APSPlayerCharacter::Roll()
{
	if (bMovementLocked) return;
	if (!bIsRolling)
	{
		StartRoll(CurrentMovementInput);
	}
}

void APSPlayerCharacter::SetMovementLocked(const bool bLocked)
{
	bMovementLocked = bLocked;
	if (!bLocked) return;
	CurrentMovementInput = FVector2D::ZeroVector;
	bWantsToRun = false;
	bIsRolling = false;
	RollTimeRemaining = 0.0f;
	StatsComponent->SetMovementActive(false);
	StatsComponent->SetActionActive(false);
}

float APSPlayerCharacter::BeginFishingCastPreparation(const FVector& WorldDirection)
{
	EndFishingCastAnimation();
	const FVector2D Facing(WorldDirection.Y, WorldDirection.X);
	if (!Facing.IsNearlyZero())
	{
		LastMovementInput = Facing.GetSafeNormal();
		SetFacingFromInput(LastMovementInput);
	}
	if (!FishingCastFlipbook || FishingCastFlipbook->GetNumKeyFrames() < 2
		|| !FMath::IsFinite(FishingCastFlipbook->GetFramesPerSecond())
		|| FishingCastFlipbook->GetFramesPerSecond() <= 0) return 0;
	ActiveFishingCastFlipbook = FishingCastFlipbook;
	BeforeFishingFlipbook = GetSprite()->GetFlipbook();
	bBeforeFishingLooping = GetSprite()->IsLooping();
	bBeforeFishingPlaying = GetSprite()->IsPlaying();
	BeforeFishingPosition = GetSprite()->GetPlaybackPosition();
	FishingPreparationFrames = FMath::Max(1, ActiveFishingCastFlipbook->GetKeyFrameChecked(0).FrameRun)
		+ FMath::Max(1, ActiveFishingCastFlipbook->GetKeyFrameChecked(1).FrameRun);
	bFishingAnimationThrown = false;
	GetSprite()->SetFlipbook(ActiveFishingCastFlipbook);
	GetSprite()->SetLooping(false);
	GetSprite()->Stop();
	GetSprite()->SetPlaybackPositionInFrames(0, false);
	return FishingPreparationFrames / ActiveFishingCastFlipbook->GetFramesPerSecond();
}

void APSPlayerCharacter::UpdateFishingCastPreparation(const float ElapsedSeconds)
{
	if (!ActiveFishingCastFlipbook || bFishingAnimationThrown || GetSprite()->GetFlipbook() != ActiveFishingCastFlipbook) return;
	const int32 Frame = FMath::FloorToInt(FMath::Max(0.f, ElapsedSeconds) * ActiveFishingCastFlipbook->GetFramesPerSecond());
	GetSprite()->SetPlaybackPositionInFrames(FMath::Clamp(Frame, 0, FishingPreparationFrames - 1), false);
}

void APSPlayerCharacter::PlayFishingCastThrow()
{
	if (!ActiveFishingCastFlipbook || bFishingAnimationThrown || GetSprite()->GetFlipbook() != ActiveFishingCastFlipbook) return;
	bFishingAnimationThrown = true;
	if (ActiveFishingCastFlipbook->GetNumFrames() > FishingPreparationFrames)
	{
		GetSprite()->SetPlaybackPositionInFrames(FishingPreparationFrames, false);
		GetSprite()->Play();
	}
}

void APSPlayerCharacter::EndFishingCastAnimation()
{
	if (!ActiveFishingCastFlipbook) return;
	// Blueprint success/failure animations may have already replaced the casting clip.
	if (GetSprite()->GetFlipbook() == ActiveFishingCastFlipbook)
	{
		GetSprite()->SetFlipbook(BeforeFishingFlipbook);
		GetSprite()->SetLooping(bBeforeFishingLooping);
		GetSprite()->SetPlaybackPosition(BeforeFishingPosition, false);
		if (bBeforeFishingPlaying) GetSprite()->Play();
		else GetSprite()->Stop();
	}
	ActiveFishingCastFlipbook = nullptr;
	BeforeFishingFlipbook = nullptr;
	FishingPreparationFrames = 0;
	bFishingAnimationThrown = false;
}

float APSPlayerCharacter::PlayFishingStateAnimation(const EPSFishingState State)
{
	UPaperFlipbook* Clip = State == EPSFishingState::Success ? FishingSuccessFlipbook
		: State == EPSFishingState::Failure ? FishingFailureFlipbook
		: State == EPSFishingState::Retrieving ? FishingRetrieveFlipbook : FishingPullFlipbook;
	EndFishingCastAnimation();
	if (!Clip || Clip->GetTotalDuration() <= 0) return 0;
	BeforeFishingFlipbook = GetSprite()->GetFlipbook();
	bBeforeFishingLooping = GetSprite()->IsLooping();
	bBeforeFishingPlaying = GetSprite()->IsPlaying();
	BeforeFishingPosition = GetSprite()->GetPlaybackPosition();
	ActiveFishingCastFlipbook = Clip;
	bFishingAnimationThrown = true;
	GetSprite()->SetFlipbook(Clip);
	GetSprite()->SetLooping(State == EPSFishingState::Minigame);
	GetSprite()->PlayFromStart();
	return Clip->GetTotalDuration();
}

bool APSPlayerCharacter::IsFishingStateAnimationPlaying() const
{
	return ActiveFishingCastFlipbook && GetSprite()->GetFlipbook() == ActiveFishingCastFlipbook && GetSprite()->IsPlaying();
}

FIntPoint APSPlayerCharacter::GetFacingCellOffset() const
{
	// Match sprite facing, including the vertical tie-break used for diagonal movement.
	if (FMath::Abs(LastMovementInput.X) > FMath::Abs(LastMovementInput.Y))
		return FIntPoint(0, LastMovementInput.X > 0 ? 1 : -1);
	return FIntPoint(LastMovementInput.Y > 0 ? 1 : -1, 0);
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
