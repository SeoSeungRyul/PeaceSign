#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../Gathering/PSGatheringActionComponent.h"
#include "../Gathering/PSGatheringTreeActor.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSInventorySaveGame.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSGatheringTest, "PeaceSign.Gathering.ActionsRewardsAndSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSGatheringTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	const FString Slot = TEXT("GatheringTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot;
	Grid->FinishSpawning(FTransform::Identity);
	auto* Resources = Grid->Gathering.Get();
	Resources->InitializeResources();
	TestTrue(TEXT("Development area generates trees"), Resources->Trees.Num() > 0);
	const auto Generated = Resources->Trees;
	for (const auto& Tree : Generated)
	{
		TestTrue(TEXT("Tree has a land floor"), Grid->GetGroundTile(Tree.Cell) == EPSTileType::Grass || Grid->GetGroundTile(Tree.Cell) == EPSTileType::Dirt);
		TestEqual(TEXT("All generated trees have health 60"), Tree.Health, 60);
		TestEqual(TEXT("Tree occupancy blocks farming and shovel"), Grid->ShovelCell(Tree.Cell), EPSTileInteractionResult::NoEffect);
	}
	Resources->InitializeResources();
	TestEqual(TEXT("Generation is idempotent"), Resources->Trees.Num(), Generated.Num());
	Resources->TreeMaxHealth = 75;
	Resources->ResetResources();
	for (const auto& ConfiguredTree : Resources->GetRecords())
		TestEqual(TEXT("New trees use component HP without a species data table"), ConfiguredTree.Health, 75);
	Resources->TreeMaxHealth = 60;
	Resources->ResetResources();

	// Use a clear strip so target/range tests do not depend on procedural rocks or water.
	const FIntPoint Cell(6, 6);
	for (int32 Y = 3; Y <= 9; ++Y)
		for (int32 X = 3; X <= 9; ++X)
		{
			const FIntPoint C(X, Y);
			auto& Chunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(C, Grid->ChunkSize));
			auto& Tile = Chunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(C, Grid->ChunkSize), Grid->ChunkSize)];
			Tile.GroundType = EPSTileType::Grass;
			Tile.ObjectType = EPSWorldObjectType::None;
			auto& Save = Grid->ModifiedChunks.FindOrAdd(Chunk.Coordinate);
			Save.Coordinate = Chunk.Coordinate;
			Save.Cells = Chunk.Cells;
		}
	FPSTreeRecord Tree;
	Tree.Cell = Cell;
	Resources->Trees = {Tree};
	Resources->Drops.Reset();
	TestTrue(TEXT("Adjacent tree is reachable"), Resources->CanReach(Cell + FIntPoint(-1, 0), Cell));
	TestTrue(TEXT("Two-cell straight tree is reachable"), Resources->CanReach(Cell + FIntPoint(-2, 0), Cell));
	TestTrue(TEXT("Diagonal tree is reachable"), Resources->CanReach(Cell + FIntPoint(-1, -1), Cell));
	TestFalse(TEXT("Three-cell tree is unreachable"), Resources->CanReach(Cell + FIntPoint(-3, 0), Cell));
	Grid->SetGroundTile(Cell + FIntPoint(-1, 0), EPSTileType::Water);
	TestFalse(TEXT("Water blocks the intervening cell"), Resources->CanReach(Cell + FIntPoint(-2, 0), Cell));
	Grid->SetGroundTile(Cell + FIntPoint(-1, 0), EPSTileType::Grass);

	const int32 Hits[] = {6, 5, 4, 4, 3, 3, 2, 2, 1};
	for (int32 Tier = 1; Tier <= 9; ++Tier)
	{
		Resources->Trees[0] = Tree;
		Resources->Drops.Reset();
		const auto Axe = Resources->AxeDefinition(PSGathering::AxeId(Tier));
		TestEqual(TEXT("Axe SP cost follows accumulated upgrades"), Axe.StaminaCost, Tier >= 6 ? 5.0f : 10.0f);
		for (int32 Hit = 0; Hit < Hits[Tier - 1]; ++Hit)
		{
			TestTrue(TEXT("Valid axe strike succeeds"), Resources->Strike(Cell, Axe));
			if (Hit + 1 < Hits[Tier - 1]) TestEqual(TEXT("Partial hits do not generate rewards"), Resources->Drops.Num(), 0);
		}
		TestEqual(TEXT("Axe reaches stump at specified hit count"), Resources->GetTree(Cell).Health, 0);
		TestEqual(TEXT("Felling creates log and seed rewards once"), Resources->Drops.Num(), 2);
		TestFalse(TEXT("Stump cannot be struck again"), Resources->Strike(Cell, Axe));
		TestFalse(TEXT("Stump cannot be shaken"), Resources->Shake(Cell));
		TestEqual(TEXT("No duplicate rewards on stump"), Resources->Drops.Num(), 2);
	}

	Resources->Trees[0] = Tree;
	Resources->Drops.Reset();
	auto* Clock = World->GetSubsystem<UPSGameTimeSubsystem>();
	TestNotNull(TEXT("Game clock exists"), Clock);
	TestTrue(TEXT("First shake pays a seed"), Resources->Shake(Cell));
	TestEqual(TEXT("Shaking drops seeds only"), Resources->Drops[0].Item.ItemType, EPSItemType::TreeSeed);
	TestFalse(TEXT("Immediate repeated shake has no reward"), Resources->Shake(Cell));
	const int64 Deadline = Resources->GetTree(Cell).NextShakeHalfHour;
	Clock->RestoreClock(Deadline - 1, 0);
	TestFalse(TEXT("One half-hour before seven days is blocked"), Resources->Shake(Cell));
	Clock->RestoreClock(Deadline, 0);
	TestTrue(TEXT("Exactly seven days allows another seed"), Resources->Shake(Cell));
	const int64 Cooldown = Resources->GetTree(Cell).NextShakeHalfHour;
	Resources->Strike(Cell, Resources->AxeDefinition(PSGathering::AxeId(1)));
	Grid->SaveWorld();
	const int32 PendingCount = Resources->Drops.Num();
	Grid->LoadedChunks.Reset();
	Resources->TreeMaxHealth = 90;
	Grid->LoadWorld();
	TestEqual(TEXT("Partial damage survives save/load"), Resources->GetTree(Cell).Health, 50);
	Resources->TreeMaxHealth = 60;
	TestEqual(TEXT("Shake cooldown survives save/load"), Resources->GetTree(Cell).NextShakeHalfHour, Cooldown);
	TestEqual(TEXT("Uncollected drops survive save/load"), Resources->Drops.Num(), PendingCount);
	Resources->InitializeResources();
	TestEqual(TEXT("Restored tree positions are not regenerated"), Resources->Trees.Num(), 1);

	auto* Controller = World->SpawnActor<APSPlayerController>();
	auto* Inventory = Controller->GetInventoryComponent();
	Inventory->bAutoSave = false;
	Inventory->SaveSlotName = Slot + TEXT("Inventory");
	Inventory->ResetToDefaults();
	TestEqual(TEXT("Starter axe is hotbar nine"), Inventory->GetHotbarSlot(8).ItemType, EPSItemType::Axe);
	const FGuid SeedDrop = Resources->Drops[0].Id;
	TestTrue(TEXT("Fill all bag slots"), Inventory->AddItem(EPSItemType::Stone, 9990));
	TestFalse(TEXT("Full bag leaves persistent drop untouched"), Resources->CollectDrop(SeedDrop, Inventory));
	TestEqual(TEXT("Full bag preserves pending rewards"), Resources->Drops.Num(), PendingCount);
	Inventory->ResetToDefaults();
	TestTrue(TEXT("Free bag collects seed"), Resources->CollectDrop(SeedDrop, Inventory));
	TestFalse(TEXT("Collected reward ID cannot be reused"), Resources->CollectDrop(SeedDrop, Inventory));
	TestEqual(TEXT("Pickup clears one persistent reward"), Resources->Drops.Num(), PendingCount - 1);

	auto* Pawn = World->SpawnActor<APSPlayerCharacter>(Grid->CellToWorldCenter(Cell + FIntPoint(-1, 0)) + FVector(0, 0, 90), FRotator::ZeroRotator);
	Controller->Possess(Pawn);
	TestTrue(TEXT("Test controller possesses its pawn"), Controller->GetPawn() == Pawn);
	Controller->SelectHotbarSlot(8);
	auto* Action = Controller->GatheringAction.Get();
	Resources->Trees[0] = Tree;
	Resources->Drops.Reset();
	TestTrue(TEXT("Tree input is handled"), Action->HandleTree(Grid, Cell));
	TestTrue(TEXT("Action locks movement"), Pawn->IsMovementLocked());
	Action->Advance(0.29f);
	TestEqual(TEXT("No damage before impact"), Resources->GetTree(Cell).Health, 60);
	Action->Cancel();
	TestEqual(TEXT("Cancel before impact consumes no durability"), Inventory->GetHotbarSlot(8).CurrentDurability, 100);
	TestEqual(TEXT("Cancel before impact consumes no SP"), Pawn->StatsComponent->Stamina, 100.0f);
	TestFalse(TEXT("Cancel restores movement"), Pawn->IsMovementLocked());
	Action->HandleTree(Grid, Cell);
	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("Hotbar changes blocked during gathering"), Controller->GetSelectedHotbarSlot(), 8);
	Action->Advance(0.31f);
	TestEqual(TEXT("Damage applies at impact"), Resources->GetTree(Cell).Health, 50);
	TestEqual(TEXT("One hit costs SP ten"), Pawn->StatsComponent->Stamina, 90.0f);
	TestEqual(TEXT("One hit consumes durability once"), Inventory->GetHotbarSlot(8).CurrentDurability, 99);
	Action->Advance(0.31f);
	TestEqual(TEXT("Completion does not repeat impact"), Resources->GetTree(Cell).Health, 50);
	TestFalse(TEXT("Completed action releases movement"), Pawn->IsMovementLocked());
	for (const int32 OtherSlot : {0, 2, 4, 5, 9})
	{
		Controller->SelectHotbarSlot(OtherSlot);
		const auto Before = Inventory->GetHotbarSlot(OtherSlot);
		const int32 HealthBefore = Resources->GetTree(Cell).Health;
		const float SPBefore = Pawn->StatsComponent->Stamina;
		Action->HandleTree(Grid, Cell);
		TestTrue(TEXT("Non-axe equipment starts shaking"), Action->IsGatheringActive());
		Action->Advance(0.81f);
		TestEqual(TEXT("Shaking never damages the tree"), Resources->GetTree(Cell).Health, HealthBefore);
		TestEqual(TEXT("Shaking never consumes SP"), Pawn->StatsComponent->Stamina, SPBefore);
		TestEqual(TEXT("Shaking never consumes equipped durability"), Inventory->GetHotbarSlot(OtherSlot).CurrentDurability, Before.CurrentDurability);
	}
	Controller->SelectHotbarSlot(8);
	Resources->bDevelopmentUnlocked = false;
	Action->HandleTree(Grid, Cell);
	TestFalse(TEXT("Locked gathering prevents work"), Action->IsGatheringActive());
	Resources->SetUnlocked(true);
	Action->HandleTree(Grid, Cell);
	TestTrue(TEXT("Unlock permits work"), Action->IsGatheringActive());
	Controller->FlushPressedKeys();
	TestFalse(TEXT("Focus loss cancels pending hit"), Action->IsGatheringActive());
	Pawn->StatsComponent->Stamina = 5;
	Action->HandleTree(Grid, Cell);
	TestFalse(TEXT("Insufficient SP prevents action start"), Action->IsGatheringActive());
	Pawn->StatsComponent->Stamina = 100;
	bool bDestroyed = false;
	Inventory->ConsumeHotbarDurability(8, 98, bDestroyed);
	const int32 RewardsBeforeFinalHit = Resources->Drops.Num();
	Resources->Trees[0].Health = 10;
	Action->HandleTree(Grid, Cell);
	Action->Advance(0.31f);
	TestTrue(TEXT("Broken axe keeps movement locked through the remaining swing"), Pawn->IsMovementLocked());
	Action->Advance(0.31f);
	TestTrue(TEXT("Last durability breaks axe"), Inventory->GetHotbarSlot(8).IsEmpty());
	TestEqual(TEXT("Last-durability hit still fells tree"), Resources->GetTree(Cell).Health, 0);
	TestEqual(TEXT("Final hit adds exactly two stacks independently of prior shaking"), Resources->Drops.Num(), RewardsBeforeFinalHit + 2);
	TestFalse(TEXT("Last-durability hit releases movement"), Pawn->IsMovementLocked());

	// Existing saves with a full inventory preserve the starter grant until a slot opens.
	Inventory->ResetToDefaults();
	Inventory->SaveInventory();
	auto* Legacy = Cast<UPSInventorySaveGame>(UGameplayStatics::LoadGameFromSlot(Inventory->SaveSlotName, 0));
	Legacy->DataVersion = 8;
	for (auto& Item : Legacy->HotbarSlots) { Item.ItemType = EPSItemType::Stone; Item.ItemId = NAME_None; Item.Quantity = 999; Item.CurrentDurability = INDEX_NONE; }
	for (auto& Item : Legacy->BagSlots) { Item.ItemType = EPSItemType::Stone; Item.ItemId = NAME_None; Item.Quantity = 999; Item.CurrentDurability = INDEX_NONE; }
	UGameplayStatics::SaveGameToSlot(Legacy, Inventory->SaveSlotName, 0);
	Inventory->LoadInventory();
	TestEqual(TEXT("Legacy full hotbar is never overwritten"), Inventory->GetHotbarSlot(8).ItemType, EPSItemType::Stone);
	Inventory->RemoveFromHotbarSlot(8, 999);
	TestEqual(TEXT("Pending axe appears when space opens"), Inventory->GetHotbarSlot(8).ItemType, EPSItemType::Axe);

	Grid->SaveWorld();
	Grid->LoadWorld();
	TestEqual(TEXT("Felled stump remains felled after reload"), Resources->GetTree(Cell).Health, 0);
	const auto* Saved = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	TestTrue(TEXT("Persistent drops recorded in world save"), Saved && Saved->GatheringDrops.Num() == RewardsBeforeFinalHit + 2);
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(Slot + TEXT("Inventory"), 0);
	return true;
}
#endif
