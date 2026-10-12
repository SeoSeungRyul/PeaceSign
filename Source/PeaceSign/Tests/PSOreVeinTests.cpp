#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSOreVeinGeneration.h"
#include "../World/PSWorldSaveGame.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSOreVeinTest, "PeaceSign.Mining.VeinGenerationAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSOreVeinTest::RunTest(const FString& Parameters)
{
	// Exhaust all percent rolls: probabilities must match the design exactly, not merely sum to 100.
	constexpr int32 Expected[8][8] = {
		{70,30,0,0,0,0,0,0}, {35,45,20,0,0,0,0,0}, {25,20,40,15,0,0,0,0}, {20,10,15,40,15,0,0,0},
		{20,5,10,15,35,15,0,0}, {18,4,6,10,15,32,15,0}, {15,3,5,7,10,15,30,15}, {15,2,3,5,10,15,20,30}};
	for (int32 Band = 0; Band < 8; ++Band)
	{
		int32 Counts[8] = {};
		for (int32 Roll = 0; Roll < 100; ++Roll) ++Counts[static_cast<int32>(PSOreGeneration::SelectMineral(Band, Roll))];
		for (int32 Mineral = 0; Mineral < 8; ++Mineral)
			TestEqual(TEXT("Exact mineral probability from the October 12 table"), Counts[Mineral], Expected[Band][Mineral]);
		TestEqual(TEXT("Interval lower bound belongs to the new band"), PSOreGeneration::DistanceBand(10.0 + Band * 10.0, 10.0, 90.0), Band);
	}
	TestEqual(TEXT("25 percent belongs to band three"), PSOreGeneration::DistanceBand(30, 10, 90), 2);
	TestEqual(TEXT("100 percent stays in the eighth band"), PSOreGeneration::DistanceBand(90, 10, 90), 7);
	TestEqual(TEXT("Equal distance limits use the first band"), PSOreGeneration::DistanceBand(99, 5, 5), 0);
	TestEqual(TEXT("Negative ratio clamps to the first band"), PSOreGeneration::DistanceBand(0, 10, 90), 0);

	int32 VeinsSeen = 0;
	for (int32 Seed = 0; Seed < 64; ++Seed)
	{
		FPSChunkData Data;
		Data.Coordinate = FIntPoint(-2, 1);
		Data.Cells.SetNum(32 * 32);
		for (FPSTileCell& Tile : Data.Cells) Tile.ObjectType = EPSWorldObjectType::Stone;
		PSOreGeneration::GenerateVeins(Data, 32, Seed, FIntPoint::ZeroValue, 0, 100);
		TestTrue(TEXT("At most three veins per chunk"), Data.Veins.Num() <= 3);
		VeinsSeen += Data.Veins.Num();
		for (int32 Id = 0; Id < Data.Veins.Num(); ++Id)
		{
			const FPSOreVein& Vein = Data.Veins[Id];
			TestTrue(TEXT("Vein has four to ten blocks"), Vein.Cells.Num() >= 4 && Vein.Cells.Num() <= 10);
			TestTrue(TEXT("Center belongs to its vein"), Vein.Cells.Contains(Vein.Center));
			for (int32 Other = 0; Other < Id; ++Other)
				TestTrue(TEXT("Centers are at least six tiles apart"), (FVector2D(Vein.Center) - FVector2D(Data.Veins[Other].Center)).SizeSquared() >= 36);
			TArray<FIntPoint> Reached = {Vein.Center};
			for (int32 Step = 0; Step < Reached.Num(); ++Step)
				for (const FIntPoint Direction : {FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1)})
					if (Vein.Cells.Contains(Reached[Step] + Direction)) Reached.AddUnique(Reached[Step] + Direction);
			TestEqual(TEXT("Every block is orthogonally reachable from the center"), Reached.Num(), Vein.Cells.Num());
			for (const FIntPoint Cell : Vein.Cells)
			{
				TestEqual(TEXT("Vein never crosses a chunk boundary"), PSGrid::CellToChunk(Cell, 32), Data.Coordinate);
				const FPSTileCell& Tile = Data.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell,32),32)];
				TestEqual(TEXT("All vein blocks use one mineral"), Tile.MineralType, Vein.MineralType);
				TestEqual(TEXT("Block membership is unique"), Tile.VeinId, Id);
			}
		}
		FPSChunkData Isolated;
		Isolated.Cells.SetNum(16);
		for (int32 Index : {0,5,10,15}) Isolated.Cells[Index].ObjectType = EPSWorldObjectType::Stone;
		PSOreGeneration::GenerateVeins(Isolated,4,Seed,FIntPoint::ZeroValue,0,10);
		TestEqual(TEXT("Diagonal-only or undersized candidates are cancelled"), Isolated.Veins.Num(), 0);
		for (const FPSTileCell& Tile : Isolated.Cells)
			TestEqual(TEXT("Cancellation leaves no partial ore"), Tile.MineralType, EPSMineralType::None);
	}
	TestTrue(TEXT("Property tests exercised actual veins"), VeinsSeen > 0);
	FPSChunkData Sea;
	Sea.Cells.SetNum(1024);
	for (FPSTileCell& Tile : Sea.Cells) Tile.GroundType = EPSTileType::Water;
	PSOreGeneration::GenerateVeins(Sea,32,1337,FIntPoint::ZeroValue,0,100);
	TestTrue(TEXT("All-sea chunk has no veins"), Sea.Veins.IsEmpty());

	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	const FString Slot = TEXT("OreVeinTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSGridWorld* Grid = World->SpawnActor<APSGridWorld>();
	Grid->SaveSlotName = Slot;
	Grid->WorldHalfExtentInCells = 96;
	FPSMiningExclusionArea Exclusion;
	Exclusion.Min = FIntPoint(32,32); Exclusion.Max = FIntPoint(63,63);
	Grid->OreGenerationSettings.Exclusions.Add(Exclusion);
	FPSTileStamp Stamp; Stamp.Origin = FIntPoint(-12,18); Stamp.Set = 1;
	Grid->TileStamps.Add(Stamp);
	FIntPoint OreCell = FIntPoint::ZeroValue;
	FIntPoint OreChunk = FIntPoint::ZeroValue;
	bool bFoundOre = false;
	for (int32 Y = -3; Y < 3; ++Y)
		for (int32 X = -3; X < 3; ++X)
		{
			const FPSChunkData Generated = Grid->GenerateChunk(FIntPoint(X,Y));
			for (int32 Index = 0; Index < Generated.Cells.Num(); ++Index)
			{
				const FPSTileCell& Tile = Generated.Cells[Index];
				const FIntPoint Cell = Generated.Coordinate * 32 + FIntPoint(Index % 32,Index / 32);
				if (Tile.GroundType == EPSTileType::Water || PSOreGeneration::IsReserved(Cell,Grid->OreGenerationSettings))
					TestEqual(TEXT("Sea, base, buildings and routes have no rocks"),Tile.ObjectType,EPSWorldObjectType::None);
				if (Tile.bFixedVariant) TestEqual(TEXT("Authored terrain contains no rocks"),Tile.ObjectType,EPSWorldObjectType::None);
				if (!bFoundOre && Tile.MineralType != EPSMineralType::None)
				{
					OreCell = Cell; OreChunk = Generated.Coordinate; bFoundOre = true;
				}
			}
		}
	TestTrue(TEXT("Default world generation creates connected mineral deposits"),bFoundOre);
	TestTrue(TEXT("Procedural queries do not mark chunks visited"),Grid->ModifiedChunks.IsEmpty());
	if (bFoundOre)
	{
		const FPSChunkData Original = Grid->GetOrCreateChunk(OreChunk);
		TestTrue(TEXT("First visit records unmodified veins"),Grid->ModifiedChunks.Contains(OreChunk));
		const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(OreCell,32),32);
		TestEqual(TEXT("Ore can be partially mined"),Grid->MineCell(OreCell,10),EPSTileInteractionResult::StoneDamaged);
		const int32 RemainingHP = Grid->GetStoneHealth(OreCell);
		Grid->LoadedChunks.Remove(OreChunk);
		TestEqual(TEXT("Unloading does not reroll or repair an ore block"),Grid->GetOrCreateChunk(OreChunk).Cells[Index].ObjectHealth,RemainingHP);
		Grid->SaveWorld();
		APSGridWorld* Reloaded = World->SpawnActor<APSGridWorld>();
		Reloaded->SaveSlotName = Slot;
		Reloaded->WorldSeed = 98765; Reloaded->ChunkSize = 16; Reloaded->WorldHalfExtentInCells = 10;
		Reloaded->OreGenerationSettings.RockPatchChance = 0;
		Reloaded->LoadWorld();
		TestEqual(TEXT("Saved chunk size overrides changed editor defaults"),Reloaded->ChunkSize,32);
		TestEqual(TEXT("Saved world extent is restored"),Reloaded->WorldHalfExtentInCells,96);
		TestEqual(TEXT("Mining generator version is restored"),Reloaded->MiningGenerationVersion,1);
		TestEqual(TEXT("Remaining HP survives restart"),Reloaded->GetStoneHealth(OreCell),RemainingHP);
		const FPSChunkData Restored = Reloaded->GetOrCreateChunk(OreChunk);
		TestEqual(TEXT("Original vein membership survives restart"),Restored.Veins.Num(),Original.Veins.Num());
		for (int32 Id = 0; Id < Original.Veins.Num(); ++Id)
		{
			TestEqual(TEXT("Original vein center is saved"),Restored.Veins[Id].Center,Original.Veins[Id].Center);
			TestTrue(TEXT("Original vein shape is saved"),Restored.Veins[Id].Cells == Original.Veins[Id].Cells);
		}
		const FPSChunkData Unvisited = Grid->GenerateChunk(FIntPoint(-3,-3));
		const FPSChunkData ReloadedUnvisited = Reloaded->GenerateChunk(FIntPoint(-3,-3));
		for (int32 CellIndex = 0; CellIndex < Unvisited.Cells.Num(); ++CellIndex)
		{
			TestEqual(TEXT("Unvisited rocks retain their saved generation rules"),ReloadedUnvisited.Cells[CellIndex].ObjectType,Unvisited.Cells[CellIndex].ObjectType);
			TestEqual(TEXT("Unvisited minerals never reroll"),ReloadedUnvisited.Cells[CellIndex].MineralType,Unvisited.Cells[CellIndex].MineralType);
		}
		TestEqual(TEXT("Mining destroys one ore block"),Grid->MineCell(OreCell,1000),EPSTileInteractionResult::Mined);
		Reloaded->LoadWorld();
		TestEqual(TEXT("Destroyed ore stays absent after restart"),Reloaded->GetWorldObjectType(OreCell),EPSWorldObjectType::None);
		TestTrue(TEXT("Development reset succeeds"),Grid->ResetWorld());
		TestEqual(TEXT("Reset restores original mineral"),Grid->GetMineralType(OreCell),Original.Cells[Index].MineralType);
		TestEqual(TEXT("Reset restores full mineral HP"),Grid->GetStoneHealth(OreCell),Original.Cells[Index].ObjectHealth);
	}

	// A real pre-vein 16x16 save must retain damage, harvested blocks and its old generator.
	const FString LegacySlot = Slot + TEXT("_Legacy");
	UPSWorldSaveGame* Legacy = NewObject<UPSWorldSaveGame>();
	Legacy->DataVersion = 3;
	FPSChunkSaveData LegacyChunk; LegacyChunk.Cells.SetNum(256);
	LegacyChunk.Cells[17].ObjectType = EPSWorldObjectType::Stone;
	LegacyChunk.Cells[17].MineralType = EPSMineralType::Copper;
	LegacyChunk.Cells[17].ObjectHealth = 35;
	Legacy->ModifiedChunks.Add(LegacyChunk);
	UGameplayStatics::SaveGameToSlot(Legacy,LegacySlot,0);
	APSGridWorld* OldGrid = World->SpawnActor<APSGridWorld>(); OldGrid->SaveSlotName = LegacySlot; OldGrid->LoadWorld();
	TestEqual(TEXT("Pre-vein chunks retain 16x16 indexing"),OldGrid->ChunkSize,16);
	TestEqual(TEXT("Pre-vein worlds retain the legacy generator"),OldGrid->MiningGenerationVersion,0);
	TestEqual(TEXT("Old mineral damage is retained"),OldGrid->GetStoneHealth(FIntPoint(1,1)),35);
	TestEqual(TEXT("Old empty blocks remain empty"),OldGrid->GetWorldObjectType(FIntPoint(2,1)),EPSWorldObjectType::None);
	OldGrid->SaveWorld(); OldGrid->LoadWorld();
	TestEqual(TEXT("Resaving old worlds does not upgrade their generator"),OldGrid->MiningGenerationVersion,0);
	TestEqual(TEXT("Resaving old worlds does not change chunk size"),OldGrid->ChunkSize,16);
	TestTrue(TEXT("Resetting an old world succeeds"),OldGrid->ResetWorld());
	OldGrid->LoadWorld();
	TestEqual(TEXT("Resetting then restarting an old world retains its generator"),OldGrid->MiningGenerationVersion,0);
	TestEqual(TEXT("Resetting then restarting an old world retains its chunk size"),OldGrid->ChunkSize,16);
	UGameplayStatics::DeleteGameInSlot(Slot,0);
	UGameplayStatics::DeleteGameInSlot(LegacySlot,0);
	World->DestroyWorld(false);
	return true;
}
#endif
