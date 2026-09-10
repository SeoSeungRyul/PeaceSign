#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Time/PSGameClockState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSGameTimeTest, "PeaceSign.Time.Clock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSGameTimeTest::RunTest(const FString& Parameters)
{
	FPSGameClockState Clock;
	TestEqual(TEXT("Starts at 6 AM"), Clock.GetMinuteOfDay(), 360);
	TestFalse(TEXT("No early display update"), Clock.Advance(14.0));
	TestTrue(TEXT("15 seconds advances half an hour"), Clock.Advance(1.0));
	TestEqual(TEXT("6:30 AM"), Clock.GetMinuteOfDay(), 390);
	Clock.Advance(165.0);
	TestEqual(TEXT("Noon after three real minutes"), Clock.GetMinuteOfDay(), 720);
	Clock.Advance(360.0);
	TestEqual(TEXT("Midnight wraps correctly"), Clock.GetMinuteOfDay(), 0);
	TestEqual(TEXT("Date changes at midnight"), Clock.Day, int64(2));
	Clock.Advance(45.0);
	TestEqual(TEXT("1:30 AM"), Clock.GetMinuteOfDay(), 90);
	Clock.Advance(15.0);
	TestEqual(TEXT("2 AM is not skipped"), Clock.GetMinuteOfDay(), 120);
	Clock.Advance(15.0);
	TestEqual(TEXT("Night continues to 2:30 AM"), Clock.GetMinuteOfDay(), 150);
	Clock.Advance(105.0);
	TestEqual(TEXT("720 seconds returns to 6 AM"), Clock.GetMinuteOfDay(), 360);
	TestEqual(TEXT("One full day elapsed"), Clock.Day, int64(2));
	Clock.Advance(1456.0);
	TestEqual(TEXT("Long frame advances multiple days"), Clock.Day, int64(4));
	TestEqual(TEXT("Long frame retains half-hour slot"), Clock.GetMinuteOfDay(), 390);
	TestEqual(TEXT("Remainder preserved"), Clock.RemainingSeconds, 1.0);
	TestFalse(TEXT("Negative delta ignored"), Clock.Advance(-15.0));
	TestEqual(TEXT("Invalid delta leaves time intact"), Clock.GetMinuteOfDay(), 390);
	FPSGameClockState FineClock;
	for (int32 I = 0; I < 1500; ++I) FineClock.Advance(0.01);
	// Floating-point input may land microscopically below a boundary; no time is discarded.
	FineClock.Advance(0.00001);
	TestEqual(TEXT("Small frame deltas accumulate"), FineClock.GetMinuteOfDay(), 390);
	return true;
}
#endif
