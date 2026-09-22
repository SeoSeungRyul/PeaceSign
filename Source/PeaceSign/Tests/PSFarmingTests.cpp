#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"
#include "../World/PSTileChunkActor.h"
#include "../World/PSCropGrowth.h"
#include "../PSPlayerController.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Skills/PSPlayerSkillComponent.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSFarmingTest, "PeaceSign.Grid.Tilling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSFarmingTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	APSGridWorld* Grid = World->SpawnActor<APSGridWorld>();
	// Isolate test writes from the player's world save.
	const FString Slot = TEXT("TillingTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FindFProperty<FStrProperty>(APSGridWorld::StaticClass(), TEXT("SaveSlotName"))->SetPropertyValue_InContainer(Grid, Slot);
	TMap<EPSTileType, FIntPoint> Cells;
	TArray<FIntPoint> TilledCells;
	for (int32 Y = -10; Y < 10; ++Y)
		for (int32 X = -10; X < 10; ++X)
			Cells.FindOrAdd(Grid->GetGroundTile(FIntPoint(X, Y)), FIntPoint(X, Y));
	for (const EPSTileType Type : {EPSTileType::Grass, EPSTileType::Dirt})
	{
		const FIntPoint* Cell = Cells.Find(Type);
		TestNotNull(TEXT("Generated terrain contains tillable ground"), Cell);
		if (!Cell) continue;
		TestEqual(TEXT("Grass and dirt can be tilled"), Grid->TillCell(*Cell), EPSTileInteractionResult::Tilled);
		TestEqual(TEXT("Tilling creates distinct farmland"), Grid->GetGroundTile(*Cell), EPSTileType::TilledSoil);
		TilledCells.Add(*Cell);
		TestEqual(TEXT("Repeated tilling is a no-op"), Grid->TillCell(*Cell), EPSTileInteractionResult::NoEffect);
	}
	const FIntPoint* Stone = Cells.Find(EPSTileType::Stone);
	TestNotNull(TEXT("Generated terrain contains stone"), Stone);
	if (Stone)
	{
		TestEqual(TEXT("Hoe cannot mine stone"), Grid->TillCell(*Stone), EPSTileInteractionResult::NoEffect);
		TestEqual(TEXT("Stone remains stone"), Grid->GetGroundTile(*Stone), EPSTileType::Stone);
	}
	TestEqual(TEXT("Outside world cannot be tilled"), Grid->TillCell(FIntPoint(10000, 10000)), EPSTileInteractionResult::InvalidCell);
	TestTrue(TEXT("Test found farmland for planting"), !TilledCells.IsEmpty());
	if (!TilledCells.IsEmpty())
	{
		TestEqual(TEXT("Seed can be planted on farmland"), Grid->PlantSeed(TilledCells[0]), EPSTileInteractionResult::Planted);
		TestEqual(TEXT("Planted seed is stored separately from ground"), Grid->GetCropType(TilledCells[0]), EPSCropType::TestCrop);
		TestEqual(TEXT("A tile cannot be planted twice"), Grid->PlantSeed(TilledCells[0]), EPSTileInteractionResult::NoEffect);
		TestEqual(TEXT("Bare hands remove a crop"), Grid->RemoveCrop(TilledCells[0]), EPSTileInteractionResult::CropRemoved);
		TestEqual(TEXT("Removed crop leaves tilled soil empty"), Grid->GetCropType(TilledCells[0]), EPSCropType::None);
		TestEqual(TEXT("Removed crop can be replanted"), Grid->PlantSeed(TilledCells[0]), EPSTileInteractionResult::Planted);
	}
	if (Stone)
	{
		TestEqual(TEXT("Seed cannot be planted on stone"), Grid->PlantSeed(*Stone), EPSTileInteractionResult::NoEffect);
	}
	TestEqual(TEXT("Outside world cannot be planted"), Grid->PlantSeed(FIntPoint(10000, 10000)), EPSTileInteractionResult::InvalidCell);
	UPSWorldSaveGame* Saved = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	TestNotNull(TEXT("Farmland save can be loaded"), Saved);
	if (Saved)
	{
		int32 TilledCount = 0;
		int32 CropCount = 0;
		for (const FPSChunkSaveData& Chunk : Saved->ModifiedChunks)
			for (const FPSTileCell& Cell : Chunk.Cells)
			{
				if (Cell.GroundType == EPSTileType::TilledSoil) ++TilledCount;
				if (Cell.CropType != EPSCropType::None) ++CropCount;
			}
		TestEqual(TEXT("Both tilled tiles survive serialization"), TilledCount, 2);
		TestEqual(TEXT("Planted seed survives serialization"), CropCount, 1);
		TestTrue(TEXT("Current game time is saved"), Saved->SavedClockHalfHour >= 0);
	}
	APSTileChunkActor* Renderer = World->SpawnActor<APSTileChunkActor>();
	FPSChunkData Chunk;
	Chunk.Cells.SetNum(4);
	Chunk.Cells[0].GroundType = EPSTileType::Grass;
	Chunk.Cells[1].GroundType = EPSTileType::Dirt;
	Chunk.Cells[2].GroundType = EPSTileType::Stone;
	Chunk.Cells[3].GroundType = EPSTileType::TilledSoil;
	Chunk.Cells[3].CropType = EPSCropType::TestCrop;
	Renderer->Rebuild(Chunk, 2, 100.0f);
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Renderer->GetComponents(Components);
	TestEqual(TEXT("Ground and seed have renderers"), Components.Num(), 6);
	for (auto* Component : Components)
		TestEqual(TEXT("Each ground type renders one tile"), Component->GetInstanceCount(), Component->GetFName() == TEXT("WaterInstances") ? 0 : 1);
	Renderer->Rebuild(Chunk, 2, 100.0f);
	for (auto* Component : Components)
		TestEqual(TEXT("Rebuilding does not duplicate tiles"), Component->GetInstanceCount(), Component->GetFName() == TEXT("WaterInstances") ? 0 : 1);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSFarmingInventoryTest, "PeaceSign.Farming.InventoryIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSFarmingInventoryTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	const FString WorldSlot = TEXT("FarmingInventoryWorld_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSGridWorld* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = WorldSlot;
	Grid->FinishSpawning(FTransform::Identity);
	const FString InventorySlot = TEXT("FarmingInventory_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSPlayerController* Controller = World->SpawnActorDeferred<APSPlayerController>(
		APSPlayerController::StaticClass(), FTransform::Identity);
	Controller->InventoryComponent->SaveSlotName = InventorySlot;
	Controller->InventoryComponent->bAutoSave = false;
	Controller->SkillComponent->SaveSlotName = TEXT("FarmingSkillTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Controller->SkillComponent->bAutoSave = false;
	Controller->GridWorld = Grid;
	Controller->FinishSpawning(FTransform::Identity);
	Controller->InventoryComponent->ResetToDefaults();
	Controller->SkillComponent->ResetSkills();
	TestEqual(TEXT("Controller instance has ten hotbar mappings"), Controller->HotbarMappingContext->GetMappings().Num(), 10);
	for (const FEnhancedActionKeyMapping& Mapping : Controller->HotbarMappingContext->GetMappings())
	{
		TestTrue(TEXT("Instance hotbar mapping references one of its actions"),
			Controller->HotbarSlotActions.Contains(Mapping.Action));
	}

	TArray<FIntPoint> Cells;
	for (int32 Y = -10; Y < 10 && Cells.Num() < 2; ++Y)
		for (int32 X = -10; X < 10 && Cells.Num() < 2; ++X)
			if (Grid->GetGroundTile(FIntPoint(X, Y)) == EPSTileType::Grass) Cells.Add(FIntPoint(X, Y));
	if (!TestEqual(TEXT("Two farm cells are available"), Cells.Num(), 2))
	{
		World->DestroyWorld(false);
		return false;
	}
	for (const FIntPoint Cell : Cells) Grid->TillCell(Cell);

	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("Slot one equips its hoe"), Controller->GetEquipment(), EPSEquipment::Hoe);
	TestTrue(TEXT("Moving the selected hoe succeeds"), Controller->InventoryComponent->MoveItem(
		EPSInventoryArea::Hotbar, 0, EPSInventoryArea::Hotbar, 3));
	TestEqual(TEXT("Empty selected slot becomes bare hands"), Controller->GetEquipment(), EPSEquipment::BareHands);
	TestTrue(TEXT("Returning the hoe refreshes equipment"), Controller->InventoryComponent->MoveItem(
		EPSInventoryArea::Hotbar, 3, EPSInventoryArea::Hotbar, 0));
	TestEqual(TEXT("Returned hoe equips again"), Controller->GetEquipment(), EPSEquipment::Hoe);

	Controller->SelectHotbarSlot(1);
	TestEqual(TEXT("Slot two equips its seed"), Controller->GetEquipment(), EPSEquipment::Seed);
	const int32 SeedsBefore = Controller->InventoryComponent->GetHotbarSlot(1).Quantity;
	TestEqual(TEXT("Selected seed plants"), Controller->UseEquippedItemOnCell(Cells[0]), EPSTileInteractionResult::Planted);
	TestEqual(TEXT("Successful planting consumes one selected seed"), Controller->InventoryComponent->GetHotbarSlot(1).Quantity, SeedsBefore - 1);
	TestEqual(TEXT("Planted tile stores crop data ID"), Grid->GetCropId(Cells[0]), 0);
	TestEqual(TEXT("Failed repeat planting consumes nothing"), Controller->UseEquippedItemOnCell(Cells[0]), EPSTileInteractionResult::NoEffect);
	TestEqual(TEXT("Rejected planting preserves seeds"), Controller->InventoryComponent->GetHotbarSlot(1).Quantity, SeedsBefore - 1);

	UPSGameTimeSubsystem* Clock = World->GetSubsystem<UPSGameTimeSubsystem>();
	TestNotNull(TEXT("Growth clock exists"), Clock);
	Clock->Tick(5760.0f);
	TestEqual(TEXT("Crop reaches harvest stage"), Grid->GetCropStage(Cells[0]), PSCropGrowth::MaxStage);
	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("Hoe harvest succeeds"), Controller->UseEquippedItemOnCell(Cells[0]), EPSTileInteractionResult::Harvested);
	TestEqual(TEXT("Successful harvest consumes one hoe durability"), Controller->InventoryComponent->GetHotbarSlot(0).CurrentDurability, 99);
	TestEqual(TEXT("Successful harvest awards two farming XP"),
		Controller->SkillComponent->GetSkillState(EPSPlayerSkillField::Farming).Experience, 2);
	TestEqual(TEXT("Harvest adds matching produce"), Controller->InventoryComponent->CountItem(EPSItemType::TestCrop, 0), 1);
	TestEqual(TEXT("Harvest leaves empty tilled soil"), Grid->GetCropType(Cells[0]), EPSCropType::None);

	Controller->SelectHotbarSlot(1);
	TestEqual(TEXT("Second seed plants"), Controller->UseEquippedItemOnCell(Cells[1]), EPSTileInteractionResult::Planted);
	Clock->Tick(5760.0f);
	TestTrue(TEXT("Produce stack can be filled"), Controller->InventoryComponent->AddItem(EPSItemType::TestCrop, 998, 0));
	TestTrue(TEXT("Remaining bag cells can be filled"), Controller->InventoryComponent->AddItem(EPSItemType::Wood, 8991));
	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("Full inventory blocks harvest"), Controller->UseEquippedItemOnCell(Cells[1]), EPSTileInteractionResult::InventoryFull);
	TestEqual(TEXT("Blocked harvest keeps mature crop"), Grid->GetCropStage(Cells[1]), PSCropGrowth::MaxStage);
	TestEqual(TEXT("Blocked harvest does not add produce"), Controller->InventoryComponent->CountItem(EPSItemType::TestCrop, 0), 999);
	TestEqual(TEXT("Blocked harvest consumes no hoe durability"), Controller->InventoryComponent->GetHotbarSlot(0).CurrentDurability, 99);

	UGameplayStatics::DeleteGameInSlot(WorldSlot, 0);
	UGameplayStatics::DeleteGameInSlot(InventorySlot, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
