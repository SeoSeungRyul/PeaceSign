#pragma once

#include "CoreMinimal.h"

namespace PSCropGrowth
{
	constexpr int32 MaxStage = 5;
	constexpr int64 HalfHoursPerStage = 48;
	constexpr int64 MaxElapsedHalfHours = (MaxStage - 1) * HalfHoursPerStage;
	constexpr int32 MinutesPerDay = 24 * 60;
	constexpr int32 DawnMinute = 6 * 60;
	constexpr int32 LegacyGrowthMinutes = 4 * MinutesPerDay;
	inline uint8 GetStageForMinutes(const double Minutes, const int32 Required)
	{
		if (Minutes >= Required) return 5;
		if (Minutes * 5 >= Required * 3.0) return 4;
		if (Minutes * 5 >= Required * 2.0) return 3;
		if (Minutes * 5 >= Required) return 2;
		return 1;
	}
	inline double MinutesForLegacyStage(const int32 Stage, const int32 Required)
	{
		return Stage >= MaxStage ? Required : Required * (FMath::Clamp(Stage, 1, 4) - 1) / 5.0;
	}
	inline uint8 GetStage(int64 PlantedHalfHour, int64 CurrentHalfHour)
	{
		const int64 Elapsed = FMath::Max<int64>(0, CurrentHalfHour - PlantedHalfHour);
		return static_cast<uint8>(1 + FMath::Min<int64>(MaxStage - 1, Elapsed / HalfHoursPerStage));
	}
}
