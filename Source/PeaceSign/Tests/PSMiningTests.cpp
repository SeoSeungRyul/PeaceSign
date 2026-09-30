#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"
#include "../World/PSTileChunkActor.h"
#include "../PSPlayerController.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSItemTypes.h"
#include "../Inventory/PSWorldItemActor.h"
#include "Components/BoxComponent.h"
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
	for (int32 Y = -49; Y < 50 && !bFoundStone; ++Y)
		for (int32 X = -49; X < 50 && !bFoundStone; ++X)
		{
			StoneCell = FIntPoint(X, Y);
			bFoundStone = Grid->GetGroundTile(StoneCell) == EPSTileType::Stone
				&& Grid->GetMineralType(StoneCell) == EPSMineralType::None;
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
	TestEqual(TEXT("Bare hands use mining power ten"), PSItems::GetMiningPower(nullptr), 10);
	TestEqual(TEXT("Copper block health matches the design"), APSGridWorld::GetStoneMaxHealth(EPSMineralType::Copper), 45);
	TestEqual(TEXT("Iron block health matches the design"), APSGridWorld::GetStoneMaxHealth(EPSMineralType::Iron), 75);
	TestEqual(TEXT("Silver block health matches the design"), APSGridWorld::GetStoneMaxHealth(EPSMineralType::Silver), 120);
	TestEqual(TEXT("Gold block health matches the design"), APSGridWorld::GetStoneMaxHealth(EPSMineralType::Gold), 195);
	TestEqual(TEXT("Titanium block health matches the design"), APSGridWorld::GetStoneMaxHealth(EPSMineralType::Titanium), 315);
	TestEqual(TEXT("Lumistone block health matches the design"), APSGridWorld::GetStoneMaxHealth(EPSMineralType::Lumistone), 510);
	TestEqual(TEXT("Asterium block health matches the design"), APSGridWorld::GetStoneMaxHealth(EPSMineralType::Asterium), 825);
	bool bFoundGeneratedMineral = false;
	for (int32 Y = -49; Y < 50 && !bFoundGeneratedMineral; ++Y)
		for (int32 X = -49; X < 50 && !bFoundGeneratedMineral; ++X)
			bFoundGeneratedMineral = Grid->GetMineralType(FIntPoint(X, Y)) != EPSMineralType::None;
	TestTrue(TEXT("Procedural stone tiles include minerals"), bFoundGeneratedMineral);
	FPSItemStack TierPickaxe;
	TierPickaxe.ItemType = EPSItemType::Pickaxe;
	TierPickaxe.Quantity = 1;
	const TArray<TPair<FName, int32>> PickaxePowers = {
		{PSItemIds::StonePickaxe, 10}, {PSItemIds::CopperPickaxe, 15}, {PSItemIds::IronPickaxe, 25},
		{PSItemIds::SilverPickaxe, 40}, {PSItemIds::GoldPickaxe, 65}, {PSItemIds::TitaniumPickaxe, 105},
		{PSItemIds::LumistonePickaxe, 170}, {PSItemIds::AsteriumPickaxe, 275}};
	for (const TPair<FName, int32>& Tier : PickaxePowers)
	{
		TierPickaxe.ItemId = Tier.Key;
		TestEqual(*FString::Printf(TEXT("%s mining power matches the design"), *Tier.Key.ToString()),
			PSItems::GetMiningPower(&TierPickaxe), Tier.Value);
	}
	const FIntPoint StoneChunk = PSGrid::CellToChunk(StoneCell, Grid->ChunkSize);
	Grid->SpawnChunkRenderer(StoneChunk);
	APSTileChunkActor* Renderer = Grid->ActiveChunkActors.FindRef(StoneChunk);
	TestNotNull(TEXT("Stone chunk renderer exists"), Renderer);
	UHierarchicalInstancedStaticMeshComponent* StoneInstances = nullptr;
	UHierarchicalInstancedStaticMeshComponent* CopperOreInstances = nullptr;
	if (Renderer)
	{
		TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
		Renderer->GetComponents(Components);
		for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
		{
			if (Component->GetFName() == TEXT("StoneInstances")) StoneInstances = Component;
			if (Component->GetFName() == TEXT("CopperOreInstances")) CopperOreInstances = Component;
		}
	}
	TestNotNull(TEXT("Stone instance renderer exists"), StoneInstances);
	const auto IsCellPawnBlocked = [Renderer, Grid](const FIntPoint Cell)
	{
		if (!Renderer) return false;
		const FIntPoint Local = PSGrid::CellToLocal(Cell, Grid->ChunkSize);
		const FVector CellCenter(
			(static_cast<float>(Local.X) + 0.5f) * Grid->CellSize,
			(static_cast<float>(Local.Y) + 0.5f) * Grid->CellSize,
			0.0f);
		TArray<UBoxComponent*> Blockers;
		Renderer->GetComponents(Blockers);
		for (const UBoxComponent* Blocker : Blockers)
		{
			if (!Blocker || !Blocker->IsRegistered()
				|| Blocker->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
				continue;
			const FVector Center = Blocker->GetRelativeLocation();
			const FVector Extent = Blocker->GetUnscaledBoxExtent();
			if (FMath::Abs(CellCenter.X - Center.X) <= Extent.X
				&& FMath::Abs(CellCenter.Y - Center.Y) <= Extent.Y)
				return true;
		}
		return false;
	};
	TestTrue(TEXT("Mineable stone blocks pawn movement"), IsCellPawnBlocked(StoneCell));
	const int32 InitialRenderedStones = StoneInstances ? StoneInstances->GetInstanceCount() : 0;
	TestEqual(TEXT("Stone begins with thirty health"), Grid->GetStoneHealth(StoneCell), 30);
	TestEqual(TEXT("First hit damages stone"), Controller->UseEquippedItemOnCell(StoneCell),
		EPSTileInteractionResult::StoneDamaged);
	TestEqual(TEXT("Bare-hand hit removes ten health"), Grid->GetStoneHealth(StoneCell), 20);
	if (UPSWorldSaveGame* PartialSave = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
	{
		TestEqual(TEXT("Mining save uses the mineral data version"), PartialSave->DataVersion, 2);
		const FIntPoint ChunkCoordinate = PSGrid::CellToChunk(StoneCell, Grid->ChunkSize);
		const FPSChunkSaveData* PartialChunk = PartialSave->ModifiedChunks.FindByPredicate(
			[ChunkCoordinate](const FPSChunkSaveData& Chunk) { return Chunk.Coordinate == ChunkCoordinate; });
		TestNotNull(TEXT("Damaged stone chunk is saved"), PartialChunk);
		if (PartialChunk)
		{
			const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(StoneCell, Grid->ChunkSize), Grid->ChunkSize);
			TestEqual(TEXT("Partial stone health is saved"), PartialChunk->Cells[Index].StoneHealth, 20);
		}
	}
	else
	{
		AddError(TEXT("Damaged stone save could not be loaded"));
	}
	Controller->SelectHotbarSlot(3);
	TestEqual(TEXT("Starter stone pickaxe uses mining power ten"),
		PSItems::GetMiningPower(Controller->GetInventoryComponent()->FindHotbarSlot(3)), 10);
	TestEqual(TEXT("Second hit damages stone"), Controller->UseEquippedItemOnCell(StoneCell),
		EPSTileInteractionResult::StoneDamaged);
	TestEqual(TEXT("Stone-pickaxe hit leaves ten health"), Grid->GetStoneHealth(StoneCell), 10);

	const int32 InventoryStoneBefore = Controller->GetInventoryComponent()->CountItem(EPSItemType::Stone);
	int32 DropsBefore = 0;
	for (TActorIterator<APSWorldItemActor> It(World); It; ++It) ++DropsBefore;
	TestEqual(TEXT("Third hit destroys stone"), Controller->UseEquippedItemOnCell(StoneCell),
		EPSTileInteractionResult::Mined);
	TestEqual(TEXT("Destroyed stone exposes dirt"), Grid->GetGroundTile(StoneCell), EPSTileType::Dirt);
	TestEqual(TEXT("Destroyed stone has no health"), Grid->GetStoneHealth(StoneCell), 0);
	TestFalse(TEXT("Mined dirt no longer blocks pawn movement"), IsCellPawnBlocked(StoneCell));
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
	TestEqual(TEXT("Reset restores full stone health"), Grid->GetStoneHealth(StoneCell), 30);
	TestTrue(TEXT("Reset stone blocks pawn movement again"), IsCellPawnBlocked(StoneCell));
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

	FPSChunkData& OreChunk = Grid->GetOrCreateChunk(StoneChunk);
	const int32 OreIndex = PSGrid::LocalToIndex(PSGrid::CellToLocal(StoneCell, Grid->ChunkSize), Grid->ChunkSize);
	OreChunk.Cells[OreIndex].GroundType = EPSTileType::Stone;
	OreChunk.Cells[OreIndex].MineralType = EPSMineralType::Copper;
	OreChunk.Cells[OreIndex].StoneHealth = APSGridWorld::GetStoneMaxHealth(EPSMineralType::Copper);
	Grid->RebuildChunk(StoneChunk);
	TestEqual(TEXT("Copper is applied to the tile data"), Grid->GetMineralType(StoneCell), EPSMineralType::Copper);
	TestEqual(TEXT("Copper tile receives base plus mineral HP"), Grid->GetStoneHealth(StoneCell), 45);
	const int32 CopperInstancesBeforeMining = CopperOreInstances ? CopperOreInstances->GetInstanceCount() : 0;
	TestTrue(TEXT("Copper tile uses the copper renderer"), CopperInstancesBeforeMining > 0);
	const auto CountCopperOre = [Controller]()
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Controller->GetInventoryComponent()->GetUnlockedBagSlotCount(); ++Index)
		{
			const FPSItemStack Slot = Controller->GetInventoryComponent()->GetBagSlot(Index);
			if (Slot.ItemType == EPSItemType::Ore && Slot.ItemId == PSItemIds::CopperOre) Count += Slot.Quantity;
		}
		return Count;
	};
	const int32 CopperOreBefore = CountCopperOre();
	TestEqual(TEXT("First copper hit is saved as damage"),
		Controller->UseEquippedItemOnCell(StoneCell), EPSTileInteractionResult::StoneDamaged);
	if (UPSWorldSaveGame* CopperSave = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
	{
		const FPSChunkSaveData* SavedOreChunk = CopperSave->ModifiedChunks.FindByPredicate(
			[StoneChunk](const FPSChunkSaveData& Chunk) { return Chunk.Coordinate == StoneChunk; });
		TestNotNull(TEXT("Copper tile chunk is saved"), SavedOreChunk);
		if (SavedOreChunk)
		{
			TestEqual(TEXT("Copper mineral type is saved"),
				SavedOreChunk->Cells[OreIndex].MineralType, EPSMineralType::Copper);
			TestEqual(TEXT("Copper remaining HP is saved"), SavedOreChunk->Cells[OreIndex].StoneHealth, 35);
		}
	}
	APSGridWorld* RestoredCopperGrid = World->SpawnActorDeferred<APSGridWorld>(
		APSGridWorld::StaticClass(), FTransform(FVector(400000, 0, 0)));
	RestoredCopperGrid->SaveSlotName = Slot;
	RestoredCopperGrid->FinishSpawning(FTransform(FVector(400000, 0, 0)));
	RestoredCopperGrid->LoadWorld();
	TestEqual(TEXT("Reload restores the copper mineral type"),
		RestoredCopperGrid->GetMineralType(StoneCell), EPSMineralType::Copper);
	TestEqual(TEXT("Reload restores copper remaining HP"), RestoredCopperGrid->GetStoneHealth(StoneCell), 35);
	for (int32 Hit = 0; Hit < 3; ++Hit)
		TestEqual(TEXT("Copper block remains after a non-final stone-pickaxe hit"),
			Controller->UseEquippedItemOnCell(StoneCell), EPSTileInteractionResult::StoneDamaged);
	TestEqual(TEXT("Fifth stone-pickaxe hit destroys copper block"),
		Controller->UseEquippedItemOnCell(StoneCell), EPSTileInteractionResult::Mined);
	TestEqual(TEXT("Pickaxe mining adds matching copper ore"), CountCopperOre(), CopperOreBefore + 1);
	if (CopperOreInstances)
		TestEqual(TEXT("Destroyed copper disappears from its renderer"),
			CopperOreInstances->GetInstanceCount(), CopperInstancesBeforeMining - 1);

	OreChunk.Cells[OreIndex].GroundType = EPSTileType::Stone;
	OreChunk.Cells[OreIndex].MineralType = EPSMineralType::Copper;
	OreChunk.Cells[OreIndex].StoneHealth = APSGridWorld::GetStoneMaxHealth(EPSMineralType::Copper);
	Grid->RebuildChunk(StoneChunk);
	Controller->SelectHotbarSlot(4);
	const int32 CopperOreBeforeBareHands = CountCopperOre();
	for (int32 Hit = 0; Hit < 4; ++Hit)
		TestEqual(TEXT("Bare hands can damage a copper block"),
			Controller->UseEquippedItemOnCell(StoneCell), EPSTileInteractionResult::StoneDamaged);
	TestEqual(TEXT("Bare hands can destroy a copper block"),
		Controller->UseEquippedItemOnCell(StoneCell), EPSTileInteractionResult::Mined);
	TestEqual(TEXT("Bare hands do not award embedded copper ore"),
		CountCopperOre(), CopperOreBeforeBareHands);

	const FString LegacySlot = TEXT("MiningLegacyTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPSWorldSaveGame* LegacySave = NewObject<UPSWorldSaveGame>();
	LegacySave->DataVersion = 0;
	LegacySave->WorldSeed = Grid->WorldSeed;
	FPSChunkSaveData LegacyChunk;
	LegacyChunk.Coordinate = StoneChunk;
	LegacyChunk.Cells = Grid->GetOrCreateChunk(StoneChunk).Cells;
	const int32 LegacyStoneIndex = PSGrid::LocalToIndex(
		PSGrid::CellToLocal(StoneCell, Grid->ChunkSize), Grid->ChunkSize);
	LegacyChunk.Cells[LegacyStoneIndex].GroundType = EPSTileType::Stone;
	LegacyChunk.Cells[LegacyStoneIndex].StoneHealth = 2;
	LegacySave->ModifiedChunks.Add(LegacyChunk);
	TestTrue(TEXT("Legacy mining save fixture is written"),
		UGameplayStatics::SaveGameToSlot(LegacySave, LegacySlot, 0));
	APSGridWorld* LegacyGrid = World->SpawnActorDeferred<APSGridWorld>(
		APSGridWorld::StaticClass(), FTransform(FVector(300000, 0, 0)));
	LegacyGrid->SaveSlotName = LegacySlot;
	LegacyGrid->FinishSpawning(FTransform(FVector(300000, 0, 0)));
	LegacyGrid->LoadWorld();
	TestEqual(TEXT("Legacy stone health two migrates to twenty"),
		LegacyGrid->GetStoneHealth(StoneCell), 20);

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(LegacySlot, 0);
	UGameplayStatics::DeleteGameInSlot(Controller->GetInventoryComponent()->SaveSlotName, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
