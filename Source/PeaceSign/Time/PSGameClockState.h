#pragma once

#include "CoreMinimal.h"

// One real minute equals one game hour. Dates advance at midnight.
struct FPSGameClockState
{
	static constexpr double SecondsPerStep = 30.0;
	static constexpr int32 StepsPerDay = 48;
	int64 Day = 1;
	int32 Step = 12; // 06:00, in half-hour steps since midnight.
	double RemainingSeconds = 0.0;

	int32 GetMinuteOfDay() const { return Step * 30; }
	int64 GetHalfHourIndex() const { return (Day - 1) * StepsPerDay + Step; }

	void Restore(const int64 HalfHourIndex, const double SecondsIntoStep)
	{
		const int64 SafeIndex = FMath::Max<int64>(0, HalfHourIndex);
		Day = SafeIndex / StepsPerDay + 1;
		Step = static_cast<int32>(SafeIndex % StepsPerDay);
		RemainingSeconds = FMath::IsFinite(SecondsIntoStep)
			? FMath::Clamp(SecondsIntoStep, 0.0, SecondsPerStep - UE_DOUBLE_SMALL_NUMBER)
			: 0.0;
	}

	bool AdvanceHalfHours(const int64 HalfHours)
	{
		if (HalfHours <= 0) return false;
		const int64 TotalSteps = static_cast<int64>(Step) + HalfHours;
		Day += TotalSteps / StepsPerDay;
		Step = static_cast<int32>(TotalSteps % StepsPerDay);
		return true;
	}

	bool Advance(double Seconds)
	{
		if (!FMath::IsFinite(Seconds) || Seconds <= 0.0) return false;
		RemainingSeconds += Seconds;
		const int64 Steps = FMath::FloorToInt64(RemainingSeconds / SecondsPerStep);
		if (Steps == 0) return false;
		RemainingSeconds -= static_cast<double>(Steps) * SecondsPerStep;
		return AdvanceHalfHours(Steps);
	}
};
