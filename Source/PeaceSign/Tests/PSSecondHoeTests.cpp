#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSInventorySaveGame.h"
#include "../Skills/PSPlayerSkillComponent.h"
#include "../Fishing/PSFishingJournalComponent.h"
#include "../World/PSGridWorld.h"
#include "../World/PSFarmTargetMarker.h"
#include "../World/PSCropGrowth.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputActionValue.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSSecondHoeTest, "PeaceSign.Farming.SecondHoe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSSecondHoeTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	const FString Slot = TEXT("SecondHoe_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSGridWorld* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot;
	Grid->FinishSpawning(FTransform::Identity);
	APSPlayerController* Controller = World->SpawnActorDeferred<APSPlayerController>(APSPlayerController::StaticClass(), FTransform::Identity);
	UPSInventoryComponent* Inventory = Controller->GetInventoryComponent();
	Inventory->SaveSlotName = Slot + TEXT("Inventory");
	Inventory->bAutoSave = false;
	Controller->SkillComponent->SaveSlotName = Slot + TEXT("Skills");
	Controller->SkillComponent->bAutoSave = false;
	Controller->FishingJournalComponent->SaveSlotName = Slot + TEXT("Journal");
	Controller->FishingJournalComponent->bAutoSave = false;
	Controller->GridWorld = Grid;
	Controller->FinishSpawning(FTransform::Identity);
	Inventory->ResetToDefaults();
	APSPlayerCharacter* Farmer = World->SpawnActor<APSPlayerCharacter>();
	Controller->Possess(Farmer);
	Controller->SetAsLocalPlayerController();
	Farmer->SetActorLocation(Grid->CellToWorldCenter(FIntPoint::ZeroValue));
	Controller->InputComponent = NewObject<UEnhancedInputComponent>(Controller);
	Controller->SetupInputComponent();
	auto* Input = NewObject<UEnhancedPlayerInput>(Controller);
	Controller->PlayerInput = Input;
	const TArray<UInputComponent*> InputStack = {Controller->InputComponent};
	const auto Step = [&](const bool bDown, const float Seconds)
	{
		Controller->UpdateFarmAction(Seconds);
		Input->InjectInputForAction(Controller->SpecialAttackAction, FInputActionValue(bDown));
		Input->ProcessInputStack(InputStack, FMath::Max(Seconds, 0.001f), false);
	};
	const auto SetTile = [&](const FIntPoint Cell, const EPSTileType Ground, const bool bStone = false)
	{
		if (!Grid->IsCellInsideWorld(Cell)) return;
		FPSChunkData& Chunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(Cell, Grid->ChunkSize));
		FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, Grid->ChunkSize), Grid->ChunkSize)];
		Tile = FPSTileCell();
		Tile.GroundType = Ground;
		Tile.ObjectType = bStone ? EPSWorldObjectType::Stone : EPSWorldObjectType::None;
		Tile.ObjectHealth = bStone ? 30 : 0;
		FPSChunkSaveData& Saved = Grid->ModifiedChunks.FindOrAdd(Chunk.Coordinate);
		Saved.Coordinate = Chunk.Coordinate;
		Saved.Cells = Chunk.Cells;
		Saved.Veins = Chunk.Veins;
	};
	const auto ClearTargets = [&]()
	{
		for (const FIntPoint Cell : Controller->GetFarmTargetCells(true)) SetTile(Cell, EPSTileType::Grass);
	};
	Controller->SelectHotbarSlot(7);
	TestEqual(TEXT("Second hoe starts in hotbar eight"), Inventory->GetHotbarSlot(7).ItemId, PSItemIds::SecondUpgradedHoe);
	TestEqual(TEXT("Second hoe has three charged rows"), Controller->GetEquippedFarmChargeDepth(), 3);
	TestTrue(TEXT("Second hoe displays its own name"), PSItems::GetDefinition(Inventory->GetHotbarSlot(7)).Name.ToString().Contains(TEXT("2차")));
	ClearTargets();
	Step(true, 0);
	Step(true, 0.49f);
	Step(false, 0);
	TestEqual(TEXT("Short second-hoe input still targets one cell"), Controller->FarmActionCells.Num(), 1);
	Step(false, 0.7f);
	TestEqual(TEXT("Short input tills immediate front"), Grid->GetGroundTile(FIntPoint(-1, 0)), EPSTileType::TilledSoil);
	TestEqual(TEXT("Short input leaves far row untouched"), Grid->GetGroundTile(FIntPoint(-3, 0)), EPSTileType::Grass);
	ClearTargets();
	Step(true, 0);
	Step(true, 0.5f);
	TestTrue(TEXT("Full charge waits for physical release"), Controller->IsFarmCharging());
	TestEqual(TEXT("Charging alone applies no effect"), Grid->GetGroundTile(FIntPoint(-1, 0)), EPSTileType::Grass);
	Controller->UpdateFarmTargetMarker();
	TestEqual(TEXT("Nine-cell preview allocates eight extra markers"), Controller->FarmSideMarkers.Num(), 8);
	for (APSFarmTargetMarker* Marker : Controller->FarmSideMarkers)
		TestTrue(TEXT("All extra preview cells are visible"), IsValid(Marker) && !Marker->IsHidden());
	Step(false, 0);
	const auto FullRange = Controller->FarmActionCells;
	TestEqual(TEXT("Release snapshots nine cells"), FullRange.Num(), 9);
	TestTrue(TEXT("Use phase locks movement"), Farmer->IsMovementLocked());
	Step(false, 0.69f);
	TestEqual(TEXT("Use phase has its own 0.7-second delay"), Grid->GetGroundTile(FIntPoint(-1, 0)), EPSTileType::Grass);
	Step(false, 0.01f);
	for (const FIntPoint Cell : FullRange) TestEqual(TEXT("All nine cells are tilled"), Grid->GetGroundTile(Cell), EPSTileType::TilledSoil);
	TestEqual(TEXT("Nine cells cost one durability per swing"), Inventory->GetHotbarSlot(7).CurrentDurability, 98);
	TestFalse(TEXT("Use completion unlocks movement"), Farmer->IsMovementLocked());
	Controller->UpdateFarmTargetMarker();
	for (APSFarmTargetMarker* Marker : Controller->FarmSideMarkers)
		TestTrue(TEXT("Nine-cell preview extras hide on return to single cell"), IsValid(Marker) && Marker->IsHidden());

	for (const FVector2D Direction : {FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1)})
	{
		Farmer->Move(FInputActionValue(Direction));
		const FIntPoint Facing = Farmer->GetFacingCellOffset();
		const FIntPoint Side(-Facing.Y, Facing.X);
		const auto Cells = Controller->GetFarmTargetCells(true);
		TSet<FIntPoint> Unique;
		for (const FIntPoint Cell : Cells)
		{
			Unique.Add(Cell);
			const int32 Forward = Cell.X * Facing.X + Cell.Y * Facing.Y;
			const int32 Lateral = Cell.X * Side.X + Cell.Y * Side.Y;
			TestTrue(TEXT("Entire range lies in front, one to three rows away"), Forward >= 1 && Forward <= 3);
			TestTrue(TEXT("Entire range is at most one cell sideways"), FMath::Abs(Lateral) <= 1);
		}
		TestEqual(TEXT("Each rotated footprint has nine unique cells"), Unique.Num(), 9);
		TestFalse(TEXT("Footprint never includes the standing cell"), Unique.Contains(FIntPoint::ZeroValue));
		ClearTargets();
		TestTrue(TEXT("Second hoe can start in each cardinal direction"), Controller->BeginFarmAction(true));
		Controller->UpdateFarmAction(0.7f);
		for (const FIntPoint Cell : Cells) TestEqual(TEXT("Effect matches rotated footprint"), Grid->GetGroundTile(Cell), EPSTileType::TilledSoil);
	}
	Farmer->Move(FInputActionValue(FVector2D(0, -1)));
	ClearTargets();
	const auto Cells = Controller->GetFarmTargetCells(true);
	SetTile(Cells[0], EPSTileType::Grass, true);
	SetTile(Cells[1], EPSTileType::Water);
	SetTile(Cells[2], EPSTileType::TilledSoil);
	Grid->PlantSeed(Cells[2]);
	FPSChunkData& CropChunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(Cells[2], Grid->ChunkSize));
	CropChunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cells[2], Grid->ChunkSize), Grid->ChunkSize)].GrowthStage = PSCropGrowth::MaxStage;
	TestTrue(TEXT("Partial range starts with blocked front cell"), Controller->BeginFarmAction(true));
	// Invalidate another cell after release; the other five must still succeed.
	SetTile(Cells[3], EPSTileType::Grass, true);
	Controller->UpdateFarmAction(0.7f);
	TestEqual(TEXT("Front stone remains intact"), Grid->GetStoneHealth(Cells[0]), 30);
	TestEqual(TEXT("Water remains intact"), Grid->GetGroundTile(Cells[1]), EPSTileType::Water);
	TestEqual(TEXT("Mature crop is protected from charged use"), Grid->GetCropStage(Cells[2]), PSCropGrowth::MaxStage);
	TestEqual(TEXT("New obstacle is revalidated at effect time"), Grid->GetStoneHealth(Cells[3]), 30);
	for (int32 Index = 4; Index < Cells.Num(); ++Index) TestEqual(TEXT("Other cells still succeed"), Grid->GetGroundTile(Cells[Index]), EPSTileType::TilledSoil);
	const int32 Durability = Inventory->GetHotbarSlot(7).CurrentDurability;
	TestFalse(TEXT("Fully invalid range starts no action"), Controller->BeginFarmAction(true));
	TestEqual(TEXT("Rejected range costs no durability"), Inventory->GetHotbarSlot(7).CurrentDurability, Durability);
	ClearTargets();
	Controller->BeginFarmAction(true);
	for (const FIntPoint Cell : Cells) SetTile(Cell, EPSTileType::Water);
	Controller->UpdateFarmAction(0.7f);
	TestEqual(TEXT("Total effect-time failure costs no durability"), Inventory->GetHotbarSlot(7).CurrentDurability, Durability);
	ClearTargets();
	Controller->HandleSpecialAttack();
	Controller->UpdateFarmAction(0.5f);
	Controller->UpdateFarmTargetMarker();
	Controller->SelectHotbarSlot(6);
	Controller->HandleSpecialAttackReleased();
	TestFalse(TEXT("Switching to first hoe cancels second hoe charge"), Controller->IsFarmActionActive() || Controller->IsFarmCharging());
	Controller->HandleSpecialAttack();
	Controller->UpdateFarmAction(0.5f);
	Controller->UpdateFarmTargetMarker();
	int32 Visible = IsValid(Controller->FarmTargetMarker) && !Controller->FarmTargetMarker->IsHidden() ? 1 : 0;
	for (APSFarmTargetMarker* Marker : Controller->FarmSideMarkers) Visible += IsValid(Marker) && !Marker->IsHidden();
	TestEqual(TEXT("Switching nine-cell pool to first hoe shows only three cells"), Visible, 3);
	Controller->HandleSpecialAttackCanceled();
	Controller->SelectHotbarSlot(7);
	// A clipped footprint at the world edge still processes its valid first row.
	Farmer->Move(FInputActionValue(FVector2D(0, 1)));
	Farmer->SetActorLocation(Grid->CellToWorldCenter(FIntPoint(Grid->WorldHalfExtentInCells - 2, 0)));
	ClearTargets();
	const auto EdgeRange = Controller->GetFarmTargetCells(true);
	Controller->BeginFarmAction(true);
	Controller->UpdateFarmAction(0.7f);
	int32 Inside = 0;
	for (const FIntPoint Cell : EdgeRange)
	{
		if (Grid->IsCellInsideWorld(Cell)) { ++Inside; TestEqual(TEXT("Valid edge cell is tilled"), Grid->GetGroundTile(Cell), EPSTileType::TilledSoil); }
		else TestEqual(TEXT("Outside world remains empty"), Grid->GetGroundTile(Cell), EPSTileType::Empty);
	}
	TestEqual(TEXT("Only immediate row lies inside the edge"), Inside, 3);
	Farmer->SetActorLocation(Grid->CellToWorldCenter(FIntPoint::ZeroValue));
	ClearTargets();
	bool bDestroyed = false;
	Inventory->ConsumeHotbarDurability(7, Inventory->GetHotbarSlot(7).CurrentDurability - 1, bDestroyed);
	const auto LastRange = Controller->GetFarmTargetCells(true);
	Controller->BeginFarmAction(true);
	Controller->UpdateFarmAction(0.7f);
	TestEqual(TEXT("Last swing still includes nine cells"), LastRange.Num(), 9);
	for (const FIntPoint Cell : LastRange) TestEqual(TEXT("Last durability applies entire range"), Grid->GetGroundTile(Cell), EPSTileType::TilledSoil);
	TestTrue(TEXT("Second hoe breaks after its last full swing"), Inventory->GetHotbarSlot(7).IsEmpty());
	TestFalse(TEXT("Broken second hoe releases movement"), Farmer->IsMovementLocked());

	UPSInventorySaveGame* Legacy = NewObject<UPSInventorySaveGame>();
	Legacy->DataVersion = 7;
	for (int32 Index = 0; Index < 10; ++Index) Legacy->HotbarSlots.Add(Inventory->GetHotbarSlot(Index));
	// Version 7 predates the starter axe; preserve the legacy empty ninth slot.
	Legacy->HotbarSlots[8].Clear();
	for (int32 Index = 0; Index < 50; ++Index) Legacy->BagSlots.Add(Inventory->GetBagSlot(Index));
	Legacy->HotbarSlots[6].CurrentDurability = 42;
	Legacy->HotbarSlots[7].ItemType = EPSItemType::Wood;
	Legacy->HotbarSlots[7].Quantity = 1;
	const FString MigrationSlot = Slot + TEXT("Migration");
	UGameplayStatics::SaveGameToSlot(Legacy, MigrationSlot, 0);
	UPSInventoryComponent* Loaded = NewObject<UPSInventoryComponent>(World);
	Loaded->SaveSlotName = MigrationSlot;
	Loaded->bAutoSave = false;
	Loaded->LoadInventory();
	TestEqual(TEXT("Migration preserves occupied eighth slot"), Loaded->GetHotbarSlot(7).ItemType, EPSItemType::Wood);
	TestEqual(TEXT("Migration preserves first hoe durability"), Loaded->GetHotbarSlot(6).CurrentDurability, 42);
	TestEqual(TEXT("Migration uses next empty slot for second hoe"), Loaded->GetHotbarSlot(8).ItemId, PSItemIds::SecondUpgradedHoe);
	Loaded->SaveInventory();
	Loaded->LoadInventory();
	TestEqual(TEXT("Reload never duplicates second hoe"), Loaded->CountItem(EPSItemType::Hoe), 3);
	Loaded->ConsumeHotbarDurability(8, 100, bDestroyed);
	Loaded->SaveInventory();
	Loaded->LoadInventory();
	TestTrue(TEXT("Broken second hoe is not re-granted on reload"), Loaded->GetHotbarSlot(8).IsEmpty());
	for (auto& Item : Legacy->HotbarSlots) { Item.Clear(); Item.ItemType = EPSItemType::Wood; Item.Quantity = 1; }
	for (int32 Index = 0; Index < 10; ++Index) { Legacy->BagSlots[Index].Clear(); Legacy->BagSlots[Index].ItemType = EPSItemType::Wood; Legacy->BagSlots[Index].Quantity = 1; }
	UGameplayStatics::SaveGameToSlot(Legacy, MigrationSlot, 0);
	Loaded->LoadInventory();
	Loaded->SaveInventory();
	Loaded->LoadInventory();
	TestEqual(TEXT("Full old inventory keeps all twenty items"), Loaded->CountItem(EPSItemType::Wood), 20);
	Loaded->RemoveFromHotbarSlot(9, 1);
	TestEqual(TEXT("Second hoe pending grant survives full inventory and reload"), Loaded->GetHotbarSlot(9).ItemId, PSItemIds::SecondUpgradedHoe);
	Loaded->SaveInventory();
	Loaded->LoadInventory();
	TestEqual(TEXT("Pending second hoe is granted once"), Loaded->CountItem(EPSItemType::Hoe), 1);
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(MigrationSlot, 0);
	return true;
}
#endif
