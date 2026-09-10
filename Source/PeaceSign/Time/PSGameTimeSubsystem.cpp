#include "PSGameTimeSubsystem.h"
#include "Engine/World.h"

void UPSGameTimeSubsystem::Tick(float DeltaTime)
{
	if (Clock.Advance(DeltaTime)) OnClockChanged.Broadcast();
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
