#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerStatsComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSPlayerStatsTest, "PeaceSign.Player.Stats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSPlayerStatsTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues InitValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitValues);
	AActor* Owner = World->SpawnActor<AActor>();
	UPSPlayerStatsComponent* Stats = NewObject<UPSPlayerStatsComponent>(Owner);
	Stats->RegisterComponent();
	TestTrue(TEXT("Attack spends 10"), Stats->TryConsumeStamina(10.0f));
	TestEqual(TEXT("Remaining SP"), Stats->Stamina, 90.0f);
	TestFalse(TEXT("Insufficient SP rejects action"), Stats->TryConsumeStamina(100.0f));
	TestEqual(TEXT("Rejected action does not spend"), Stats->Stamina, 90.0f);
	TestFalse(TEXT("Negative cost is rejected"), Stats->TryConsumeStamina(-10.0f));
	TestTrue(TEXT("One second of running spends 5 SP"), Stats->TryConsumeStamina(5.0f));
	TestEqual(TEXT("SP after one second of running"), Stats->Stamina, 85.0f);
	Stats->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("No regeneration during consumption tick"), Stats->Stamina, 85.0f);
	Stats->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Idle regeneration is 10 per second"), Stats->Stamina, 86.0f);
	Stats->SetActionActive(true);
	Stats->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Active action blocks regeneration"), Stats->Stamina, 86.0f);
	Stats->SetActionActive(false);
	Stats->SetMovementActive(true);
	Stats->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Movement blocks regeneration"), Stats->Stamina, 86.0f);
	Stats->SetMovementActive(false);
	Stats->ApplyDamage(30.0f);
	Stats->RestoreHealth(1000.0f);
	TestEqual(TEXT("Healing caps at maximum"), Stats->Health, 100.0f);
	Stats->ApplyDamage(200.0f);
	TestEqual(TEXT("Damage clamps to zero"), Stats->Health, 0.0f);
	Stats->RestoreHealth(100.0f);
	TestEqual(TEXT("Healing cannot revive"), Stats->Health, 0.0f);
	TestFalse(TEXT("Dead pawn cannot spend SP"), Stats->TryConsumeStamina(10.0f));
	Stats->SetSurvivalValues(-1.0f, 150.0f);
	TestEqual(TEXT("Hunger clamps"), Stats->Hunger, 0.0f);
	TestEqual(TEXT("Starvation caps mental health"), Stats->MentalHealth, 50.0f);
	Stats->ResetStats();
	TestEqual(TEXT("Explicit revival restores HP"), Stats->Health, 100.0f);
	Stats->SetSurvivalValues(0.0f, 0.0f);
	TestEqual(TEXT("Combined penalties multiply SP limits"), Stats->MaxStamina, 5.0f);
	TestEqual(TEXT("Mental depletion caps HP"), Stats->Health, 10.0f);
	TestFalse(TEXT("Reduced SP cannot pay attack cost"), Stats->TryConsumeStamina(10.0f));
	Stats->RestoreHunger(20.0f);
	TestEqual(TEXT("Food removes starvation penalty"), Stats->MaxMentalHealth, 100.0f);
	TestEqual(TEXT("Mental penalty remains after eating"), Stats->MaxStamina, 10.0f);
	Stats->RestoreMentalHealth(1.0f);
	TestEqual(TEXT("Mental recovery restores HP limit"), Stats->MaxHealth, 100.0f);
	TestEqual(TEXT("Removing penalty does not heal"), Stats->Health, 10.0f);
	TestEqual(TEXT("Removing penalty does not refill SP"), Stats->Stamina, 5.0f);
	Stats->ResetStats();
	Stats->AdvanceSurvivalTime(24.0);
	TestEqual(TEXT("One day exhausts provisional hunger supply"), Stats->Hunger, 0.0f);
	Stats->ResetStats();
	Stats->HungerLossPerGameHour = 0.0f;
	Stats->AdvanceSurvivalTime(47.0);
	TestEqual(TEXT("No mental decay before two days awake"), Stats->MentalHealth, 100.0f);
	Stats->AdvanceSurvivalTime(3.0);
	TestEqual(TEXT("Only time beyond threshold incurs decay"), Stats->MentalHealth, 90.0f);
	Stats->NotifySlept();
	Stats->AdvanceSurvivalTime(1.0);
	TestEqual(TEXT("Sleep resets awake timer without mental refill"), Stats->MentalHealth, 90.0f);
	Stats->ConsumeMentalHealth(1000.0f);
	TestEqual(TEXT("Monster encounter clamps mental damage"), Stats->MentalHealth, 0.0f);
	Stats->ResetStats();
	Stats->AdvanceSurvivalTime(-1.0);
	TestEqual(TEXT("Negative time is ignored"), Stats->Hunger, 100.0f);
	World->DestroyWorld(false);
	return true;
}
#endif
