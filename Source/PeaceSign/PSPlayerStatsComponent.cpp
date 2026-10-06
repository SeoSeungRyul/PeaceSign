#include "PSPlayerStatsComponent.h"
#include "Time/PSGameTimeSubsystem.h"
#include "Engine/World.h"

namespace
{
double ClockHours(const UPSGameTimeSubsystem* Clock)
{
	return static_cast<double>(Clock->GetHalfHourIndex()) * 0.5 + Clock->GetSecondsIntoStep() / 60.0;
}
}

void UPSPlayerStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const auto* Clock = GetWorld()->GetSubsystem<UPSGameTimeSubsystem>())
	{
		LastClockHours = ClockHours(Clock);
		bClockInitialized = true;
	}
}

UPSPlayerStatsComponent::UPSPlayerStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPSPlayerStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (const auto* Clock = GetWorld() ? GetWorld()->GetSubsystem<UPSGameTimeSubsystem>() : nullptr)
	{
		const double Now = ClockHours(Clock);
		const double Elapsed = bClockInitialized ? FMath::Max(0.0, Now - LastClockHours) : 0.0;
		LastClockHours = Now;
		bClockInitialized = true;
		AdvanceSurvivalTime(Elapsed);
	}
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
	NewMentalHealth = FMath::Clamp(NewMentalHealth, 0.0f, NewHunger == 0.0f ? 50.0f : 100.0f);
	if (Hunger == NewHunger && MentalHealth == NewMentalHealth) return;
	Hunger = NewHunger;
	MentalHealth = NewMentalHealth;
	UpdateSurvivalLimits();
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
	Hunger = MentalHealth = 100.0f;
	UpdateSurvivalLimits();
	Health = MaxHealth;
	Stamina = MaxStamina;
	NotifySlept();
	bActionActive = bMovementActive = bConsumedSinceLastTick = false;
	OnStatsChanged.Broadcast();
}

void UPSPlayerStatsComponent::UpdateSurvivalLimits()
{
	const bool bStarving = Hunger == 0.0f;
	const bool bMentalDepleted = MentalHealth == 0.0f;
	MaxMentalHealth = bStarving ? 50.0f : 100.0f;
	MaxHealth = bMentalDepleted ? 10.0f : 100.0f;
	MaxStamina = (bStarving ? 50.0f : 100.0f) * (bMentalDepleted ? 0.1f : 1.0f);
	Health = FMath::Min(Health, MaxHealth);
	Stamina = FMath::Min(Stamina, MaxStamina);
	MentalHealth = FMath::Min(MentalHealth, MaxMentalHealth);
}

void UPSPlayerStatsComponent::RestoreHunger(float Amount)
{
	if (Health > 0.0f && FMath::IsFinite(Amount) && Amount > 0.0f)
		SetSurvivalValues(Hunger + Amount, MentalHealth);
}

void UPSPlayerStatsComponent::RestoreMentalHealth(float Amount)
{
	if (Health > 0.0f && FMath::IsFinite(Amount) && Amount > 0.0f)
		SetSurvivalValues(Hunger, MentalHealth + Amount);
}

void UPSPlayerStatsComponent::ConsumeMentalHealth(float Amount)
{
	if (Health > 0.0f && FMath::IsFinite(Amount) && Amount > 0.0f)
		SetSurvivalValues(Hunger, MentalHealth - Amount);
}

void UPSPlayerStatsComponent::NotifySlept()
{
	AwakeGameHours = 0.0;
	if (const auto* Clock = GetWorld() ? GetWorld()->GetSubsystem<UPSGameTimeSubsystem>() : nullptr)
	{
		LastClockHours = ClockHours(Clock);
		bClockInitialized = true;
	}
}

void UPSPlayerStatsComponent::AdvanceSurvivalTime(double GameHours)
{
	if (Health <= 0.0f || !FMath::IsFinite(GameHours) || GameHours <= 0.0) return;
	const double SleeplessHours = FMath::Max(0.0, AwakeGameHours + GameHours - 48.0) - FMath::Max(0.0, AwakeGameHours - 48.0);
	AwakeGameHours += GameHours;
	const float HungerRate = FMath::IsFinite(HungerLossPerGameHour) ? FMath::Max(0.0f, HungerLossPerGameHour) : 0.0f;
	const float MentalRate = FMath::IsFinite(SleeplessMentalLossPerGameHour) ? FMath::Max(0.0f, SleeplessMentalLossPerGameHour) : 0.0f;
	SetSurvivalValues(static_cast<float>(FMath::Max(0.0, Hunger - GameHours * HungerRate)),
		static_cast<float>(FMath::Max(0.0, MentalHealth - SleeplessHours * MentalRate)));
}
