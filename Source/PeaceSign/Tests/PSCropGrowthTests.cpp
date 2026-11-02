#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"
#include "../World/PSTileChunkActor.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSCropGrowthTest, "PeaceSign.Farming.Growth", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSCropGrowthTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	UPSGameTimeSubsystem* Clock = World->GetSubsystem<UPSGameTimeSubsystem>();
	if (!TestNotNull(TEXT("Clock exists"), Clock)) { World->DestroyWorld(false); return false; }
	Clock->RestoreClock(12, 0);
	const FString SaveSlot = TEXT("WateringTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	APSGridWorld* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = SaveSlot;
	Grid->FinishSpawning(FTransform::Identity);
	Grid->EnsureGrowthUpdates();
	TArray<FIntPoint> Cells;
	for (int32 X = -10; X < 10 && Cells.Num() < 3; ++X)
	{
		const FIntPoint Cell(X, -1);
		if (Grid->TillCell(Cell) == EPSTileInteractionResult::Tilled) Cells.Add(Cell);
	}
	if (!TestEqual(TEXT("Three tillable cells"), Cells.Num(), 3)) { World->DestroyWorld(false); return false; }
	TestEqual(TEXT("Watering outside world is rejected"), Grid->WaterCell(FIntPoint(MAX_int32, 0)), EPSTileInteractionResult::InvalidCell);
	TestEqual(TEXT("Empty farmland can be watered"), Grid->WaterCell(Cells[2]), EPSTileInteractionResult::Watered);
	TestEqual(TEXT("Repeat watering has no effect"), Grid->WaterCell(Cells[2]), EPSTileInteractionResult::NoEffect);
	Grid->PlantSeed(Cells[0]);
	Grid->PlantSeed(Cells[1]);
	Grid->WaterCell(Cells[0]);
	Clock->AdvanceGameHours(23);
	TestTrue(TEXT("Wet soil persists through midnight"), Grid->IsCellWatered(Cells[0]));
	TestEqual(TEXT("No growth before dawn"), Grid->GetCropStage(Cells[0]), 1);
	Clock->AdvanceGameHours(1);
	TestEqual(TEXT("Watered crop grows at six"), Grid->GetCropStage(Cells[0]), 2);
	TestEqual(TEXT("Dry crop pauses"), Grid->GetCropStage(Cells[1]), 1);
	TestFalse(TEXT("Empty soil dries at six"), Grid->IsCellWatered(Cells[2]));
	TestFalse(TEXT("Crop soil dries after growth"), Grid->IsCellWatered(Cells[0]));
	Grid->WaterCell(Cells[0]);
	Grid->LoadedChunks.Reset();
	Clock->AdvanceGameDays(1);
	TestTrue(TEXT("Daily growth does not load invisible chunks"), Grid->LoadedChunks.IsEmpty());
	TestEqual(TEXT("Watered unloaded crop grows"), Grid->GetCropStage(Cells[0]), 3);
	TestEqual(TEXT("Second dry dawn kills crop"), Grid->GetCropType(Cells[1]), EPSCropType::None);
	TestEqual(TEXT("Dead crop leaves tilled soil"), Grid->GetGroundTile(Cells[1]), EPSTileType::TilledSoil);
	TestEqual(TEXT("Dead crop cannot be harvested"), Grid->HarvestCrop(Cells[1]), EPSTileInteractionResult::NoEffect);
	for (int32 Day = 0; Day < 2; ++Day) { Grid->WaterCell(Cells[0]); Clock->AdvanceGameDays(1); }
	TestEqual(TEXT("Four watered days reaches maturity"), Grid->GetCropStage(Cells[0]), 5);
	TestEqual(TEXT("Mature harvest succeeds"), Grid->HarvestCrop(Cells[0]), EPSTileInteractionResult::Harvested);
	Grid->PlantSeed(Cells[0]);
	Clock->AdvanceGameDays(1);
	Grid->WaterCell(Cells[0]);
	Grid->SaveWorld();
	Grid->LoadWorld();
	TestTrue(TEXT("Save restores moisture"), Grid->IsCellWatered(Cells[0]));
	Clock->AdvanceGameDays(1);
	TestEqual(TEXT("Water resets dry streak and permits growth"), Grid->GetCropStage(Cells[0]), 2);
	Grid->SaveWorld();
	Grid->LoadWorld();
	Clock->AdvanceGameDays(1);
	TestEqual(TEXT("First dry dawn after reload preserves crop"), Grid->GetCropStage(Cells[0]), 2);
	Grid->SaveWorld();
	Grid->LoadWorld();
	Clock->AdvanceGameHours(6);
	TestEqual(TEXT("Six-hour skip without dawn preserves stage"), Grid->GetCropStage(Cells[0]), 2);
	Clock->AdvanceGameHours(18);
	TestEqual(TEXT("Saved dry streak kills crop on second dawn"), Grid->GetCropType(Cells[0]), EPSCropType::None);
	Grid->PlantSeed(Cells[0]);
	Grid->WaterCell(Cells[0]);
	Clock->AdvanceGameDays(28);
	TestEqual(TEXT("Long skip processes wet first day and dry days"), Grid->GetCropType(Cells[0]), EPSCropType::None);
	TestFalse(TEXT("Long skip clears moisture"), Grid->IsCellWatered(Cells[0]));
	TestTrue(TEXT("Dead crops leave scheduler"), Grid->GrowingCrops.IsEmpty());
	APSTileChunkActor* Renderer = World->SpawnActor<APSTileChunkActor>();
	FPSChunkData RenderData;
	RenderData.Cells.SetNum(4);
	RenderData.Cells[0].GroundType = EPSTileType::TilledSoil;
	RenderData.Cells[1].GroundType = EPSTileType::TilledSoil;
	RenderData.Cells[0].Variant = 5;
	RenderData.Cells[1].Variant = 5;
	RenderData.Cells[0].bWatered = true;
	RenderData.Cells[0].CropType = EPSCropType::TestCrop;
	RenderData.Cells[0].GrowthStage = 5;
	Renderer->Rebuild(RenderData, 2, 100.0f);
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Renderer->GetComponents(Components);
	for (auto* Component : Components)
	{
		if (Component->GetFName() == TEXT("WetSoilInstances")) TestEqual(TEXT("Wet soil render instance"), Component->GetInstanceCount(), 1);
		if (Component->GetFName() == TEXT("FarmVariant_5_4")) TestEqual(TEXT("Wet soil retains base farm artwork"), Component->GetInstanceCount(), 2);
		if (Component->GetFName() == TEXT("SeedInstances")) TestEqual(TEXT("Mature crop renders five markers"), Component->GetInstanceCount(), 5);
	}
	UPSWorldSaveGame* Legacy = NewObject<UPSWorldSaveGame>();
	FPSChunkSaveData LegacyChunk;
	LegacyChunk.Coordinate = FIntPoint::ZeroValue;
	LegacyChunk.Cells.SetNum(256);
	LegacyChunk.Cells[0].GroundType = EPSTileType::TilledSoil;
	LegacyChunk.Cells[0].CropType = EPSCropType::TestCrop;
	LegacyChunk.Cells[0].GrowthStage = 3;
	Legacy->ModifiedChunks.Add(LegacyChunk);
	UGameplayStatics::SaveGameToSlot(Legacy, SaveSlot, 0);
	Grid->LoadWorld();
	TestEqual(TEXT("Old save preserves stage"), Grid->GetCropStage(FIntPoint::ZeroValue), 3);
	TestTrue(TEXT("Old crop receives one watered day"), Grid->IsCellWatered(FIntPoint::ZeroValue));
	Clock->AdvanceGameDays(1);
	TestEqual(TEXT("Migrated crop continues growing"), Grid->GetCropStage(FIntPoint::ZeroValue), 4);
	Grid->ResetWorld();
	TestTrue(TEXT("Reset clears scheduler"), Grid->GrowingCrops.IsEmpty());
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
	return true;
}
#endif
