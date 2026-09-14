#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PSGameClockState.h"
#include "PSGameTimeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPSClockChanged);

UCLASS()
class PEACESIGN_API UPSGameTimeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintPure, Category = "Game Time")
	int32 GetMinuteOfDay() const { return Clock.GetMinuteOfDay(); }
	UFUNCTION(BlueprintPure, Category = "Game Time")
	int64 GetDay() const { return Clock.Day; }
	UFUNCTION(BlueprintPure, Category = "Game Time")
	int64 GetHalfHourIndex() const { return Clock.GetHalfHourIndex(); }
	double GetSecondsIntoStep() const { return Clock.RemainingSeconds; }
	void RestoreClock(int64 HalfHourIndex, double SecondsIntoStep);
	UFUNCTION(BlueprintCallable, Category = "Game Time")
	void AdvanceGameHours(int32 Hours);
	UPROPERTY(BlueprintAssignable, Category = "Game Time")
	FPSClockChanged OnClockChanged;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	FPSGameClockState Clock;
};
