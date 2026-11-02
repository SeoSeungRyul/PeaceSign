#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSInventorySaveGame.h"
#include "../Skills/PSPlayerSkillComponent.h"
#include "../Fishing/PSFishingJournalComponent.h"
#include "../World/PSGridWorld.h"
#include "../World/PSFarmTargetMarker.h"
#include "../World/PSCropGrowth.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSChargedHoeTest, "PeaceSign.Farming.ChargedHoe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSChargedHoeTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	const FString Slot = TEXT("ChargedHoe_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSGridWorld* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot;
	Grid->FinishSpawning(FTransform::Identity);
	APSPlayerController* Controller = World->SpawnActorDeferred<APSPlayerController>(APSPlayerController::StaticClass(), FTransform::Identity);
	UPSInventoryComponent* Inventory = Controller->GetInventoryComponent();
	Inventory->SaveSlotName = Slot + TEXT("Inventory");
	Inventory->bAutoSave = false;
	Controller->SkillComponent->SaveSlotName = Slot + TEXT("Skill");
	Controller->SkillComponent->bAutoSave = false;
	Controller->FishingJournalComponent->SaveSlotName = Slot + TEXT("Journal");
	Controller->FishingJournalComponent->bAutoSave = false;
	Controller->GridWorld = Grid;
	Controller->FinishSpawning(FTransform::Identity);
	Inventory->ResetToDefaults();
	APSPlayerCharacter* Farmer = World->SpawnActor<APSPlayerCharacter>();
	Controller->Possess(Farmer);
	// This isolated world has no viewport/login flow to mark the controller local.
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
		for (const auto Cell : Controller->GetFarmTargetCells(true)) SetTile(Cell, EPSTileType::Grass);
	};
	Controller->SelectHotbarSlot(6);
	TestEqual(TEXT("Temporary upgraded hoe starts in slot seven"), Inventory->GetHotbarSlot(6).ItemId, PSItemIds::UpgradedHoe);
	TestTrue(TEXT("Upgraded hoe is chargeable"), Controller->CanChargeEquippedHoe());
	TestTrue(TEXT("Upgraded hoe has a distinct display name"), PSItems::GetDefinition(Inventory->GetHotbarSlot(6)).Name.ToString().Contains(TEXT("1차")));
	ClearTargets();
	Step(true, 0);
	TestTrue(TEXT("Enhanced Input Started begins charging"), Controller->IsFarmCharging());
	TestFalse(TEXT("Holding does not start use animation"), Controller->IsFarmActionActive());
	TestFalse(TEXT("Charging allows movement"), Farmer->IsMovementLocked());
	Step(true, 0.49f);
	TestFalse(TEXT("Below threshold is not a charged swing"), Controller->GetFarmChargeProgress() >= 1);
	Step(false, 0);
	TestTrue(TEXT("Enhanced Input Completed starts short swing"), Controller->IsFarmActionActive());
	TestEqual(TEXT("Short swing has one target"), Controller->FarmActionCells.Num(), 1);
	Step(false, 0.5f);
	TestEqual(TEXT("Short swing tills front cell"), Grid->GetGroundTile(FIntPoint(-1, 0)), EPSTileType::TilledSoil);
	TestEqual(TEXT("Short swing leaves side cell alone"), Grid->GetGroundTile(FIntPoint(-1, 1)), EPSTileType::Grass);
	ClearTargets();
	Step(true, 0);
	Step(true, 0.5f);
	TestTrue(TEXT("Still holding at threshold keeps charging"), Controller->IsFarmCharging());
	TestEqual(TEXT("Threshold reaches full charge"), Controller->GetFarmChargeProgress(), 1.0f);
	TestEqual(TEXT("Holding never changes terrain"), Grid->GetGroundTile(FIntPoint(-1, 0)), EPSTileType::Grass);
	Controller->UpdateFarmTargetMarker();
	TestTrue(TEXT("Charged preview has a center marker"), IsValid(Controller->FarmTargetMarker));
	TestEqual(TEXT("Charged preview has two side markers"), Controller->FarmSideMarkers.Num(), 2);
	for (APSFarmTargetMarker* Marker : Controller->FarmSideMarkers)
		TestTrue(TEXT("Side preview is visible while fully charged"), IsValid(Marker) && !Marker->IsHidden());
	Step(false, 0);
	TestTrue(TEXT("Release locks movement for use"), Farmer->IsMovementLocked());
	TestEqual(TEXT("Charged swing has three targets"), Controller->FarmActionCells.Num(), 3);
	Step(false, 0.49f);
	TestEqual(TEXT("Charge time does not replace use time"), Grid->GetGroundTile(FIntPoint(-1, 0)), EPSTileType::Grass);
	Step(false, 0.01f);
	for (const auto Cell : Controller->GetFarmTargetCells(true))
		TestEqual(TEXT("Full swing tills all three eligible cells"), Grid->GetGroundTile(Cell), EPSTileType::TilledSoil);
	TestEqual(TEXT("Three cells consume one durability per swing"), Inventory->GetHotbarSlot(6).CurrentDurability, 98);
	TestFalse(TEXT("Swing unlocks movement"), Farmer->IsMovementLocked());
	Controller->UpdateFarmTargetMarker();
	for (APSFarmTargetMarker* Marker : Controller->FarmSideMarkers)
		TestTrue(TEXT("Side preview hides after swing"), IsValid(Marker) && Marker->IsHidden());

	for (const FVector2D Direction : {FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1)})
	{
		Farmer->Move(FInputActionValue(Direction));
		const auto Cells = Controller->GetFarmTargetCells(true);
		const FIntPoint Facing = Farmer->GetFacingCellOffset();
		TestEqual(TEXT("Center is one cell forward"), Cells[0], Facing);
		for (int32 Index = 1; Index < 3; ++Index)
		{
			const FIntPoint Offset = Cells[Index] - Cells[0];
			TestEqual(TEXT("Side targets are perpendicular to facing"), Offset.X * Facing.X + Offset.Y * Facing.Y, 0);
			TestEqual(TEXT("Side targets are exactly one cell from center"), FMath::Abs(Offset.X) + FMath::Abs(Offset.Y), 1);
		}
		ClearTargets();
		TestTrue(TEXT("Charged action works in every direction"), Controller->BeginFarmAction(true));
		Controller->UpdateFarmAction(0.5f);
		for (const auto Cell : Cells) TestEqual(TEXT("Rotated range matches effect"), Grid->GetGroundTile(Cell), EPSTileType::TilledSoil);
	}
	Farmer->Move(FInputActionValue(FVector2D(0, -1)));
	ClearTargets();
	const auto Cells = Controller->GetFarmTargetCells(true);
	SetTile(Cells[0], EPSTileType::Grass, true);
	SetTile(Cells[1], EPSTileType::Water);
	TestEqual(TEXT("Blocked target has invalid charged preview"), Controller->EvaluateFarmActionCell(Cells[0], true), EPSTileInteractionResult::NoEffect);
	TestTrue(TEXT("Valid side cell starts action even when center is blocked"), Controller->BeginFarmAction(true));
	Controller->UpdateFarmAction(0.5f);
	TestEqual(TEXT("Blocked center is preserved"), Grid->GetStoneHealth(Cells[0]), 30);
	TestEqual(TEXT("Water side is preserved"), Grid->GetGroundTile(Cells[1]), EPSTileType::Water);
	TestEqual(TEXT("Eligible side is tilled"), Grid->GetGroundTile(Cells[2]), EPSTileType::TilledSoil);
	const int32 Durability = Inventory->GetHotbarSlot(6).CurrentDurability;
	TestFalse(TEXT("All-invalid range starts no action"), Controller->BeginFarmAction(true));
	TestEqual(TEXT("All-invalid range consumes no durability"), Inventory->GetHotbarSlot(6).CurrentDurability, Durability);
	ClearTargets();
	Controller->BeginFarmAction(true);
	for (const auto Cell : Cells) SetTile(Cell, EPSTileType::Water);
	Controller->UpdateFarmAction(0.5f);
	TestEqual(TEXT("Effect-time revalidation consumes no durability on total failure"), Inventory->GetHotbarSlot(6).CurrentDurability, Durability);
	ClearTargets();
	SetTile(Cells[1], EPSTileType::TilledSoil);
	Grid->PlantSeed(Cells[1]);
	FPSChunkData& Chunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(Cells[1], Grid->ChunkSize));
	Chunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cells[1], Grid->ChunkSize), Grid->ChunkSize)].GrowthStage = PSCropGrowth::MaxStage;
	Controller->BeginFarmAction(true);
	Controller->UpdateFarmAction(0.5f);
	TestEqual(TEXT("Charged tilling never harvests mature crop"), Grid->GetCropStage(Cells[1]), PSCropGrowth::MaxStage);

	ClearTargets();
	Controller->HandleSpecialAttack();
	Controller->UpdateFarmAction(0.5f);
	Controller->HandleSpecialAttackCanceled();
	Controller->HandleSpecialAttackReleased();
	TestFalse(TEXT("Canceled input never starts an effect"), Controller->IsFarmActionActive());
	Controller->HandleSpecialAttack();
	Controller->UpdateFarmAction(0.5f);
	Controller->FlushPressedKeys();
	Controller->HandleSpecialAttackReleased();
	TestFalse(TEXT("Focus loss cannot synthesize a charged swing"), Controller->IsFarmActionActive() || Controller->IsFarmCharging());
	Controller->HandleSpecialAttack();
	Controller->SelectHotbarSlot(0);
	Controller->HandleSpecialAttackReleased();
	TestFalse(TEXT("Equipment change cancels charge and ignores stale release"), Controller->IsFarmActionActive() || Controller->IsFarmCharging());
	Controller->SelectHotbarSlot(6);
	Controller->HandleSpecialAttack();
	Inventory->MoveItem(EPSInventoryArea::Hotbar, 6, EPSInventoryArea::Bag, 0);
	TestFalse(TEXT("Inventory mutation cancels charging immediately"), Controller->IsFarmCharging());
	Inventory->MoveItem(EPSInventoryArea::Bag, 0, EPSInventoryArea::Hotbar, 6);
	Controller->HandleSpecialAttack();
	Farmer->SetMovementLocked(true);
	Controller->UpdateFarmAction(0.1f);
	TestFalse(TEXT("External movement lock cancels charge"), Controller->IsFarmCharging());
	TestTrue(TEXT("Charge cancellation preserves external movement lock"), Farmer->IsMovementLocked());
	Farmer->SetMovementLocked(false);
	Controller->HandleSpecialAttack();
	Farmer->Move(FInputActionValue(FVector2D(1, 0)));
	ClearTargets();
	Controller->UpdateFarmAction(0.5f);
	const auto TurnedCells = Controller->GetFarmTargetCells(true);
	Controller->HandleSpecialAttackReleased();
	Controller->UpdateFarmAction(0.5f);
	for (const auto Cell : TurnedCells) TestEqual(TEXT("Release snapshots the current facing"), Grid->GetGroundTile(Cell), EPSTileType::TilledSoil);
	ClearTargets();
	bool bDestroyed = false;
	Inventory->ConsumeHotbarDurability(6, Inventory->GetHotbarSlot(6).CurrentDurability - 1, bDestroyed);
	Controller->BeginFarmAction(true);
	Controller->UpdateFarmAction(0.5f);
	for (const auto Cell : TurnedCells) TestEqual(TEXT("Last durability completes entire range"), Grid->GetGroundTile(Cell), EPSTileType::TilledSoil);
	TestTrue(TEXT("Last swing removes broken upgraded hoe"), Inventory->GetHotbarSlot(6).IsEmpty());
	TestFalse(TEXT("Broken tool never leaves movement locked"), Farmer->IsMovementLocked());

	UPSInventorySaveGame* Legacy = NewObject<UPSInventorySaveGame>();
	Legacy->DataVersion = 6;
	for (int32 Index = 0; Index < 10; ++Index) Legacy->HotbarSlots.Add(Inventory->GetHotbarSlot(Index));
	for (int32 Index = 0; Index < 50; ++Index) Legacy->BagSlots.Add(Inventory->GetBagSlot(Index));
	Legacy->HotbarSlots[6].ItemType = EPSItemType::Wood;
	Legacy->HotbarSlots[6].Quantity = 1;
	Legacy->HotbarSlots[7].Clear(); // Version 6 predates both upgraded starter tools.
	const FString MigrationSlot = Slot + TEXT("Migration");
	TestTrue(TEXT("Old save fixture created"), UGameplayStatics::SaveGameToSlot(Legacy, MigrationSlot, 0));
	UPSInventoryComponent* Loaded = NewObject<UPSInventoryComponent>(World);
	Loaded->bAutoSave = false;
	Loaded->SaveSlotName = MigrationSlot;
	TestTrue(TEXT("Old save loads"), Loaded->LoadInventory());
	TestEqual(TEXT("Migration preserves occupied seventh slot"), Loaded->GetHotbarSlot(6).ItemType, EPSItemType::Wood);
	TestEqual(TEXT("Migration uses next empty hotbar slot"), Loaded->GetHotbarSlot(7).ItemId, PSItemIds::UpgradedHoe);
	Loaded->SaveInventory();
	Loaded->LoadInventory();
	TestEqual(TEXT("Reload does not duplicate either upgraded hoe"), Loaded->CountItem(EPSItemType::Hoe), 3);
	TestEqual(TEXT("Upgraded durability survives save"), Loaded->GetHotbarSlot(7).CurrentDurability, 100);
	for (auto& Item : Legacy->HotbarSlots) { Item.Clear(); Item.ItemType = EPSItemType::Wood; Item.Quantity = 1; }
	UGameplayStatics::SaveGameToSlot(Legacy, MigrationSlot, 0);
	Loaded->LoadInventory();
	TestEqual(TEXT("Full hotbar falls back to open bag slot"), Loaded->GetBagSlot(0).ItemId, PSItemIds::UpgradedHoe);
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Legacy->BagSlots[Index].Clear();
		Legacy->BagSlots[Index].ItemType = EPSItemType::Wood;
		Legacy->BagSlots[Index].Quantity = 1;
	}
	UGameplayStatics::SaveGameToSlot(Legacy, MigrationSlot, 0);
	Loaded->LoadInventory();
	TestEqual(TEXT("Full inventory preserves every existing wood item"), Loaded->CountItem(EPSItemType::Wood), 20);
	Loaded->SaveInventory();
	Loaded->LoadInventory();
	Loaded->RemoveFromHotbarSlot(9, 1);
	TestEqual(TEXT("Pending grant survives reload and fills newly freed slot"), Loaded->GetHotbarSlot(9).ItemId, PSItemIds::UpgradedHoe);
	Loaded->SaveInventory();
	Loaded->LoadInventory();
	TestEqual(TEXT("Pending grant is fulfilled only once"), Loaded->CountItem(EPSItemType::Hoe), 1);
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(MigrationSlot, 0);
	return true;
}
#endif
