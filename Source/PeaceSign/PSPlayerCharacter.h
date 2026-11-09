// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "Fishing/PSFishingTypes.h"
#include "PSPlayerCharacter.generated.h"

class UPSPlayerStatsComponent;
class UCameraComponent;
class UInputAction;
class UPaperFlipbook;
class UPaperSprite;
class USpringArmComponent;
class UTexture2D;
struct FInputActionValue;

UCLASS()
class PEACESIGN_API APSPlayerCharacter : public APaperCharacter
{
	GENERATED_BODY()

public:
	APSPlayerCharacter();
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void UnPossessed() override;
	float GetFishingCameraBlendSeconds() const { return FMath::IsFinite(FishingCameraBlendSeconds) ? FMath::Max(0.f, FishingCameraBlendSeconds) : 1.2f; }
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	UFUNCTION(BlueprintCallable, Category="Movement") void SetMovementLocked(bool bLocked);
	UFUNCTION(BlueprintPure, Category="Movement") bool IsMovementLocked() const { return bMovementLocked; }
	UFUNCTION(BlueprintPure, Category="Movement") FIntPoint GetFacingCellOffset() const;
	/** Plays preparation through the second sprite; zero uses the controller's fallback timer. */
	float BeginFishingCastPreparation(const FVector& WorldDirection);
	void UpdateFishingCastPreparation(float ElapsedSeconds);
	void PlayFishingCastThrow();
	void EndFishingCastAnimation();
	float PlayFishingStateAnimation(EPSFishingState State);
	bool IsFishingStateAnimationPlaying() const;
	bool IsRolling() const { return bIsRolling; }
	bool HasMovementInput() const { return !CurrentMovementInput.IsNearlyZero(); }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	TObjectPtr<UPSPlayerStatsComponent> StatsComponent;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Camera components are created in C++ and tuned in a Blueprint child.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;
	/** Ease-in/out duration of the pan to the bobber and back. Zero switches immediately. Zoom stays unchanged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fishing|Camera", meta = (ClampMin = "0.0"))
	float FishingCameraBlendSeconds = 1.2f;

	// The pawn owns actions that directly operate the pawn.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RunAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RollAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float TemporaryMoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Stamina", meta = (ClampMin = "0.0"))
	float RunStaminaCostPerSecond = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Roll", meta = (ClampMin = "0.01"))
	float RollDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Roll", meta = (ClampMin = "0.0"))
	float RollDistance = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Roll", meta = (ClampMin = "0.0"))
	float RollStaminaCost = 30.0f;
	/** Optional full casting animation: first two keyframes prepare, remaining keyframes throw. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Animation")
	TObjectPtr<UPaperFlipbook> FishingCastFlipbook;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Animation") TObjectPtr<UPaperFlipbook> FishingPullFlipbook;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Animation") TObjectPtr<UPaperFlipbook> FishingSuccessFlipbook;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Animation") TObjectPtr<UPaperFlipbook> FishingFailureFlipbook;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Animation") TObjectPtr<UPaperFlipbook> FishingRetrieveFlipbook;

private:
	friend class FPSFishingTest;
	friend class FPSFishingOct26Test;
	friend class FPSFishingAnimationTest;
	friend class FPSFarmToolActionTest;
	friend class FPSChargedHoeTest;
	friend class FPSSecondHoeTest;
	void Move(const FInputActionValue& Value);
	void StopMoving();
	void StartRun();
	void StopRun();
	void Roll();
	void StartRoll(const FVector2D& MovementInput);
	void TickRoll(float DeltaSeconds);
	void UpdateFishingCamera(float DeltaSeconds);
	void SetFacingFromInput(const FVector2D& MovementInput);
	UPaperFlipbook* CreateIdleFlipbook(UTexture2D* Texture, const FName& ObjectName);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FrontTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BackTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LeftTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RightTexture;

	UPROPERTY(Transient)
	TObjectPtr<UPaperSprite> FrontSprite;

	UPROPERTY(Transient)
	TObjectPtr<UPaperSprite> BackSprite;

	UPROPERTY(Transient)
	TObjectPtr<UPaperSprite> LeftSprite;

	UPROPERTY(Transient)
	TObjectPtr<UPaperSprite> RightSprite;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> FrontIdle;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> BackIdle;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> LeftIdle;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> RightIdle;
	UPROPERTY(Transient) TObjectPtr<UPaperFlipbook> ActiveFishingCastFlipbook;
	UPROPERTY(Transient) TObjectPtr<UPaperFlipbook> BeforeFishingFlipbook;
	int32 FishingPreparationFrames = 0;
	bool bFishingAnimationThrown = false;
	bool bBeforeFishingLooping = true;
	bool bBeforeFishingPlaying = false;
	float BeforeFishingPosition = 0;
	FVector FishingCameraOffset = FVector::ZeroVector;
	FVector FishingCameraBlendStart = FVector::ZeroVector;
	float FishingCameraBlendElapsed = 0;
	bool bHasFishingCameraOffset = false;
	bool bWasFishingCameraFocused = false;

	bool bWantsToRun = false;
	bool bMovementLocked = false;
	bool bIsRolling = false;
	float RollTimeRemaining = 0.0f;
	FVector2D CurrentMovementInput = FVector2D::ZeroVector;
	FVector2D LastMovementInput = FVector2D(0.0f, -1.0f);
	FVector RollWorldDirection = FVector(-1.0f, 0.0f, 0.0f);
};
