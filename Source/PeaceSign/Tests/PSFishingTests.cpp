#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSTileChunkActor.h"
#include "../PSPlayerController.h"
#include "../Inventory/PSInventoryComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSFishingTest, "PeaceSign.Fishing.LakesAndRod", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSFishingTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	APSGridWorld* Grid = World->SpawnActor<APSGridWorld>();
	Grid->SaveSlotName = TEXT("FishingTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FIntPoint Water = FIntPoint::ZeroValue;
	FIntPoint Shore = FIntPoint::ZeroValue;
	bool bFoundShore = false;
	for (const int32 Seed : {1337, 42, -17})
	{
		Grid->WorldSeed = Seed;
		// Odd extents also exercise lakes that would otherwise be clipped at the edge.
		for (const int32 Extent : {17, 50})
		{
			Grid->WorldHalfExtentInCells = Extent;
			int32 WaterCount = 0;
			for (int32 Y = -Extent; Y < Extent; ++Y)
				for (int32 X = -Extent; X < Extent; ++X)
				{
					const FIntPoint Cell(X, Y);
					if (Grid->GetGroundTile(Cell) != EPSTileType::Water) continue;
					++WaterCount;
					bool bInSquare = false;
					for (int32 DY : {-1, 1})
						for (int32 DX : {-1, 1})
							bInSquare |= Grid->GetGroundTile(Cell + FIntPoint(DX, 0)) == EPSTileType::Water
								&& Grid->GetGroundTile(Cell + FIntPoint(0, DY)) == EPSTileType::Water
								&& Grid->GetGroundTile(Cell + FIntPoint(DX, DY)) == EPSTileType::Water;
					TestTrue(TEXT("Every water tile belongs to a full 2x2 square"), bInSquare);
				}
			TestTrue(TEXT("Terrain contains lakes"), WaterCount > 0);
			TestTrue(TEXT("Starting cell stays dry"), Grid->GetGroundTile(FIntPoint::ZeroValue) != EPSTileType::Water);
		}
	}
	Grid->WorldSeed = 1337;
	for (int32 Y = -50; Y < 50 && !bFoundShore; ++Y)
		for (int32 X = -50; X < 50 && !bFoundShore; ++X)
		{
			Water = FIntPoint(X, Y);
			Shore = Water - FIntPoint(1, 0);
			bFoundShore = Grid->CanFishFrom(Shore, Water);
		}
	if (!TestTrue(TEXT("A shoreline exists"), bFoundShore)) { World->DestroyWorld(false); return false; }
	TestFalse(TEXT("Cannot fish while standing in water"), Grid->CanFishFrom(Water, Water + FIntPoint(1, 0)));
	TestTrue(TEXT("Can cast from two cells away"), Grid->CanFishFrom(Shore - FIntPoint(1, 0), Water));
	TestFalse(TEXT("Cannot cast from three cells away"), Grid->CanFishFrom(Shore - FIntPoint(2, 0), Water));
	TestTrue(TEXT("One-cell diagonal cast is allowed"), Grid->CanFishFrom(Shore + FIntPoint(0, 1), Water));
	TestTrue(TEXT("Opposite diagonal cast is allowed"), Grid->CanFishFrom(Shore - FIntPoint(0, 1), Water));
	TestFalse(TEXT("Two-cell diagonal cast is rejected"), Grid->CanFishFrom(Shore + FIntPoint(-1, 2), Water));
	TestFalse(TEXT("Two by one offset is rejected"), Grid->CanFishFrom(Shore + FIntPoint(-1, 1), Water));
	TestFalse(TEXT("Land cannot be targeted"), Grid->CanFishFrom(Water, Shore));
	TestFalse(TEXT("Outside world cannot fish"), Grid->CanFishFrom(FIntPoint(1000, 1000), Water));
	TestEqual(TEXT("Water cannot be tilled"), Grid->TillCell(Water), EPSTileInteractionResult::NoEffect);
	TestEqual(TEXT("Water cannot be planted"), Grid->PlantSeed(Water), EPSTileInteractionResult::NoEffect);

	APSPlayerController* Controller = World->SpawnActorDeferred<APSPlayerController>(
		APSPlayerController::StaticClass(), FTransform::Identity);
	Controller->InventoryComponent->SaveSlotName = TEXT("FishingInventoryTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Controller->InventoryComponent->bAutoSave = false;
	Controller->FinishSpawning(FTransform::Identity);
	Controller->InventoryComponent->ResetToDefaults();
	TestEqual(TEXT("Starter fishing rod occupies hotbar slot three"), Controller->InventoryComponent->GetHotbarSlot(2).ItemType, EPSItemType::FishingRod);
	APawn* Pawn = World->SpawnActor<APawn>();
	USceneComponent* Root = NewObject<USceneComponent>(Pawn);
	Pawn->SetRootComponent(Root);
	Root->RegisterComponent();
	Controller->Possess(Pawn);
	Controller->GridWorld = Grid;
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore));
	TestFalse(TEXT("Rod cannot be used without equipping"), Controller->TryUseFishingRod(Water));
	Controller->SelectHotbarSlot(2);
	TestEqual(TEXT("Third hotbar slot equips rod"), Controller->GetEquipment(), EPSEquipment::FishingRod);
	TestTrue(TEXT("Equipped rod works on adjacent water"), Controller->TryUseFishingRod(Water));
	for (int32 Frame = 0; Frame < 180; ++Frame) Controller->UpdateFishing();
	TestTrue(TEXT("One click keeps fishing active across frames"), Controller->IsFishing());
	TestFalse(TEXT("Repeated cast does not restart fishing"), Controller->TryUseFishingRod(Water));
	TestFalse(TEXT("Cannot retarget an active cast"), Controller->TryUseFishingRod(Shore));
	TestTrue(TEXT("Invalid target does not cancel active fishing"), Controller->IsFishing());
	TestEqual(TEXT("Original fishing target is retained"), Controller->FishingCell.GetValue(), Water);
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore) + FVector(1, 0, 0));
	Controller->UpdateFishing();
	TestFalse(TEXT("Movement within the same cell stops fishing"), Controller->IsFishing());
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore - FIntPoint(1, 0)));
	TestTrue(TEXT("Controller permits two-cell cast"), Controller->TryUseFishingRod(Water));
	Controller->SelectHotbarSlot(0);
	TestFalse(TEXT("Changing equipment stops fishing"), Controller->IsFishing());
	Controller->SelectHotbarSlot(2);
	TestTrue(TEXT("Can start again after equipment change"), Controller->TryUseFishingRod(Water));
	Controller->UnPossess();
	Controller->UpdateFishing();
	TestFalse(TEXT("Losing pawn stops fishing"), Controller->IsFishing());
	Controller->Possess(Pawn);
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore - FIntPoint(2, 0)));
	TestFalse(TEXT("Controller rejects three-cell casts"), Controller->TryUseFishingRod(Water));

	// An odd chunk size forces lakes across chunk boundaries. Saving and unloading
	// one side must not change their shape or the fishing range check.
	Grid->ChunkSize = 3;
	const FIntPoint ChunkCoord = PSGrid::CellToChunk(Water, Grid->ChunkSize);
	const FPSChunkData Chunk = Grid->GetOrCreateChunk(ChunkCoord);
	FPSChunkSaveData& Saved = Grid->ModifiedChunks.Add(ChunkCoord);
	Saved.Coordinate = ChunkCoord;
	Saved.Cells = Chunk.Cells;
	Grid->SaveWorld();
	Grid->LoadWorld();
	TestTrue(TEXT("Lake survives save and reload"), Grid->CanFishFrom(Shore, Water));
	// Simulate a pre-water saved chunk: no new partial lake may grow beside it.
	for (FPSTileCell& Cell : Grid->ModifiedChunks.FindChecked(ChunkCoord).Cells) Cell.GroundType = EPSTileType::Grass;
	for (int32 DY = -4; DY <= 4; ++DY)
		for (int32 DX = -4; DX <= 4; ++DX)
			if (PSGrid::CellToChunk(Water + FIntPoint(DX, DY), 8) == PSGrid::CellToChunk(Water, 8))
				TestTrue(TEXT("Legacy saved land suppresses the whole lake"), Grid->GetGroundTile(Water + FIntPoint(DX, DY)) != EPSTileType::Water);

	APSTileChunkActor* Renderer = World->SpawnActor<APSTileChunkActor>();
	FPSChunkData RenderData;
	RenderData.Cells.SetNum(4);
	for (FPSTileCell& Cell : RenderData.Cells) Cell.GroundType = EPSTileType::Water;
	Renderer->Rebuild(RenderData, 2, 100);
	Renderer->Rebuild(RenderData, 2, 100);
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Renderer->GetComponents(Components);
	int32 RenderedWater = 0;
	for (auto* Component : Components)
		if (Component->GetFName() == TEXT("WaterInstances")) RenderedWater += Component->GetInstanceCount();
	TestEqual(TEXT("2x2 water renders without duplicate instances"), RenderedWater, 4);
	UGameplayStatics::DeleteGameInSlot(Grid->SaveSlotName, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
