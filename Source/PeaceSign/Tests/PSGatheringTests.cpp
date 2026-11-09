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
#include "../Inventory/PSWorldItemActor.h"
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
		Resources->Trees = {Tree};
		Resources->Drops.Reset();
		const auto Axe = Resources->AxeDefinition(PSGathering::AxeId(Tier));
		TestEqual(TEXT("Axe SP cost follows accumulated upgrades"), Axe.StaminaCost, Tier >= 6 ? 5.0f : 10.0f);
		for (int32 Hit = 0; Hit < Hits[Tier - 1]; ++Hit)
		{
			TestTrue(TEXT("Valid axe strike succeeds"), Resources->Strike(Cell, Axe));
			if (Hit + 1 < Hits[Tier - 1]) TestEqual(TEXT("Partial hits do not generate rewards"), Resources->Drops.Num(), 0);
		}
		TestFalse(TEXT("The last hit removes the tree record"), Resources->HasTree(Cell));
		TestEqual(TEXT("Felling creates log and seed rewards once"), Resources->Drops.Num(), 2);
		TestFalse(TEXT("A felled tree cannot be struck again"), Resources->Strike(Cell, Axe));
		TestFalse(TEXT("A felled tree cannot be shaken"), Resources->Shake(Cell));
		TestEqual(TEXT("No duplicate felling rewards"), Resources->Drops.Num(), 2);
	}

	Resources->Trees = {Tree};
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
	TestTrue(TEXT("Fill all bag slots"), Inventory->AddItem(EPSItemType::Stone, 8 * 999));
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
	TestFalse(TEXT("Felled tree stays removed after reload"), Resources->HasTree(Cell));
	const auto* Saved = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	TestTrue(TEXT("Persistent drops recorded in world save"), Saved && Saved->GatheringDrops.Num() == RewardsBeforeFinalHit + 2);

	// The same path used by right-click must spend only a valid, selected tree seed.
	Controller->GridWorld = Grid;
	Inventory->ResetToDefaults();
	Pawn->SetActorLocation(Grid->CellToWorldCenter(FIntPoint(5, 6)) + FVector(0, 0, 90));
	const FIntPoint Grass(5, 5);
	const FIntPoint Water(5, 7);
	const FIntPoint Dirt(4, 6);
	const FIntPoint Rock(5, 8);
	Grid->SetGroundTile(Water, EPSTileType::Water);
	Grid->SetGroundTile(Dirt, EPSTileType::Dirt);
	Grid->SetGroundTile(Rock, EPSTileType::Stone);
	// Rebuild the active chunk so the water collision blocker is present.
	Grid->UpdateActiveChunks(PSGrid::CellToChunk(Water, Grid->ChunkSize));
	const auto MapleSeed = PSGathering::GetTree(EPSTreeSpecies::Maple).SeedId;
	TestTrue(TEXT("Add three maple seeds"), Inventory->AddItemVariant(EPSItemType::TreeSeed, MapleSeed, 3));
	TestTrue(TEXT("Equip seed stack from the bag"), Inventory->MoveItem(EPSInventoryArea::Bag, 0, EPSInventoryArea::Hotbar, 9));
	Controller->SelectHotbarSlot(9);
	TestEqual(TEXT("Tree seeds are usable hotbar items"), Controller->GetEquipment(), EPSEquipment::TreeSeed);
	for (const FIntPoint Invalid : {Dirt, Rock, FIntPoint(5, 6), FIntPoint(5, 9)})
	{
		TestEqual(TEXT("Occupied, unsupported, underfoot or distant tiles reject planting"),
			Controller->UseEquippedItemOnCell(Invalid), EPSTileInteractionResult::InvalidCell);
	}
	TestEqual(TEXT("Failed planting spends nothing"), Inventory->GetHotbarSlot(9).Quantity, 3);
	TestEqual(TEXT("Plant on grass"), Controller->UseEquippedItemOnCell(Grass), EPSTileInteractionResult::Planted);
	TestEqual(TEXT("Plant on water from shore"), Controller->UseEquippedItemOnCell(Water), EPSTileInteractionResult::Planted);
	TestEqual(TEXT("Successful planting spends exactly one seed each"), Inventory->GetHotbarSlot(9).Quantity, 1);
	TestEqual(TEXT("The water tile remains water"), Grid->GetGroundTile(Water), EPSTileType::Water);
	TestEqual(TEXT("A sapling occupies its tile"), Grid->GetWorldObjectType(Grass), EPSWorldObjectType::Tree);
	TestEqual(TEXT("The same tile cannot be planted twice"), Controller->UseEquippedItemOnCell(Grass), EPSTileInteractionResult::InvalidCell);
	TestEqual(TEXT("New saplings have 10 HP"), Resources->GetTree(Grass).Health, 10);
	TestFalse(TEXT("A sapling cannot be shaken"), Resources->Shake(Water));
	const int64 MaturesAt = Resources->GetTree(Water).MaturesAtHalfHour;
	TestEqual(TEXT("Growth takes two in-game days"), MaturesAt - Resources->Now(), PSGathering::SaplingGrowthTime);
	Clock->RestoreClock(MaturesAt - 1, 0);
	Grid->LoadWorld();
	Resources->InitializeResources();
	TestTrue(TEXT("The sapling survives reloading just before maturity"), Resources->GetTree(Water).IsSapling());
	TestEqual(TEXT("The growth deadline survives reloading"), Resources->GetTree(Water).MaturesAtHalfHour, MaturesAt);
	Clock->RestoreClock(MaturesAt, 0);
	TestFalse(TEXT("Grass sapling matures on time"), Resources->GetTree(Grass).IsSapling());
	TestFalse(TEXT("Water sapling matures on time"), Resources->GetTree(Water).IsSapling());
	TestEqual(TEXT("The mature tree matches its seed"), Resources->GetTree(Water).Species, EPSTreeSpecies::Maple);
	TestTrue(TEXT("The mature tree on water is reachable from shore"), Resources->CanReach(FIntPoint(5, 6), Water, Pawn));
	const int32 PreviousDropCount = Resources->GetDrops().Num();
	TestTrue(TEXT("Mature water tree can be felled"), Resources->Strike(Water, Resources->AxeDefinition(PSGathering::AxeId(9)), Pawn));
	TestEqual(TEXT("Felling the water tree creates both reward stacks"), Resources->GetDrops().Num(), PreviousDropCount + 2);
	for (int32 Index = PreviousDropCount; Index < Resources->GetDrops().Num(); ++Index)
		TestEqual(TEXT("Water tree drops land where the player worked"), Resources->GetDrops()[Index].Cell, FIntPoint(5, 6));
	TestEqual(TEXT("The original felling tile can be replanted"), Controller->UseEquippedItemOnCell(Cell), EPSTileInteractionResult::Planted);
	Inventory->ResetToDefaults();
	Controller->SelectHotbarSlot(8);
	Pawn->StatsComponent->Stamina = 100;
	const int32 DropsBeforeSapling = Resources->GetDrops().Num();
	Action->HandleTree(Grid, Cell);
	TestTrue(TEXT("Axe input starts a sapling cut"), Action->IsGatheringActive());
	Action->Advance(0.61f);
	TestFalse(TEXT("An axe removes the sapling"), Resources->HasTree(Cell));
	TestEqual(TEXT("Sapling removal gives no wood or seeds"), Resources->GetDrops().Num(), DropsBeforeSapling);
	TestEqual(TEXT("Sapling removal consumes axe durability"), Inventory->GetHotbarSlot(8).CurrentDurability, 99);

	// Tile entry must work across the whole cell, independently of visual offsets and pawn height.
	Inventory->ResetToDefaults();
	FPSGatheringDrop TileDrop;
	TileDrop.Id = FGuid::NewGuid();
	TileDrop.Cell = FIntPoint(-3, -2);
	TileDrop.Item.ItemType = EPSItemType::Wood;
	TileDrop.Item.ItemId = TEXT("030101");
	TileDrop.Item.Quantity = 3;
	Resources->Restore({}, {TileDrop}, true, true);
	const FVector DropCenter = Grid->CellToWorldCenter(TileDrop.Cell);
	const float CellSize = Grid->GetCellSize();
	const FTransform DropTransform(DropCenter + FVector(0.49f, 0.49f, 0.18f) * CellSize);
	auto* DropActor = World->SpawnActorDeferred<APSWorldItemActor>(APSWorldItemActor::StaticClass(), DropTransform);
	DropActor->InitializeGatheringItem(TileDrop.Item, Resources, TileDrop.Id, nullptr, CellSize, true);
	DropActor->FinishSpawning(DropTransform);
	Pawn->SetActorLocation(DropCenter + FVector(0.49f, 0.49f, 1) * CellSize);
	DropActor->Tick(0.1f);
	TestEqual(TEXT("Items stay on the ground while landing"), Resources->GetDrops().Num(), 1);
	Pawn->SetActorLocation(DropCenter + FVector(0.51f, 0.49f, 1) * CellSize);
	DropActor->Tick(1);
	TestEqual(TEXT("Adjacent tile never collects across the boundary"), Inventory->CountItem(EPSItemType::Wood), 0);
	TestEqual(TEXT("Adjacent tile leaves the saved drop intact"), Resources->GetDrops().Num(), 1);
	TestTrue(TEXT("Fill the bag before entering the drop tile"), Inventory->AddItem(EPSItemType::Stone, 8 * 999));
	Pawn->SetActorLocation(DropCenter + FVector(-0.49f, -0.49f, 1) * CellSize);
	DropActor->Tick(0.01f);
	TestEqual(TEXT("Full inventory keeps the drop after tile entry"), Resources->GetDrops().Num(), 1);
	TestTrue(TEXT("Make room while standing on the drop tile"), Inventory->RemoveItem(EPSItemType::Stone, 999));
	DropActor->Tick(0.01f);
	TestEqual(TEXT("Same tile collects even far from the item artwork"), Inventory->CountItem(EPSItemType::Wood), 3);
	TestTrue(TEXT("Pickup removes the saved drop"), Resources->GetDrops().IsEmpty());
	TestTrue(TEXT("Pickup removes the world item actor"), DropActor->IsActorBeingDestroyed() || !IsValid(DropActor));

	const FTransform GenericTransform(DropCenter + FVector(0.49f, 0.49f, 0.2f) * CellSize);
	auto* GenericDrop = World->SpawnActorDeferred<APSWorldItemActor>(APSWorldItemActor::StaticClass(), GenericTransform);
	GenericDrop->InitializeItem(TileDrop.Item, Grid);
	GenericDrop->FinishSpawning(GenericTransform);
	GenericDrop->Tick(0.01f);
	TestEqual(TEXT("Ordinary item drops use the same tile entry rule"), Inventory->CountItem(EPSItemType::Wood), 6);
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(Slot + TEXT("Inventory"), 0);
	return true;
}
#endif
