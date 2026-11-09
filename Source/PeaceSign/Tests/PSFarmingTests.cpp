#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"
#include "../World/PSTileChunkActor.h"
#include "../World/PSCropGrowth.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "../Data/PSCropDefinition.h"
#include "../Gathering/PSGatheringWorldComponent.h"
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
	TOptional<FIntPoint> Stone;
	TArray<FIntPoint> TilledCells;
	for (int32 Y = -10; Y < 10; ++Y)
		for (int32 X = -10; X < 10; ++X)
		{
			const FIntPoint Cell(X, Y);
			if (Grid->GetWorldObjectType(Cell) == EPSWorldObjectType::Stone) Stone = Cell;
			else Cells.FindOrAdd(Grid->GetGroundTile(Cell), Cell);
		}
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
	TestTrue(TEXT("Generated terrain contains a stone object"), Stone.IsSet());
	if (Stone.IsSet())
	{
		const EPSTileType GroundBelowStone = Grid->GetGroundTile(Stone.GetValue());
		TestEqual(TEXT("Hoe cannot till below a stone object"), Grid->TillCell(Stone.GetValue()), EPSTileInteractionResult::NoEffect);
		TestEqual(TEXT("Stone does not replace its ground"), Grid->GetGroundTile(Stone.GetValue()), GroundBelowStone);
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
	if (Stone.IsSet())
	{
		TestEqual(TEXT("Seed cannot be planted below stone"), Grid->PlantSeed(Stone.GetValue()), EPSTileInteractionResult::NoEffect);
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
	Chunk.Cells[2].GroundType = EPSTileType::Grass;
	Chunk.Cells[2].ObjectType = EPSWorldObjectType::Stone;
	Chunk.Cells[2].ObjectHealth = APSGridWorld::DefaultStoneHealth;
	Chunk.Cells[3].GroundType = EPSTileType::TilledSoil;
	Chunk.Cells[3].Variant = 5;
	Chunk.Cells[3].CropType = EPSCropType::TestCrop;
	Renderer->Rebuild(Chunk, 2, 100.0f);
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Renderer->GetComponents(Components);
	TestEqual(TEXT("Ground, farm artwork, wet soil, seed, and mineral renderers"), Components.Num(), 15);
	const auto ExpectedInstances = [](const UHierarchicalInstancedStaticMeshComponent* Component)
	{
		const FName Name = Component->GetFName();
		if (Name == TEXT("GrassInstances")) return 2;
		return Name == TEXT("DirtInstances")
			|| Name == TEXT("StoneInstances") || Name == TEXT("FarmVariant_5_4")
			|| Name == TEXT("SeedInstances") ? 1 : 0;
	};
	for (auto* Component : Components)
		TestEqual(TEXT("Only populated ground types render one tile"), Component->GetInstanceCount(), ExpectedInstances(Component));
	Renderer->Rebuild(Chunk, 2, 100.0f);
	for (auto* Component : Components)
		TestEqual(TEXT("Rebuilding does not duplicate tiles"), Component->GetInstanceCount(), ExpectedInstances(Component));
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
	Controller->SelectHotbarSlot(3);
	TestEqual(TEXT("Slot four equips its starter pickaxe"), Controller->GetEquipment(), EPSEquipment::Pickaxe);
	TestEqual(TEXT("Controller instance has ten hotbar mappings"), Controller->HotbarMappingContext->GetMappings().Num(), 10);
	for (const FEnhancedActionKeyMapping& Mapping : Controller->HotbarMappingContext->GetMappings())
	{
		TestTrue(TEXT("Instance hotbar mapping references one of its actions"),
			Controller->HotbarSlotActions.Contains(Mapping.Action));
	}

	APSPlayerCharacter* Farmer = World->SpawnActor<APSPlayerCharacter>();
	Controller->Possess(Farmer);
	TArray<FIntPoint> Cells;
	for (int32 Y = -10; Y < 10 && Cells.Num() < 3; ++Y)
		for (int32 X = -10; X < 10 && Cells.Num() < 3; ++X)
			if (Grid->CanTillCell(FIntPoint(X, Y))) Cells.Add(FIntPoint(X, Y));
	if (!TestEqual(TEXT("Three farm cells available"), Cells.Num(), 3)
		|| !TestNotNull(TEXT("Imported lettuce row exists"), Grid->FindCropDefinition(PSItemIds::LettuceCropId)))
	{
		World->DestroyWorld(false);
		return false;
	}
	const FIntPoint LettuceCell = Cells[0];
	const auto StandAt = [&](FIntPoint Cell) { Farmer->SetActorLocation(Grid->CellToWorldCenter(Cell) + FVector(0, 0, 100)); };
	UPSGameTimeSubsystem* Clock = World->GetSubsystem<UPSGameTimeSubsystem>();
	Clock->RestoreClock(12, 0);
	Grid->TillCell(LettuceCell);
	StandAt(LettuceCell);
	Controller->SelectHotbarSlot(1);
	const int32 SeedsBefore = Controller->InventoryComponent->GetHotbarSlot(1).Quantity;
	TestEqual(TEXT("Starter seed has the lettuce item identity"), Controller->InventoryComponent->GetHotbarSlot(1).ItemId, PSItemIds::LettuceSeed);
	TestEqual(TEXT("Level zero cannot plant lettuce"), Controller->UseEquippedItemOnCell(LettuceCell), EPSTileInteractionResult::NoEffect);
	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("First player tilling succeeds"), Controller->UseEquippedItemOnCell(Cells[1]), EPSTileInteractionResult::Tilled);
	TestEqual(TEXT("Tilling opens farming level one"), Controller->SkillComponent->GetSkillLevel(EPSPlayerSkillField::Farming), 1);
	Controller->UseEquippedItemOnCell(Cells[2]);
	TestEqual(TEXT("Second till does not grant bonus XP"), Controller->SkillComponent->GetSkillState(EPSPlayerSkillField::Farming).Experience, 0);
	Controller->SelectHotbarSlot(1);
	StandAt(LettuceCell + FIntPoint(3, 0));
	TestEqual(TEXT("Out-of-range sowing fails"), Controller->UseEquippedItemOnCell(LettuceCell), EPSTileInteractionResult::NoEffect);
	TestEqual(TEXT("Failed sowing preserves seeds"), Controller->InventoryComponent->GetHotbarSlot(1).Quantity, SeedsBefore);
	StandAt(LettuceCell + FIntPoint(2, 0));
	TestEqual(TEXT("Lettuce can be planted two cells away in spring"), Controller->UseEquippedItemOnCell(LettuceCell), EPSTileInteractionResult::Planted);
	TestEqual(TEXT("One seed consumed"), Controller->InventoryComponent->GetHotbarSlot(1).Quantity, SeedsBefore - 1);
	TestEqual(TEXT("Crop ID is carried into the field"), Grid->GetCropId(LettuceCell), PSItemIds::LettuceCropId);
	TestEqual(TEXT("Occupied plot rejects sowing"), Controller->UseEquippedItemOnCell(LettuceCell), EPSTileInteractionResult::NoEffect);
	TestEqual(TEXT("Occupied plot never consumes seeds"), Controller->InventoryComponent->GetHotbarSlot(1).Quantity, SeedsBefore - 1);
	for (int32 Day = 0; Day < 2; ++Day)
	{
		Grid->WaterCell(LettuceCell);
		Clock->AdvanceGameDays(1);
	}
	Grid->WaterCell(LettuceCell);
	Clock->AdvanceGameHours(23);
	Clock->Tick(50.f);
	TestEqual(TEXT("Lettuce at 71h50 is not ready"), Grid->GetCropStage(LettuceCell), 4);
	Clock->Tick(10.f);
	TestEqual(TEXT("Lettuce reaches 100% at 72 watered hours"), Grid->GetCropStage(LettuceCell), 5);
	Grid->WaterCell(LettuceCell);
	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("Hoe no longer harvests ripe crop"), Controller->UseEquippedItemOnCell(LettuceCell), EPSTileInteractionResult::NoEffect);
	Controller->HandleInteract();
	Controller->AdvanceCropHarvest(0.4f);
	Controller->HandleInteractReleased();
	Controller->AdvanceCropHarvest(0.8f);
	TestEqual(TEXT("Released hold preserves ripe crop"), Grid->GetCropStage(LettuceCell), 5);
	Controller->HandleInteract();
	Controller->AdvanceCropHarvest(0.4f);
	StandAt(LettuceCell + FIntPoint(1, 0));
	Controller->AdvanceCropHarvest(0.8f);
	TestEqual(TEXT("Movement cancels harvest even inside range"), Grid->GetCropStage(LettuceCell), 5);
	Controller->HandleInteract();
	Controller->AdvanceCropHarvest(0.4f);
	Farmer->StatsComponent->ApplyDamage(1.f);
	Controller->AdvanceCropHarvest(0.8f);
	TestEqual(TEXT("Damage cancels harvest"), Grid->GetCropStage(LettuceCell), 5);
	Controller->HandleInteract();
	Controller->AdvanceCropHarvest(0.79f);
	TestEqual(TEXT("Harvest cannot finish early"), Grid->GetCropStage(LettuceCell), 5);
	Controller->AdvanceCropHarvest(0.01f);
	Controller->AdvanceCropHarvest(1.0f);
	TestEqual(TEXT("One completed hold awards one lettuce"), Controller->InventoryComponent->CountItem(EPSItemType::TestCrop, PSItemIds::LettuceCropId), 1);
	TestEqual(TEXT("Harvest awards two farming XP"), Controller->SkillComponent->GetSkillState(EPSPlayerSkillField::Farming).Experience, 2);
	TestEqual(TEXT("Harvest retains the soil"), Grid->GetGroundTile(LettuceCell), EPSTileType::TilledSoil);
	TestTrue(TEXT("Harvest retains moisture"), Grid->IsCellWatered(LettuceCell));
	TestEqual(TEXT("Harvest removes only the crop"), Grid->GetCropType(LettuceCell), EPSCropType::None);

	Controller->InventoryComponent->ResetToDefaults();
	TestTrue(TEXT("Full bag fixture"), Controller->InventoryComponent->AddItem(EPSItemType::Wood, 8 * 999));
	Grid->PlantSeed(LettuceCell);
	for (int32 Day = 0; Day < 4; ++Day)
	{
		Grid->WaterCell(LettuceCell);
		Clock->AdvanceGameDays(1);
	}
	Clock->AdvanceGameDays(2);
	TestEqual(TEXT("Ripe crop survives two dry days"), Grid->GetCropStage(LettuceCell), 5);
	const int32 DropsBefore = Grid->Gathering->GetDrops().Num();
	Controller->HandleInteract();
	Controller->AdvanceCropHarvest(0.8f);
	TestEqual(TEXT("A full bag still permits harvest"), Grid->GetCropType(LettuceCell), EPSCropType::None);
	if (TestEqual(TEXT("Full bag creates one persistent produce drop"), Grid->Gathering->GetDrops().Num(), DropsBefore + 1))
		TestEqual(TEXT("The overflow is exactly one crop"), Grid->Gathering->GetDrops().Last().Item.Quantity, 1);
	const auto* SavedHarvest = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(WorldSlot, 0));
	if (TestNotNull(TEXT("World save is readable after harvesting with a full bag"), SavedHarvest))
		TestEqual(TEXT("The world save contains the overflow drop"), SavedHarvest->GatheringDrops.Num(), DropsBefore + 1);
	Clock->AdvanceGameDays(28);
	StandAt(LettuceCell);
	Controller->SelectHotbarSlot(1);
	TestEqual(TEXT("Summer blocks lettuce sowing"), Controller->UseEquippedItemOnCell(LettuceCell), EPSTileInteractionResult::NoEffect);
	TestEqual(TEXT("Season rejection keeps the seed"), Controller->InventoryComponent->GetHotbarSlot(1).Quantity, 24);

	UGameplayStatics::DeleteGameInSlot(WorldSlot, 0);
	UGameplayStatics::DeleteGameInSlot(InventorySlot, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
