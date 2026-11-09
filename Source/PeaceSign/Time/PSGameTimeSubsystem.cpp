#include "PSGameTimeSubsystem.h"
#include "Engine/World.h"

void UPSGameTimeSubsystem::Tick(float DeltaTime)
{
	const int64 Previous = FMath::FloorToInt64(GetAbsoluteGameMinute() / 10.0);
	const bool bHalfHourChanged = Clock.Advance(DeltaTime);
	if (FMath::FloorToInt64(GetAbsoluteGameMinute() / 10.0) != Previous) OnTenMinuteChanged.Broadcast();
	if (bHalfHourChanged) OnClockChanged.Broadcast();
}

void UPSGameTimeSubsystem::RestoreClock(const int64 HalfHourIndex, const double SecondsIntoStep)
{
	Clock.Restore(HalfHourIndex, SecondsIntoStep);
	OnTenMinuteChanged.Broadcast();
	OnClockChanged.Broadcast();
}

void UPSGameTimeSubsystem::AdvanceGameHours(const int32 Hours)
{
	if (Hours > 0 && Clock.AdvanceHalfHours(static_cast<int64>(Hours) * 2))
	{
		OnTenMinuteChanged.Broadcast();
		OnClockChanged.Broadcast();
	}
}

void UPSGameTimeSubsystem::AdvanceGameDays(const int32 Days)
{
	if (Days > 0 && Clock.AdvanceHalfHours(static_cast<int64>(Days) * FPSGameClockState::StepsPerDay))
	{
		OnTenMinuteChanged.Broadcast();
		OnClockChanged.Broadcast();
	}
}

TStatId UPSGameTimeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPSGameTimeSubsystem, STATGROUP_Tickables);
}

bool UPSGameTimeSubsystem::IsTickable() const
{
	return Super::IsTickable() && GetWorld() && GetWorld()->HasBegunPlay();
}

bool UPSGameTimeSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
