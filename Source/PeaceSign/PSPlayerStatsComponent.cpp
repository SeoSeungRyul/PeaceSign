#include "PSPlayerStatsComponent.h"

UPSPlayerStatsComponent::UPSPlayerStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPSPlayerStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Health > 0.0f && !bMovementActive && !bActionActive && !bConsumedSinceLastTick && FMath::IsFinite(DeltaTime) && DeltaTime > 0.0f)
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

void UPSPlayerStatsComponent::SetMovementActive(bool bActive)
{
	bMovementActive = bActive;
}

void UPSPlayerStatsComponent::ResetStats()
{
	Health = MaxHealth;
	Stamina = MaxStamina;
	Hunger = MentalHealth = 100.0f;
	bActionActive = bMovementActive = bConsumedSinceLastTick = false;
	OnStatsChanged.Broadcast();
}
