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
	/** 0=봄, 1=여름, 2=가을, 3=겨울. 한 계절은 28일이다. */
	UFUNCTION(BlueprintPure, Category = "Game Time")
	int32 GetSeasonIndex() const { return GetSeasonIndexForDay(Clock.Day); }
	UFUNCTION(BlueprintPure, Category = "Game Time")
	int32 GetDayOfSeason() const { return GetDayOfSeasonForDay(Clock.Day); }
	static int32 GetSeasonIndexForDay(int64 Day) { return static_cast<int32>(((FMath::Max<int64>(1, Day) - 1) / DaysPerSeason) % 4); }
	static int32 GetDayOfSeasonForDay(int64 Day) { return static_cast<int32>((FMath::Max<int64>(1, Day) - 1) % DaysPerSeason) + 1; }
	UFUNCTION(BlueprintPure, Category = "Game Time")
	int64 GetHalfHourIndex() const { return Clock.GetHalfHourIndex(); }
	double GetSecondsIntoStep() const { return Clock.RemainingSeconds; }
	void RestoreClock(int64 HalfHourIndex, double SecondsIntoStep);
	UFUNCTION(BlueprintCallable, Category = "Game Time")
	void AdvanceGameHours(int32 Hours);
	UFUNCTION(BlueprintCallable, Category = "Game Time")
	void AdvanceGameDays(int32 Days);
	UPROPERTY(BlueprintAssignable, Category = "Game Time")
	FPSClockChanged OnClockChanged;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	static constexpr int32 DaysPerSeason = 28;
	FPSGameClockState Clock;
};
