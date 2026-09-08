// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
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
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
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

private:
	void Move(const FInputActionValue& Value);
	void StopMoving();
	void StartRun();
	void StopRun();
	void Roll();
	void StartRoll(const FVector2D& MovementInput);
	void TickRoll(float DeltaSeconds);
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

	bool bWantsToRun = false;
	bool bIsRolling = false;
	float RollTimeRemaining = 0.0f;
	FVector2D CurrentMovementInput = FVector2D::ZeroVector;
	FVector2D LastMovementInput = FVector2D(0.0f, -1.0f);
	FVector RollWorldDirection = FVector(-1.0f, 0.0f, 0.0f);
};
