#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSTileChunkActor.h"
#include "../World/PSTileTypes.h"
#include "../PSPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSWaterCollisionTest, "PeaceSign.Fishing.WaterCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSWaterCollisionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	const FVector Origin(-800, 600, 0);
	APSTileChunkActor* Chunk = World->SpawnActor<APSTileChunkActor>(Origin, FRotator::ZeroRotator);
	FPSChunkData Data;
	Data.Cells.SetNum(16);
	for (FPSTileCell& Cell : Data.Cells) Cell.GroundType = EPSTileType::Grass;
	for (int32 Y = 1; Y <= 2; ++Y)
		for (int32 X = 1; X <= 2; ++X) Data.Cells[Y * 4 + X].GroundType = EPSTileType::Water;
	Chunk->Rebuild(Data, 4, 100);
	APSPlayerCharacter* Player = World->SpawnActor<APSPlayerCharacter>(Origin + FVector(-100, 200, 50), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Character spawned"), Player)) { World->DestroyWorld(false); return false; }
	const auto Sweep = [&](const FVector& Start, const FVector& Delta)
	{
		Player->SetActorLocation(Origin + Start, false, nullptr, ETeleportType::TeleportPhysics);
		FHitResult Hit;
		Player->AddActorWorldOffset(Delta, true, &Hit);
		return Hit.IsValidBlockingHit();
	};
	TestTrue(TEXT("Long movement cannot tunnel through the lake"), Sweep(FVector(-100, 200, 50), FVector(600, 0, 0)));
	TestTrue(TEXT("Character capsule stops before water edge"), Player->GetActorLocation().X + Player->GetCapsuleComponent()->GetScaledCapsuleRadius() <= Origin.X + 100.1f);
	TestTrue(TEXT("Opposite shore blocks movement"), Sweep(FVector(500, 200, 50), FVector(-600, 0, 0)));
	TestTrue(TEXT("North shore blocks movement"), Sweep(FVector(200, -100, 50), FVector(0, 600, 0)));
	TestTrue(TEXT("South shore blocks movement"), Sweep(FVector(200, 500, 50), FVector(0, -600, 0)));
	TestTrue(TEXT("Diagonal entry is blocked"), Sweep(FVector(0, 0, 50), FVector(400, 400, 0)));
	TestFalse(TEXT("Dry ground remains traversable"), Sweep(FVector(-100, 50, 50), FVector(600, 0, 0)));
	Chunk->Rebuild(Data, 4, 100);
	TestTrue(TEXT("Collision survives chunk rebuild"), Sweep(FVector(-100, 200, 50), FVector(600, 0, 0)));
	for (FPSTileCell& Cell : Data.Cells) Cell.GroundType = EPSTileType::Grass;
	Chunk->Rebuild(Data, 4, 100);
	TestFalse(TEXT("Removing water removes old collision"), Sweep(FVector(-100, 200, 50), FVector(600, 0, 0)));
	Data.Cells[5].GroundType = EPSTileType::Water;
	Chunk->Rebuild(Data, 4, 100);
	TestTrue(TEXT("Regenerated water blocks again"), Sweep(FVector(-100, 150, 50), FVector(600, 0, 0)));
	Chunk->Destroy();
	TestFalse(TEXT("Unloaded chunk leaves no collision"), Sweep(FVector(-100, 150, 50), FVector(600, 0, 0)));
	World->DestroyWorld(false);
	return true;
}
#endif
