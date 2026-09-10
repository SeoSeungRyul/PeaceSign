#pragma once

#include "CoreMinimal.h"

namespace PSCropGrowth
{
	constexpr int32 MaxStage = 5;
	constexpr int64 HalfHoursPerStage = 2;
	inline uint8 GetStage(int64 PlantedHalfHour, int64 CurrentHalfHour)
	{
		const int64 Elapsed = FMath::Max<int64>(0, CurrentHalfHour - PlantedHalfHour);
		return static_cast<uint8>(1 + FMath::Min<int64>(MaxStage - 1, Elapsed / HalfHoursPerStage));
	}
}
