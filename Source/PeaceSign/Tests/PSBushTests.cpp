#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Gathering/PSBushWorldComponent.h"
#include "../Gathering/PSBushInteractionComponent.h"
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../World/PSGridWorld.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSBushTest, "PeaceSign.Gathering.BushHoldRewardsAndSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSBushTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	const FString Slot = TEXT("BushTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot;
	Grid->Gathering->bEnableTestArea = false;
	Grid->FinishSpawning(FTransform::Identity);
	// A real saved grass patch excludes procedural stone/water from input boundary tests.
	for (int32 X = -10; X < 10; ++X)
		for (int32 Y = -10; Y < 10; ++Y)
		{
			const FIntPoint Cell(X, Y);
			auto& Chunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(Cell, Grid->ChunkSize));
			auto& Tile = Chunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, Grid->ChunkSize), Grid->ChunkSize)];
			Tile.GroundType = (X == -10 ? EPSTileType::Water : EPSTileType::Grass);
			Tile.ObjectType = EPSWorldObjectType::None;
			auto& Saved = Grid->ModifiedChunks.FindOrAdd(Chunk.Coordinate);
			Saved.Coordinate = Chunk.Coordinate;
			Saved.Cells = Chunk.Cells;
		}
	Grid->Bushes->bEnabled = true;
	Grid->Bushes->Initialize();
	TestEqual(TEXT("The prototype patch creates eight bushes"), Grid->Bushes->GetRecords().Num(), 8);
	for (const auto& Bush : Grid->Bushes->GetRecords())
		TestEqual(TEXT("Every bush appears on grass"), Grid->GetGroundTile(Bush.Cell), EPSTileType::Grass);
	if (Grid->Bushes->GetRecords().IsEmpty()) { World->DestroyWorld(false); return false; }
	const FIntPoint Target = Grid->Bushes->GetRecords()[0].Cell;
	auto* Controller = World->SpawnActor<APSPlayerController>();
	Controller->GridWorld = Grid;
	auto* Inventory = Controller->GetInventoryComponent();
	Inventory->bAutoSave = false;
	Inventory->SaveSlotName = Slot + TEXT("Inventory");
	Inventory->ResetToDefaults();
	auto* Player = World->SpawnActor<APSPlayerCharacter>(Grid->CellToWorldCenter(Target) + FVector(0, 0, 100), FRotator::ZeroRotator);
	Controller->Possess(Player);
	auto* Action = Controller->BushInteraction.Get();
	Controller->HandleInteract();
	TestTrue(TEXT("F targets a nearby bush through the player controller"), Action->IsActive());
	Action->Advance(0.79f);
	Controller->HandleInteractReleased();
	TestEqual(TEXT("Early release spends no harvest"), Grid->Bushes->Find(Target)->RemainingHarvests, 3);
	TestEqual(TEXT("Early release grants nothing"), Inventory->CountItem(EPSItemType::GatheredFruit), 0);
	TestTrue(TEXT("Right click targets the cursor bush"), Action->Press(Grid, true, Target));
	Action->Advance(0.81f);
	TestEqual(TEXT("Completed right click spends one harvest"), Grid->Bushes->Find(Target)->RemainingHarvests, 2);
	const int32 FirstReward = Inventory->CountItem(EPSItemType::GatheredFruit);
	TestTrue(TEXT("Reward is one to three berries"), FirstReward >= 1 && FirstReward <= 3);
	TestTrue(TEXT("Actual inventory acquisition registers the berry"), Inventory->GetDiscoveredGatheringItems().Contains(TEXT("Forage.WildStrawberry")));
	TestEqual(TEXT("First harvest starts two game days of fruit cooldown"),
		Grid->Bushes->Find(Target)->ReadyAt - Grid->Gathering->Now(), static_cast<int64>(96));
	Action->Advance(5);
	TestEqual(TEXT("A held completed input cannot pay twice"), Inventory->CountItem(EPSItemType::GatheredFruit), FirstReward);
	Grid->LoadWorld();
	TestEqual(TEXT("Harvest count survives reload"), Grid->Bushes->Find(Target)->RemainingHarvests, 2);
	auto* Clock = World->GetSubsystem<UPSGameTimeSubsystem>();
	Clock->AdvanceGameDays(2);
	Action->Press(Grid, false);
	Action->Advance(0.4f);
	Player->SetActorLocation(Player->GetActorLocation() + FVector(1, 0, 0));
	Action->Advance(0.5f);
	TestFalse(TEXT("Movement cancels a held harvest"), Action->IsActive());
	TestEqual(TEXT("Movement cancellation spends no harvest"), Grid->Bushes->Find(Target)->RemainingHarvests, 2);
	Action->Press(Grid, false);
	Action->Advance(0.81f);
	TestEqual(TEXT("F completes the second harvest"), Grid->Bushes->Find(Target)->RemainingHarvests, 1);
	Clock->AdvanceGameDays(2);
	auto Records = Grid->Bushes->GetRecords();
	for (auto& Bush : Records)
		if (Bush.Cell == Target)
			for (int32 Sequence = 0; Sequence < 100; ++Sequence)
			{
				FRandomStream Random(Grid->GetGatheringSeed() ^ GetTypeHash(Target) ^ Sequence);
				if (Random.RandRange(1, 3) == 3) { Bush.HarvestSequence = Sequence; break; }
			}
	Grid->Bushes->Restore(Records, true);
	Inventory->ResetToDefaults();
	Inventory->AddItem(EPSItemType::Stone, 8 * 999);
	Inventory->RemoveItem(EPSItemType::Stone, 999);
	Inventory->AddItemVariant(EPSItemType::GatheredFruit, TEXT("Forage.WildStrawberry"), 998);
	Action->Press(Grid, true, Target);
	Action->Advance(0.81f);
	TestEqual(TEXT("Existing stack accepts only the available one berry"), Inventory->CountItem(EPSItemType::GatheredFruit), 999);
	TestFalse(TEXT("Third harvest removes the bush occupant"), Grid->Bushes->HasBush(Target));
	TestEqual(TEXT("Only two excess berries are dropped"), Grid->Gathering->GetDrops().Num(), 1);
	if (!Grid->Gathering->GetDrops().IsEmpty())
		TestEqual(TEXT("Excess drop quantity is exact"), Grid->Gathering->GetDrops()[0].Item.Quantity, 2);
	Clock->AdvanceGameDays(3);
	Grid->Bushes->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Respawn retains the patch population"), Grid->Bushes->GetRecords().Num(), 8);
	for (const auto& Bush : Grid->Bushes->GetRecords())
		TestTrue(TEXT("Every removed bush is replaced after three days"), Bush.RemainingHarvests > 0);
	Inventory->SaveInventory();
	TestTrue(TEXT("Inventory and discovered entries reload"), Inventory->LoadInventory());
	TestTrue(TEXT("Discovery survives reloading"), Inventory->GetDiscoveredGatheringItems().Contains(TEXT("Forage.WildStrawberry")));
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(Slot + TEXT("Inventory"), 0);
	return true;
}
#endif
