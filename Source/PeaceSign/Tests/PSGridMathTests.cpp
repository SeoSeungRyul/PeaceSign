// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../World/PSTileTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPSGridCoordinateTest,
	"PeaceSign.Grid.CoordinateConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSGridCoordinateTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Positive world position maps to a cell"), PSGrid::WorldToCell(FVector(235.0f, 470.0f, 0.0f), 100.0f), FIntPoint(2, 4));
	TestEqual(TEXT("Negative world position floors to the previous cell"), PSGrid::WorldToCell(FVector(-1.0f, -100.1f, 0.0f), 100.0f), FIntPoint(-1, -2));
	TestEqual(TEXT("Negative cell maps to the previous chunk"), PSGrid::CellToChunk(FIntPoint(-1, 0), 16), FIntPoint(-1, 0));
	TestEqual(TEXT("Negative cell has a positive local coordinate"), PSGrid::CellToLocal(FIntPoint(-1, 0), 16), FIntPoint(15, 0));
	TestEqual(TEXT("Negative chunk boundary stays in its chunk"), PSGrid::CellToChunk(FIntPoint(-16, -16), 16), FIntPoint(-1, -1));
	TestEqual(TEXT("Negative chunk boundary starts at local zero"), PSGrid::CellToLocal(FIntPoint(-16, -16), 16), FIntPoint::ZeroValue);
	return true;
}

#endif
