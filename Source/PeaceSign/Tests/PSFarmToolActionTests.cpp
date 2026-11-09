#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Skills/PSPlayerSkillComponent.h"
#include "../Fishing/PSFishingJournalComponent.h"
#include "../World/PSGridWorld.h"
#include "../World/PSFarmTargetMarker.h"
#include "../World/PSCropGrowth.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSFarmToolActionTest, "PeaceSign.Farming.FrontCellToolAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSFarmToolActionTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	const FString Slot = TEXT("FarmToolAction_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSGridWorld* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot;
	Grid->FinishSpawning(FTransform::Identity);
	APSPlayerController* Controller = World->SpawnActorDeferred<APSPlayerController>(APSPlayerController::StaticClass(), FTransform::Identity);
	Controller->InventoryComponent->SaveSlotName = Slot + TEXT("Inventory");
	Controller->InventoryComponent->bAutoSave = false;
	Controller->SkillComponent->SaveSlotName = Slot + TEXT("Skills");
	Controller->SkillComponent->bAutoSave = false;
	Controller->FishingJournalComponent->SaveSlotName = Slot + TEXT("Fishing");
	Controller->FishingJournalComponent->bAutoSave = false;
	Controller->GridWorld = Grid;
	Controller->FinishSpawning(FTransform::Identity);
	Controller->InventoryComponent->ResetToDefaults();
	APSPlayerCharacter* Farmer = World->SpawnActor<APSPlayerCharacter>();
	Controller->Possess(Farmer);
	Farmer->SetActorLocation(Grid->CellToWorldCenter(FIntPoint::ZeroValue) + FVector(0, 0, 100));
	const FIntPoint Target(-1, 0);
	const auto SetTile = [&](const FIntPoint Cell, const EPSTileType Ground)
	{
		FPSChunkData& Chunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(Cell, Grid->ChunkSize));
		FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, Grid->ChunkSize), Grid->ChunkSize)];
		Tile = FPSTileCell();
		Tile.GroundType = Ground;
		FPSChunkSaveData& Saved = Grid->ModifiedChunks.FindOrAdd(Chunk.Coordinate);
		Saved.Coordinate = Chunk.Coordinate;
		Saved.Cells = Chunk.Cells;
		Saved.Veins = Chunk.Veins;
	};
	const auto ResetTarget = [&](const EPSTileType Ground = EPSTileType::Grass) { SetTile(Target, Ground); };
	const auto& Inventory = Controller->InventoryComponent;
	TestEqual(TEXT("Default facing is screen down / world negative X"), Farmer->GetFacingCellOffset(), Target);
	Farmer->Move(FInputActionValue(FVector2D(1, 0)));
	TestEqual(TEXT("Screen right maps to world positive Y"), Farmer->GetFacingCellOffset(), FIntPoint(0, 1));
	Farmer->Move(FInputActionValue(FVector2D(-1, 0)));
	TestEqual(TEXT("Screen left maps to world negative Y"), Farmer->GetFacingCellOffset(), FIntPoint(0, -1));
	Farmer->Move(FInputActionValue(FVector2D(1, 1)));
	TestEqual(TEXT("Diagonal ties match sprite vertical facing"), Farmer->GetFacingCellOffset(), FIntPoint(1, 0));
	Farmer->Move(FInputActionValue(FVector2D(0, -1)));
	ResetTarget();
	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("Preview permits tilling the front cell"), Controller->EvaluateFarmTarget(Target), EPSTileInteractionResult::Tilled);
	Controller->HandleSpecialAttack();
	TestTrue(TEXT("Right-click starts action without a cursor hit"), Controller->IsFarmActionActive());
	TestEqual(TEXT("Effect is delayed"), Grid->GetGroundTile(Target), EPSTileType::Grass);
	TestTrue(TEXT("Movement locks during action"), Farmer->IsMovementLocked());
	Farmer->Move(FInputActionValue(FVector2D(1, 0)));
	Farmer->StartRun();
	Farmer->Roll();
	TestTrue(TEXT("Held movement cannot turn during action"), Farmer->CurrentMovementInput.IsNearlyZero());
	TestFalse(TEXT("Cannot run during action"), Farmer->bWantsToRun);
	TestFalse(TEXT("Cannot roll during action"), Farmer->IsRolling());
	TestEqual(TEXT("Locked input keeps original front cell"), Controller->GetFarmFrontCell().GetValue(), Target);
	Controller->SelectHotbarSlot(1);
	Controller->HandleInventory();
	TestEqual(TEXT("Hotbar selection is blocked"), Controller->SelectedHotbarSlot, 0);
	TestFalse(TEXT("Inventory cannot open mid-action"), Controller->bInventoryOpen);
	Controller->UpdateFarmAction(0.25f);
	Controller->HandleSpecialAttack();
	TestTrue(TEXT("Repeated click cannot restart the timer"), FMath::IsNearlyEqual(Controller->GetFarmActionTimeRemaining(), 0.45f));
	Controller->UpdateFarmAction(0.44f);
	TestEqual(TEXT("Effect does not occur before 0.7 seconds"), Grid->GetGroundTile(Target), EPSTileType::Grass);
	TestEqual(TEXT("The use animation does not spend stamina early"), Farmer->StatsComponent->Stamina, 100.f);
	Controller->UpdateFarmAction(0.01f);
	TestEqual(TEXT("Effect tills exactly the front cell"), Grid->GetGroundTile(Target), EPSTileType::TilledSoil);
	TestEqual(TEXT("A successful action consumes one durability"), Inventory->GetHotbarSlot(0).CurrentDurability, 99);
	TestEqual(TEXT("A successful hoe action spends 10 SP"), Farmer->StatsComponent->Stamina, 90.f);
	TestFalse(TEXT("Action finishes"), Controller->IsFarmActionActive());
	TestFalse(TEXT("Movement unlocks"), Farmer->IsMovementLocked());
	Controller->UpdateFarmAction(1);
	TestEqual(TEXT("Finished action cannot repeat"), Inventory->GetHotbarSlot(0).CurrentDurability, 99);
	TestFalse(TEXT("Already tilled cell is unusable"), Controller->BeginFarmAction());

	ResetTarget();
	TestTrue(TEXT("Second action starts"), Controller->BeginFarmAction());
	Grid->SetGroundTile(Target, EPSTileType::Water);
	Controller->UpdateFarmAction(0.7f);
	TestEqual(TEXT("Effect-time validation protects changed terrain"), Grid->GetGroundTile(Target), EPSTileType::Water);
	TestEqual(TEXT("Invalidated action costs no durability"), Inventory->GetHotbarSlot(0).CurrentDurability, 99);
	TestEqual(TEXT("Invalidated action costs no stamina"), Farmer->StatsComponent->Stamina, 90.f);
	TestFalse(TEXT("Rejected effect still unlocks movement"), Farmer->IsMovementLocked());
	ResetTarget();
	Controller->BeginFarmAction();
	Inventory->MoveItem(EPSInventoryArea::Hotbar, 0, EPSInventoryArea::Bag, 0);
	TestFalse(TEXT("External equipment removal cancels pending effect"), Controller->IsFarmActionActive());
	TestFalse(TEXT("External removal releases movement lock"), Farmer->IsMovementLocked());
	Controller->UpdateFarmAction(1);
	TestEqual(TEXT("Cancelled action leaves ground untouched"), Grid->GetGroundTile(Target), EPSTileType::Grass);
	Inventory->MoveItem(EPSInventoryArea::Bag, 0, EPSInventoryArea::Hotbar, 0);
	Controller->BeginFarmAction();
	Controller->UnPossess();
	TestFalse(TEXT("Unpossess cancels pending action"), Controller->IsFarmActionActive());
	TestFalse(TEXT("Unpossess releases old pawn's lock"), Farmer->IsMovementLocked());
	Controller->Possess(Farmer);
	Controller->BeginFarmAction();
	Farmer->StatsComponent->Health = 0;
	Controller->UpdateFarmAction(0.7f);
	TestEqual(TEXT("Dead pawn cannot finish action"), Grid->GetGroundTile(Target), EPSTileType::Grass);
	TestFalse(TEXT("Death releases action lock"), Farmer->IsMovementLocked());
	Farmer->StatsComponent->Health = 100;
	Controller->BeginFarmAction();
	Farmer->SetActorLocation(Grid->CellToWorldCenter(FIntPoint(2, 0)));
	Controller->UpdateFarmAction(0.7f);
	TestFalse(TEXT("Forced relocation cancels the latched target"), Controller->IsFarmActionActive());
	TestEqual(TEXT("Relocation never applies at stale target"), Grid->GetGroundTile(Target), EPSTileType::Grass);
	Farmer->SetActorLocation(Grid->CellToWorldCenter(FIntPoint::ZeroValue));

	ResetTarget(EPSTileType::TilledSoil);
	Controller->SelectHotbarSlot(5);
	TestEqual(TEXT("Dry soil gives usable watering preview"), Controller->EvaluateFarmTarget(Target), EPSTileInteractionResult::Watered);
	Farmer->StatsComponent->Stamina = 9.f;
	Controller->HandleSpecialAttack();
	TestFalse(TEXT("Below 10 SP, watering cannot start"), Controller->IsFarmActionActive());
	TestEqual(TEXT("Rejected watering keeps full durability"), Inventory->GetHotbarSlot(5).CurrentDurability, 100);
	Farmer->StatsComponent->Stamina = 10.f;
	Controller->HandleSpecialAttack();
	TestFalse(TEXT("Watering effect is delayed"), Grid->IsCellWatered(Target));
	Controller->UpdateFarmAction(0.7f);
	TestTrue(TEXT("Watering succeeds at 0.7 seconds"), Grid->IsCellWatered(Target));
	TestEqual(TEXT("Exactly 10 SP suffices"), Farmer->StatsComponent->Stamina, 0.f);
	TestEqual(TEXT("Basic watering uses one durability"), Inventory->GetHotbarSlot(5).CurrentDurability, 99);
	Farmer->StatsComponent->Stamina = 100.f;
	TestFalse(TEXT("Already wet soil cannot start watering"), Controller->BeginFarmAction());
	ResetTarget(EPSTileType::TilledSoil);
	Controller->BeginFarmAction();
	Grid->WaterCell(Target);
	Controller->UpdateFarmAction(0.7f);
	TestFalse(TEXT("Already watered during delay finishes safely"), Controller->IsFarmActionActive());
	TestFalse(TEXT("Failed repeat watering unlocks movement"), Farmer->IsMovementLocked());
	TestEqual(TEXT("Failed repeat watering preserves stamina"), Farmer->StatsComponent->Stamina, 100.f);
	TestEqual(TEXT("Failed repeat watering preserves durability"), Inventory->GetHotbarSlot(5).CurrentDurability, 99);
	ResetTarget(EPSTileType::TilledSoil);
	Controller->BeginFarmAction();
	Farmer->StatsComponent->Stamina = 9.f;
	Controller->UpdateFarmAction(0.7f);
	TestFalse(TEXT("A loss of stamina during use prevents the effect"), Grid->IsCellWatered(Target));
	TestEqual(TEXT("No durability is spent if stamina drops before the effect"), Inventory->GetHotbarSlot(5).CurrentDurability, 99);
	Farmer->StatsComponent->Stamina = 100.f;

	ResetTarget(EPSTileType::TilledSoil);
	Grid->PlantSeed(Target);
	Controller->SelectHotbarSlot(0);
	TestFalse(TEXT("Immature crop prevents hoe action"), Controller->BeginFarmAction());
	FPSChunkData& CropChunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(Target, Grid->ChunkSize));
	CropChunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Target, Grid->ChunkSize), Grid->ChunkSize)].GrowthStage = PSCropGrowth::MaxStage;
	TestEqual(TEXT("Mature crops also block the hoe"), Controller->EvaluateFarmTarget(Target), EPSTileInteractionResult::NoEffect);
	TestFalse(TEXT("Mature crop does not start a hoe action"), Controller->BeginFarmAction());
	TestEqual(TEXT("Hoe does not award produce"), Inventory->CountItem(EPSItemType::TestCrop, 0), 0);
	TestEqual(TEXT("Hoe leaves the ripe crop intact"), Grid->GetCropStage(Target), PSCropGrowth::MaxStage);
	ResetTarget();
	FPSChunkData& BlockedChunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(Target, Grid->ChunkSize));
	FPSTileCell& Blocked = BlockedChunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Target, Grid->ChunkSize), Grid->ChunkSize)];
	Blocked.ObjectType = EPSWorldObjectType::Stone;
	Blocked.ObjectHealth = 30;
	TestFalse(TEXT("Stone gives unusable preview"), Controller->BeginFarmAction());
	TestEqual(TEXT("Hoe cannot enter the generic stone mining path"), Controller->UseEquippedItemOnCell(Target), EPSTileInteractionResult::NoEffect);
	TestEqual(TEXT("Hoe leaves stone health intact"), Grid->GetStoneHealth(Target), 30);
	ResetTarget();
	bool bDestroyed = false;
	Inventory->ConsumeHotbarDurability(0, Inventory->GetHotbarSlot(0).CurrentDurability - 1, bDestroyed);
	TestTrue(TEXT("One-durability hoe can start action"), Controller->BeginFarmAction());
	Controller->UpdateFarmAction(0.7f);
	TestEqual(TEXT("Last durability still applies effect"), Grid->GetGroundTile(Target), EPSTileType::TilledSoil);
	TestTrue(TEXT("Broken hoe is removed from selected slot"), Inventory->GetHotbarSlot(0).IsEmpty());
	TestEqual(TEXT("Breaking hoe refreshes equipment"), Controller->GetEquipment(), EPSEquipment::BareHands);
	TestFalse(TEXT("Breaking tool releases action lock"), Farmer->IsMovementLocked());

	const FIntPoint Side = Target + FIntPoint(0, 1);
	const FIntPoint Far = Target * 3;
	for (const FName ItemId : {PSItemIds::UpgradedWateringCan, PSItemIds::SecondUpgradedWateringCan})
	{
		const int32 BagIndex = ItemId == PSItemIds::UpgradedWateringCan ? 9 : 8;
		TestEqual(TEXT("New watering can is in the bag"), Inventory->GetBagSlot(BagIndex).ItemId, ItemId);
		Inventory->MoveItem(EPSInventoryArea::Bag, BagIndex, EPSInventoryArea::Hotbar, 0);
		Controller->SelectHotbarSlot(0);
		const bool bLarge = ItemId == PSItemIds::SecondUpgradedWateringCan;
		const auto Range = Controller->GetFarmTargetCells(true);
		TestEqual(TEXT("Charged watering has the matching footprint"), Range.Num(), bLarge ? 9 : 3);
		for (const auto Cell : Range) SetTile(Cell, EPSTileType::TilledSoil);
		SetTile(Far, EPSTileType::TilledSoil);
		Controller->HandleSpecialAttack();
		Controller->UpdateFarmAction(0.49f);
		Controller->HandleSpecialAttackReleased();
		Controller->UpdateFarmAction(0.7f);
		TestTrue(TEXT("Short click waters the front only"), Grid->IsCellWatered(Target));
		TestFalse(TEXT("Short click leaves side soil dry"), Grid->IsCellWatered(Side));

		const FIntPoint BlockedSide = Target - FIntPoint(0, 1);
		SetTile(BlockedSide, EPSTileType::Grass);
		TestEqual(TEXT("A separate side tile contains a growing crop"), Grid->PlantSeed(Side), EPSTileInteractionResult::Planted);
		Inventory->ConsumeHotbarDurability(0, Inventory->GetHotbarSlot(0).CurrentDurability - 1, bDestroyed);
		const float Before = Farmer->StatsComponent->Stamina;
		Controller->HandleSpecialAttack();
		Controller->UpdateFarmAction(0.5f);
		TestFalse(TEXT("Holding alone leaves crops dry"), Grid->IsCellWatered(Side));
		Controller->HandleSpecialAttackReleased();
		Controller->UpdateFarmAction(0.69f);
		TestFalse(TEXT("Charged watering waits for the use duration"), Grid->IsCellWatered(Side));
		Controller->UpdateFarmAction(0.01f);
		for (const auto Cell : Range)
			TestEqual(TEXT("The final use waters every eligible tile and skips grass"), Grid->IsCellWatered(Cell), Cell != BlockedSide);
		TestEqual(TEXT("Watering reaches the far row only with the second upgrade"), Grid->IsCellWatered(Far), bLarge);
		TestEqual(TEXT("Watering preserves the growing crop"), Grid->GetCropType(Side), EPSCropType::TestCrop);
		TestEqual(TEXT("Any number of watered cells costs 10 SP"), Farmer->StatsComponent->Stamina, Before - 10.f);
		TestTrue(TEXT("Final use breaks the watering can after watering the range"), Inventory->GetHotbarSlot(0).IsEmpty());
	}

	APSFarmTargetMarker* Marker = World->SpawnActor<APSFarmTargetMarker>();
	Marker->ShowTarget(FVector::ZeroVector, 100, true);
	auto* Border = Marker->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
	TestEqual(TEXT("Preview draws four border strips"), Border->GetInstanceCount(), 4);
	TestEqual(TEXT("Preview cannot block movement"), Border->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Preview is above wet soil overlay"), Marker->GetActorLocation().Z, 4.0);
	auto* Material = Cast<UMaterialInstanceDynamic>(Border->GetMaterial(0));
	UTexture* Green = Material ? Material->K2_GetTextureParameterValue(TEXT("SpriteTexture")) : nullptr;
	TestNotNull(TEXT("Preview has a green texture"), Green);
	Marker->ShowTarget(FVector(100, 0, 0), 50, false);
	TestEqual(TEXT("Resizing never duplicates border strips"), Border->GetInstanceCount(), 4);
	TestTrue(TEXT("Invalid preview switches texture to red"), Material && Material->K2_GetTextureParameterValue(TEXT("SpriteTexture")) != Green);
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	return true;
}
#endif
