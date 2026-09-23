#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Time/PSGameClockState.h"
#include "../Time/PSGameTimeSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSGameTimeTest, "PeaceSign.Time.Clock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSGameTimeTest::RunTest(const FString& Parameters)
{
	FPSGameClockState Clock;
	TestEqual(TEXT("Starts at 6 AM"), Clock.GetMinuteOfDay(), 360);
	TestFalse(TEXT("No early display update"), Clock.Advance(29.0));
	TestTrue(TEXT("30 seconds advances half an hour"), Clock.Advance(1.0));
	TestEqual(TEXT("6:30 AM"), Clock.GetMinuteOfDay(), 390);
	Clock.Advance(330.0);
	TestEqual(TEXT("Noon after six real minutes"), Clock.GetMinuteOfDay(), 720);
	Clock.Advance(720.0);
	TestEqual(TEXT("Midnight wraps correctly"), Clock.GetMinuteOfDay(), 0);
	TestEqual(TEXT("Date changes at midnight"), Clock.Day, int64(2));
	Clock.Advance(90.0);
	TestEqual(TEXT("1:30 AM"), Clock.GetMinuteOfDay(), 90);
	Clock.Advance(30.0);
	TestEqual(TEXT("2 AM is not skipped"), Clock.GetMinuteOfDay(), 120);
	Clock.Advance(30.0);
	TestEqual(TEXT("Night continues to 2:30 AM"), Clock.GetMinuteOfDay(), 150);
	Clock.Advance(210.0);
	TestEqual(TEXT("1440 seconds returns to 6 AM"), Clock.GetMinuteOfDay(), 360);
	TestEqual(TEXT("One full day elapsed"), Clock.Day, int64(2));
	Clock.Advance(2911.0);
	TestEqual(TEXT("Long frame advances multiple days"), Clock.Day, int64(4));
	TestEqual(TEXT("Long frame retains half-hour slot"), Clock.GetMinuteOfDay(), 390);
	TestEqual(TEXT("Remainder preserved"), Clock.RemainingSeconds, 1.0);
	TestFalse(TEXT("Negative delta ignored"), Clock.Advance(-30.0));
	TestEqual(TEXT("Invalid delta leaves time intact"), Clock.GetMinuteOfDay(), 390);
	FPSGameClockState FineClock;
	for (int32 I = 0; I < 3000; ++I) FineClock.Advance(0.01);
	// Floating-point input may land microscopically below a boundary; no time is discarded.
	FineClock.Advance(0.00001);
	TestEqual(TEXT("Small frame deltas accumulate"), FineClock.GetMinuteOfDay(), 390);
	FPSGameClockState RestoredClock;
	RestoredClock.Restore(109, 12.5);
	TestEqual(TEXT("Restored clock keeps the day"), RestoredClock.Day, int64(3));
	TestEqual(TEXT("Restored clock keeps the half-hour"), RestoredClock.GetMinuteOfDay(), 390);
	TestEqual(TEXT("Restored clock keeps partial-step time"), RestoredClock.RemainingSeconds, 12.5);
	RestoredClock.Advance(17.5);
	TestEqual(TEXT("Restored partial step continues"), RestoredClock.GetMinuteOfDay(), 420);
	RestoredClock.AdvanceHalfHours(12);
	TestEqual(TEXT("Six-hour skip advances twelve half-hours"), RestoredClock.GetMinuteOfDay(), 780);
	TestEqual(TEXT("Six-hour skip preserves partial-step time"), RestoredClock.RemainingSeconds, 0.0);
	TestEqual(TEXT("Day one is spring"), UPSGameTimeSubsystem::GetSeasonIndexForDay(1), 0);
	TestEqual(TEXT("Day 28 remains spring"), UPSGameTimeSubsystem::GetSeasonIndexForDay(28), 0);
	TestEqual(TEXT("Day 29 starts summer"), UPSGameTimeSubsystem::GetSeasonIndexForDay(29), 1);
	TestEqual(TEXT("Seasons wrap after 112 days"), UPSGameTimeSubsystem::GetSeasonIndexForDay(113), 0);
	TestEqual(TEXT("Season day resets at the boundary"), UPSGameTimeSubsystem::GetDayOfSeasonForDay(29), 1);
	FPSGameClockState SeasonClock;
	SeasonClock.AdvanceHalfHours(28 * FPSGameClockState::StepsPerDay);
	TestEqual(TEXT("Advancing 28 days preserves the time of day"), SeasonClock.GetMinuteOfDay(), 360);
	TestEqual(TEXT("Advancing 28 days reaches the next season"), UPSGameTimeSubsystem::GetSeasonIndexForDay(SeasonClock.Day), 1);
	return true;
}
#endif
