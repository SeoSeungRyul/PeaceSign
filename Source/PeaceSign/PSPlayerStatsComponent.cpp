#include "PSPlayerStatsComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

UPSPlayerStatsComponent::UPSPlayerStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPSPlayerStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	// The current pawn moves by offsets, so velocity alone cannot detect its movement.
	const bool bMoving = Pawn && (!Pawn->GetVelocity().IsNearlyZero() || (Controller &&
		(Controller->IsInputKeyDown(EKeys::W) || Controller->IsInputKeyDown(EKeys::A) ||
		 Controller->IsInputKeyDown(EKeys::S) || Controller->IsInputKeyDown(EKeys::D))));
	if (Health > 0.0f && !bMoving && !bActionActive && !bConsumedSinceLastTick && FMath::IsFinite(DeltaTime) && DeltaTime > 0.0f)
	{
		const float NewStamina = FMath::Min(MaxStamina, Stamina + DeltaTime * 10.0f);
		if (NewStamina != Stamina)
		{
			Stamina = NewStamina;
			OnStatsChanged.Broadcast();
		}
	}
	bConsumedSinceLastTick = false;
}

void UPSPlayerStatsComponent::ApplyDamage(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || Health <= 0.0f) return;
	Health = FMath::Max(0.0f, Health - Amount);
	const bool bDepleted = Health == 0.0f;
	OnStatsChanged.Broadcast();
	if (bDepleted) OnHealthDepleted.Broadcast();
}

void UPSPlayerStatsComponent::RestoreHealth(float Amount)
{
	// Revival is explicit through ResetStats; healing must not silently revive a dead pawn.
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || Health <= 0.0f) return;
	const float NewHealth = FMath::Min(MaxHealth, Health + Amount);
	if (NewHealth == Health) return;
	Health = NewHealth;
	OnStatsChanged.Broadcast();
}

bool UPSPlayerStatsComponent::TryConsumeStamina(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount < 0.0f || Health <= 0.0f || Stamina < Amount) return false;
	if (Amount == 0.0f) return true;
	Stamina -= Amount;
	bConsumedSinceLastTick = true;
	OnStatsChanged.Broadcast();
	return true;
}

void UPSPlayerStatsComponent::SetSurvivalValues(float NewHunger, float NewMentalHealth)
{
	if (!FMath::IsFinite(NewHunger) || !FMath::IsFinite(NewMentalHealth)) return;
	NewHunger = FMath::Clamp(NewHunger, 0.0f, 100.0f);
	NewMentalHealth = FMath::Clamp(NewMentalHealth, 0.0f, 100.0f);
	if (Hunger == NewHunger && MentalHealth == NewMentalHealth) return;
	Hunger = NewHunger;
	MentalHealth = NewMentalHealth;
	OnStatsChanged.Broadcast();
}

void UPSPlayerStatsComponent::SetActionActive(bool bActive)
{
	bActionActive = bActive;
}

void UPSPlayerStatsComponent::ResetStats()
{
	Health = MaxHealth;
	Stamina = MaxStamina;
	Hunger = MentalHealth = 100.0f;
	bActionActive = bConsumedSinceLastTick = false;
	OnStatsChanged.Broadcast();
}
