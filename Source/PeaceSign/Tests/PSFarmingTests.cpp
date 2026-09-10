#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"
#include "../World/PSTileChunkActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
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
	TestEqual(TEXT("Ground and seed have renderers"), Components.Num(), 5);
	for (auto* Component : Components)
		TestEqual(TEXT("Each ground type renders one tile"), Component->GetInstanceCount(), 1);
	Renderer->Rebuild(Chunk, 2, 100.0f);
	for (auto* Component : Components)
		TestEqual(TEXT("Rebuilding does not duplicate tiles"), Component->GetInstanceCount(), 1);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
