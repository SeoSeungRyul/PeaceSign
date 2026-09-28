#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"
#include "../World/PSTileChunkActor.h"
#include "../PSPlayerController.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSWorldItemActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSMiningTest, "PeaceSign.Mining.DamageDestroyAndDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSMiningTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Init);
	const FString Slot = TEXT("MiningTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSGridWorld* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot;
	Grid->FinishSpawning(FTransform::Identity);

	FIntPoint StoneCell = FIntPoint::ZeroValue;
	bool bFoundStone = false;
	for (int32 Y = -10; Y < 10 && !bFoundStone; ++Y)
		for (int32 X = -10; X < 10 && !bFoundStone; ++X)
		{
			StoneCell = FIntPoint(X, Y);
			bFoundStone = Grid->GetGroundTile(StoneCell) == EPSTileType::Stone;
		}
	if (!TestTrue(TEXT("Generated terrain contains a stone tile"), bFoundStone))
	{
		World->DestroyWorld(false);
		return false;
	}
	FIntPoint AdjacentLand = FIntPoint::ZeroValue;
	bool bFoundAdjacentLand = false;
	for (const FIntPoint Offset : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1),
		FIntPoint(1, 1), FIntPoint(1, -1), FIntPoint(-1, 1), FIntPoint(-1, -1)})
	{
		AdjacentLand = StoneCell + Offset;
		const EPSTileType Ground = Grid->GetGroundTile(AdjacentLand);
		if (Ground != EPSTileType::Empty && Ground != EPSTileType::Water)
		{
			bFoundAdjacentLand = true;
			break;
		}
	}
	TestTrue(TEXT("Stone has adjacent mineable land"), bFoundAdjacentLand);
	if (bFoundAdjacentLand)
	{
		TestTrue(TEXT("Adjacent stone is in mining range"), Grid->CanMineFrom(AdjacentLand, StoneCell));
		TestFalse(TEXT("Three-cell stone is outside mining range"),
			Grid->CanMineFrom(StoneCell + FIntPoint(3, 0), StoneCell));
	}

	APSPlayerController* Controller = World->SpawnActor<APSPlayerController>();
	Controller->GridWorld = Grid;
	Controller->GetInventoryComponent()->bAutoSave = false;
	Controller->GetInventoryComponent()->SaveSlotName = TEXT("MiningInventoryTest_")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Controller->GetInventoryComponent()->ResetToDefaults();
	const FIntPoint StoneChunk = PSGrid::CellToChunk(StoneCell, Grid->ChunkSize);
	Grid->SpawnChunkRenderer(StoneChunk);
	APSTileChunkActor* Renderer = Grid->ActiveChunkActors.FindRef(StoneChunk);
	TestNotNull(TEXT("Stone chunk renderer exists"), Renderer);
	UHierarchicalInstancedStaticMeshComponent* StoneInstances = nullptr;
	if (Renderer)
	{
		TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
		Renderer->GetComponents(Components);
		for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
			if (Component->GetFName() == TEXT("StoneInstances")) StoneInstances = Component;
	}
	TestNotNull(TEXT("Stone instance renderer exists"), StoneInstances);
	const int32 InitialRenderedStones = StoneInstances ? StoneInstances->GetInstanceCount() : 0;
	TestEqual(TEXT("Stone begins with three health"), Grid->GetStoneHealth(StoneCell), 3);
	TestEqual(TEXT("First hit damages stone"), Controller->UseEquippedItemOnCell(StoneCell),
		EPSTileInteractionResult::StoneDamaged);
	TestEqual(TEXT("First hit leaves two health"), Grid->GetStoneHealth(StoneCell), 2);
	if (UPSWorldSaveGame* PartialSave = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
	{
		const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(StoneCell, Grid->ChunkSize);
		const FPSChunkSaveData* PartialChunk = PartialSave->ModifiedChunks.FindByPredicate(
			[ChunkCoordinate](const FPSChunkSaveData& Chunk) { return Chunk.Coordinate == ChunkCoordinate; });
		TestNotNull(TEXT("Damaged stone chunk is saved"), PartialChunk);
		if (PartialChunk)
		{
			const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(StoneCell, Grid->ChunkSize), Grid->ChunkSize);
			TestEqual(TEXT("Partial stone health is saved"), PartialChunk->Cells[Index].StoneHealth, 2);
		}
	}
	else
	{
		AddError(TEXT("Damaged stone save could not be loaded"));
	}
	TestEqual(TEXT("Second hit damages stone"), Controller->UseEquippedItemOnCell(StoneCell),
		EPSTileInteractionResult::StoneDamaged);
	TestEqual(TEXT("Second hit leaves one health"), Grid->GetStoneHealth(StoneCell), 1);

	const int32 InventoryStoneBefore = Controller->GetInventoryComponent()->CountItem(EPSItemType::Stone);
	int32 DropsBefore = 0;
	for (TActorIterator<APSWorldItemActor> It(World); It; ++It) ++DropsBefore;
	TestEqual(TEXT("Third hit destroys stone"), Controller->UseEquippedItemOnCell(StoneCell),
		EPSTileInteractionResult::Mined);
	TestEqual(TEXT("Destroyed stone exposes dirt"), Grid->GetGroundTile(StoneCell), EPSTileType::Dirt);
	TestEqual(TEXT("Destroyed stone has no health"), Grid->GetStoneHealth(StoneCell), 0);
	TestEqual(TEXT("Destroyed stone is added directly to inventory"),
		Controller->GetInventoryComponent()->CountItem(EPSItemType::Stone), InventoryStoneBefore + 1);
	if (StoneInstances)
		TestEqual(TEXT("Destroyed stone disappears from the active chunk renderer"),
			StoneInstances->GetInstanceCount(), InitialRenderedStones - 1);
	int32 DropsAfter = 0;
	for (TActorIterator<APSWorldItemActor> It(World); It; ++It) ++DropsAfter;
	TestEqual(TEXT("Inventory reward does not leave a duplicate world drop"), DropsAfter, DropsBefore);
	TestEqual(TEXT("Destroyed stone cannot be mined twice"), Controller->UseEquippedItemOnCell(StoneCell),
		EPSTileInteractionResult::NoEffect);
	int32 DropsAfterRepeat = 0;
	for (TActorIterator<APSWorldItemActor> It(World); It; ++It) ++DropsAfterRepeat;
	TestEqual(TEXT("Repeated interaction creates no duplicate drop"), DropsAfterRepeat, DropsAfter);
	TestEqual(TEXT("Repeated interaction creates no duplicate inventory item"),
		Controller->GetInventoryComponent()->CountItem(EPSItemType::Stone), InventoryStoneBefore + 1);

	UPSWorldSaveGame* Saved = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	TestNotNull(TEXT("Mining state save can be loaded"), Saved);
	if (Saved)
	{
		const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(StoneCell, Grid->ChunkSize);
		const FPSChunkSaveData* SavedChunk = Saved->ModifiedChunks.FindByPredicate(
			[ChunkCoordinate](const FPSChunkSaveData& Chunk) { return Chunk.Coordinate == ChunkCoordinate; });
		TestNotNull(TEXT("Destroyed stone chunk is saved"), SavedChunk);
		if (SavedChunk)
		{
			const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(StoneCell, Grid->ChunkSize), Grid->ChunkSize);
			TestEqual(TEXT("Saved stone remains destroyed"), SavedChunk->Cells[Index].GroundType, EPSTileType::Dirt);
			TestEqual(TEXT("Saved destroyed stone has zero health"), SavedChunk->Cells[Index].StoneHealth, 0);
		}
	}
	APSGridWorld* RestoredGrid = World->SpawnActorDeferred<APSGridWorld>(
		APSGridWorld::StaticClass(), FTransform(FVector(100000, 0, 0)));
	RestoredGrid->SaveSlotName = Slot;
	RestoredGrid->FinishSpawning(FTransform(FVector(100000, 0, 0)));
	RestoredGrid->LoadWorld();
	TestEqual(TEXT("Reloaded world keeps the stone destroyed"),
		RestoredGrid->GetGroundTile(StoneCell), EPSTileType::Dirt);
	TestEqual(TEXT("Reloaded destroyed stone has no health"), RestoredGrid->GetStoneHealth(StoneCell), 0);

	TestTrue(TEXT("Development reset succeeds"), Grid->ResetWorld());
	TestEqual(TEXT("Reset regenerates the original stone tile"), Grid->GetGroundTile(StoneCell), EPSTileType::Stone);
	TestEqual(TEXT("Reset restores full stone health"), Grid->GetStoneHealth(StoneCell), 3);
	if (StoneInstances)
		TestEqual(TEXT("Reset immediately restores the visible stone instance"),
			StoneInstances->GetInstanceCount(), InitialRenderedStones);
	TestFalse(TEXT("Reset deletes the modified world save"), UGameplayStatics::DoesSaveGameExist(Slot, 0));
	APSGridWorld* FreshGrid = World->SpawnActorDeferred<APSGridWorld>(
		APSGridWorld::StaticClass(), FTransform(FVector(200000, 0, 0)));
	FreshGrid->SaveSlotName = Slot;
	FreshGrid->FinishSpawning(FTransform(FVector(200000, 0, 0)));
	FreshGrid->LoadWorld();
	TestEqual(TEXT("A new world instance after reset generates the original stone"),
		FreshGrid->GetGroundTile(StoneCell), EPSTileType::Stone);

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(Controller->GetInventoryComponent()->SaveSlotName, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
