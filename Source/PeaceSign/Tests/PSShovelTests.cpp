#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSDirtAutoTileSet.h"
#include "../PSPlayerController.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSInventorySaveGame.h"
#include "../Skills/PSPlayerSkillComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSShovelTest, "PeaceSign.Shovel.TerrainAndInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSShovelTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	const FString WorldSlot = TEXT("ShovelWorld_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString InventorySlot = TEXT("ShovelInventory_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSGridWorld* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = WorldSlot;
	Grid->FinishSpawning(FTransform::Identity);
	APSPlayerController* Controller = World->SpawnActorDeferred<APSPlayerController>(APSPlayerController::StaticClass(), FTransform::Identity);
	UPSInventoryComponent* Inventory = Controller->InventoryComponent;
	Inventory->SaveSlotName = InventorySlot;
	Inventory->bAutoSave = false;
	Controller->SkillComponent->bAutoSave = false;
	Controller->GridWorld = Grid;
	Controller->FinishSpawning(FTransform::Identity);
	Inventory->ResetToDefaults();
	Controller->SelectHotbarSlot(4);
	TestEqual(TEXT("Starter shovel equips from slot five"), Controller->GetEquipment(), EPSEquipment::Shovel);
	TestEqual(TEXT("Shovel starts with 100 durability"), Inventory->GetHotbarSlot(4).CurrentDurability, 100);
	TestEqual(TEXT("Shovels do not stack"), PSItems::GetDefinition(EPSItemType::Shovel).MaxStack, 1);

	// Both positive and negative chunk corners include cardinal and diagonal neighbors.
	for (const FIntPoint Center : {FIntPoint(15, 15), FIntPoint(-1, -1)})
	{
		for (int32 Y = -1; Y <= 1; ++Y)
			for (int32 X = -1; X <= 1; ++X)
			{
				const FIntPoint Cell = Center + FIntPoint(X, Y);
				FPSChunkData& Chunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(Cell, Grid->ChunkSize));
				Chunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, Grid->ChunkSize), Grid->ChunkSize)] = FPSTileCell();
			}
		const FIntPoint East = Center + FIntPoint(1, 0);
		const FIntPoint SouthEast = Center + FIntPoint(1, 1);
		Grid->SetGroundTile(East, EPSTileType::Dirt);
		const uint16 Before = Grid->BuildDirtNeighborMask(East);
		const uint16 DiagonalBefore = Grid->BuildDirtNeighborMask(SouthEast);
		TestEqual(TEXT("Grass becomes dirt"), Controller->UseEquippedItemOnCell(Center), EPSTileInteractionResult::GroundChanged);
		TestEqual(TEXT("Converted ground is dirt"), Grid->GetGroundTile(Center), EPSTileType::Dirt);
		TestEqual(TEXT("Neighbor mask changes across a chunk edge"), Grid->BuildDirtNeighborMask(East),
			static_cast<uint16>(Before | PSDirtAutoTile::West));
		TestEqual(TEXT("Diagonal mask changes across a chunk corner"), Grid->BuildDirtNeighborMask(SouthEast),
			static_cast<uint16>(DiagonalBefore | PSDirtAutoTile::NorthWest));
		const FIntPoint EastChunk = PSGrid::CellToChunk(East, Grid->ChunkSize);
		const FPSChunkData Render = Grid->BuildRenderChunkData(EastChunk, Grid->GetOrCreateChunk(EastChunk));
		const uint8 Variant = Render.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(East, Grid->ChunkSize), Grid->ChunkSize)].Variant;
		TestTrue(TEXT("Neighbor rendering follows the new dirt connection"),
			(PSDirtAutoTile::GetDefaultVariantMask(Variant) & PSDirtAutoTile::West) != 0);
		Grid->LoadedChunks.Reset();
		Grid->LoadWorld();
		TestEqual(TEXT("Converted terrain survives reload"), Grid->GetGroundTile(Center), EPSTileType::Dirt);
		TestEqual(TEXT("Dirt becomes grass"), Controller->UseEquippedItemOnCell(Center), EPSTileInteractionResult::GroundChanged);
		TestEqual(TEXT("Reverse conversion restores grass"), Grid->GetGroundTile(Center), EPSTileType::Grass);
		TestEqual(TEXT("Neighbor mask returns to its original state"), Grid->BuildDirtNeighborMask(East), Before);
	}
	TestEqual(TEXT("Four conversions consume four durability"), Inventory->GetHotbarSlot(4).CurrentDurability, 96);
	const FIntPoint Blocked(2, 2);
	FPSChunkData& Chunk = Grid->GetOrCreateChunk(FIntPoint::ZeroValue);
	FPSTileCell& Tile = Chunk.Cells[PSGrid::LocalToIndex(Blocked, Grid->ChunkSize)];
	Tile = FPSTileCell();
	Tile.ObjectType = EPSWorldObjectType::Stone;
	Tile.ObjectHealth = 30;
	TestEqual(TEXT("Shovel cannot mine a stone"), Controller->UseEquippedItemOnCell(Blocked), EPSTileInteractionResult::NoEffect);
	TestEqual(TEXT("Stone health stays intact"), Grid->GetStoneHealth(Blocked), 30);
	Tile.ObjectType = EPSWorldObjectType::None;
	Tile.CropType = EPSCropType::TestCrop;
	TestEqual(TEXT("Shovel cannot remove a crop"), Controller->UseEquippedItemOnCell(Blocked), EPSTileInteractionResult::NoEffect);
	Tile.CropType = EPSCropType::None;
	for (const EPSTileType Type : {EPSTileType::Water, EPSTileType::TilledSoil})
	{
		Tile.GroundType = Type;
		TestEqual(TEXT("Water and tilled soil are protected"), Controller->UseEquippedItemOnCell(Blocked), EPSTileInteractionResult::NoEffect);
	}
	TestEqual(TEXT("Outside world is rejected"), Controller->UseEquippedItemOnCell(FIntPoint(1000, 1000)), EPSTileInteractionResult::InvalidCell);
	TestEqual(TEXT("Rejected uses preserve durability"), Inventory->GetHotbarSlot(4).CurrentDurability, 96);
	TestEqual(TEXT("No stone reward is generated"), Inventory->CountItem(EPSItemType::Stone), 0);
	TestTrue(TEXT("Full inventory fixture"), Inventory->AddItem(EPSItemType::Wood, 9990));
	Tile.GroundType = EPSTileType::Grass;
	TestEqual(TEXT("Full bag does not block terrain conversion"), Controller->UseEquippedItemOnCell(Blocked), EPSTileInteractionResult::GroundChanged);
	TestEqual(TEXT("Terrain conversion adds no bag items"), Inventory->CountItem(EPSItemType::Wood), 9990);
	TestTrue(TEXT("Inventory saves shovel durability"), Inventory->SaveInventory());
	TestTrue(TEXT("Inventory reloads shovel"), Inventory->LoadInventory());
	TestEqual(TEXT("Saved durability is restored"), Inventory->GetHotbarSlot(4).CurrentDurability, 95);
	bool bDestroyed = false;
	Inventory->ConsumeHotbarDurability(4, 94, bDestroyed);
	TestEqual(TEXT("Final use still converts terrain"), Controller->UseEquippedItemOnCell(Blocked), EPSTileInteractionResult::GroundChanged);
	TestTrue(TEXT("Zero durability removes shovel"), Inventory->GetHotbarSlot(4).IsEmpty());
	TestEqual(TEXT("Broken shovel refreshes equipment"), Controller->GetEquipment(), EPSEquipment::BareHands);
	Inventory->SaveInventory();
	Inventory->LoadInventory();
	TestEqual(TEXT("Broken shovel does not reappear on reload"), Inventory->CountItem(EPSItemType::Shovel), 0);

	// Version four migration preserves worn tools and occupied hotbar slots.
	UPSInventorySaveGame* Legacy = NewObject<UPSInventorySaveGame>();
	Legacy->DataVersion = 4;
	for (int32 Index = 0; Index < UPSInventoryComponent::HotbarSlotCount; ++Index)
		Legacy->HotbarSlots.Add(Inventory->GetHotbarSlot(Index));
	Legacy->BagSlots.SetNum(UPSInventoryComponent::MaxBagSlotCount);
	Legacy->HotbarSlots[4].ItemType = EPSItemType::Wood;
	Legacy->HotbarSlots[4].Quantity = 1;
	Legacy->HotbarSlots[0].CurrentDurability = 37;
	UGameplayStatics::SaveGameToSlot(Legacy, InventorySlot, 0);
	Inventory->LoadInventory();
	TestEqual(TEXT("Migration keeps existing tool durability"), Inventory->GetHotbarSlot(0).CurrentDurability, 37);
	TestEqual(TEXT("Migration keeps occupied fifth slot"), Inventory->GetHotbarSlot(4).ItemType, EPSItemType::Wood);
	TestEqual(TEXT("Migration adds one shovel"), Inventory->CountItem(EPSItemType::Shovel), 1);
	Inventory->SaveInventory();
	Inventory->LoadInventory();
	TestEqual(TEXT("Migration does not duplicate shovel"), Inventory->CountItem(EPSItemType::Shovel), 1);
	UGameplayStatics::DeleteGameInSlot(WorldSlot, 0);
	UGameplayStatics::DeleteGameInSlot(InventorySlot, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
