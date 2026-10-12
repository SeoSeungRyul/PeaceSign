#pragma once

#include "PSTileTypes.h"

namespace PSTileStamp
{
	PEACESIGN_API FIntPoint GetSize(const FPSTileStamp& Stamp);
	PEACESIGN_API bool BuildCell(const FPSTileStamp& Stamp, FIntPoint WorldCell, FPSTileCell& OutCell);
}
