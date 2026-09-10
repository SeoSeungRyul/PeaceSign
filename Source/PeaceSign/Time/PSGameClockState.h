#pragma once

#include "CoreMinimal.h"

// 24 uninterrupted game hours = 720 play seconds. Dates advance at midnight.
struct FPSGameClockState
{
	static constexpr double SecondsPerStep = 15.0;
	static constexpr int32 StepsPerDay = 48;
	int64 Day = 1;
	int32 Step = 12; // 06:00, in half-hour steps since midnight.
	double RemainingSeconds = 0.0;

	int32 GetMinuteOfDay() const { return Step * 30; }

	bool Advance(double Seconds)
	{
		if (!FMath::IsFinite(Seconds) || Seconds <= 0.0) return false;
		RemainingSeconds += Seconds;
		const int64 Steps = FMath::FloorToInt64(RemainingSeconds / SecondsPerStep);
		if (Steps == 0) return false;
		RemainingSeconds -= static_cast<double>(Steps) * SecondsPerStep;
		const int64 TotalSteps = Step + Steps;
		Day += TotalSteps / StepsPerDay;
		Step = static_cast<int32>(TotalSteps % StepsPerDay);
		return true;
	}
};
