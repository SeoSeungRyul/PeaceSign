#include "PSGameTimeSubsystem.h"
#include "Engine/World.h"

void UPSGameTimeSubsystem::Tick(float DeltaTime)
{
	if (Clock.Advance(DeltaTime)) OnClockChanged.Broadcast();
}

void UPSGameTimeSubsystem::RestoreClock(const int64 HalfHourIndex, const double SecondsIntoStep)
{
	Clock.Restore(HalfHourIndex, SecondsIntoStep);
	OnClockChanged.Broadcast();
}

void UPSGameTimeSubsystem::AdvanceGameHours(const int32 Hours)
{
	if (Hours > 0 && Clock.AdvanceHalfHours(static_cast<int64>(Hours) * 2))
	{
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
