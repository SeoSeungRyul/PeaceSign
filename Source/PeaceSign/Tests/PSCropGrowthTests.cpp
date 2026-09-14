#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"
#include "../World/PSTileChunkActor.h"
#include "../World/PSCropGrowth.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSCropGrowthTest, "PeaceSign.Farming.Growth", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSCropGrowthTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	UPSGameTimeSubsystem* Clock = World->GetSubsystem<UPSGameTimeSubsystem>();
	if (!TestNotNull(TEXT("Clock subsystem exists"), Clock)) { World->DestroyWorld(false); return false; }
	APSGridWorld* Grid = World->SpawnActor<APSGridWorld>();
	const FString Slot = TEXT("GrowthTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Grid->SaveSlotName = Slot;
	TArray<FIntPoint> Cells;
	for (int32 X = -10; X < 10 && Cells.Num() < 3; ++X)
	{
		const FIntPoint Cell(X, -1);
		if (Grid->GetGroundTile(Cell) == EPSTileType::Grass) Cells.Add(Cell);
	}
	if (!TestEqual(TEXT("Three tillable cells available"), Cells.Num(), 3)) { World->DestroyWorld(false); return false; }
	for (FIntPoint Cell : Cells) Grid->TillCell(Cell);
	Grid->PlantSeed(Cells[0]);
	Clock->Tick(14.0f);
	Grid->PlantSeed(Cells[1]);
	TestEqual(TEXT("Same half hour shares one bucket"), Grid->GrowingCrops.Num(), 1);
	TestEqual(TEXT("New crop starts with one marker"), Grid->GetCropStage(Cells[0]), 1);
	Clock->Tick(16.0f);
	TestEqual(TEXT("Half an hour is not a full stage"), Grid->GetCropStage(Cells[0]), 1);
	Grid->PlantSeed(Cells[2]);
	TestEqual(TEXT("Next half hour creates another bucket"), Grid->GrowingCrops.Num(), 2);
	Clock->Tick(1410.0f);
	TestEqual(TEXT("One game day produces stage two"), Grid->GetCropStage(Cells[0]), 2);
	TestEqual(TEXT("Same bucket grows together"), Grid->GetCropStage(Cells[1]), 2);
	TestEqual(TEXT("Later bucket is still stage one"), Grid->GetCropStage(Cells[2]), 1);
	Clock->Tick(30.0f);
	TestEqual(TEXT("Later bucket grows on :30"), Grid->GetCropStage(Cells[2]), 2);
	// Unload cached data: growth must keep updating the saved data without a renderer.
	Grid->LoadedChunks.Reset();
	Clock->Tick(1410.0f);
	TestTrue(TEXT("Growth does not load invisible chunks"), Grid->LoadedChunks.IsEmpty());
	TestEqual(TEXT("Unloaded crop reaches stage three"), Grid->GetCropStage(Cells[0]), 3);
	const FIntPoint ChunkCoord = PSGrid::CellToChunk(Cells[0], Grid->ChunkSize);
	Grid->GetOrCreateChunk(ChunkCoord);
	TestEqual(TEXT("Reloaded chunk has current stage"), Grid->GetCropStage(Cells[0]), 3);
	Clock->Tick(11.0f);
	Grid->SaveWorld();
	Clock->OnClockChanged.RemoveDynamic(Grid, &APSGridWorld::HandleClockChanged);

	UWorld* RestoredWorld = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	RestoredWorld->InitializeActorsForPlay(FURL());
	UPSGameTimeSubsystem* RestoredClock = RestoredWorld->GetSubsystem<UPSGameTimeSubsystem>();
	APSGridWorld* Restored = RestoredWorld->SpawnActor<APSGridWorld>();
	Restored->SaveSlotName = Slot;
	Restored->LoadWorld();
	TestEqual(TEXT("Saved clock restores the day"), RestoredClock->GetDay(), int64(3));
	TestEqual(TEXT("Saved clock restores the time"), RestoredClock->GetMinuteOfDay(), 360);
	TestEqual(TEXT("Saved clock restores partial-step time"), RestoredClock->GetSecondsIntoStep(), 11.0);
	TestEqual(TEXT("Reload preserves growth age"), Restored->GetCropStage(Cells[0]), 3);
	TestEqual(TEXT("Reload preserves later bucket"), Restored->GetCropStage(Cells[2]), 2);
	RestoredClock->Tick(19.0f);
	TestEqual(TEXT("Reload preserves partial-day progress"), Restored->GetCropStage(Cells[2]), 3);
	RestoredClock->Tick(2880.0f);
	for (FIntPoint Cell : Cells) TestEqual(TEXT("Four game days completes all stages"), Restored->GetCropStage(Cell), 5);
	TestTrue(TEXT("Mature crops leave growth scheduler"), Restored->GrowingCrops.IsEmpty());
	RestoredClock->Tick(1440.0f);
	TestEqual(TEXT("Mature crop stays at five"), Restored->GetCropStage(Cells[0]), 5);
	Restored->LoadWorld();
	TestEqual(TEXT("Maturity survives reload"), Restored->GetCropStage(Cells[0]), 5);
	TestEqual(TEXT("Bare hands remove a mature crop without harvesting"), Restored->RemoveCrop(Cells[0]), EPSTileInteractionResult::CropRemoved);
	TestEqual(TEXT("Removed mature crop clears the tile"), Restored->GetCropType(Cells[0]), EPSCropType::None);
	TestEqual(TEXT("Removed crop cannot be harvested"), Restored->HarvestCrop(Cells[0]), EPSTileInteractionResult::NoEffect);
	TestEqual(TEXT("Another mature crop is harvested with the hoe"), Restored->HarvestCrop(Cells[1]), EPSTileInteractionResult::Harvested);
	TestEqual(TEXT("Harvest clears the crop but keeps the tile"), Restored->GetCropType(Cells[1]), EPSCropType::None);

	TestEqual(TEXT("One full day grows one stage"), PSCropGrowth::GetStage(47, 95), uint8(2));
	TestEqual(TEXT("Four days reaches stage five"), PSCropGrowth::GetStage(47, 239), uint8(5));
	TestEqual(TEXT("One half hour short remains at four"), PSCropGrowth::GetStage(47, 238), uint8(4));

	APSTileChunkActor* Renderer = RestoredWorld->SpawnActor<APSTileChunkActor>();
	FPSChunkData RenderData;
	RenderData.Cells.SetNum(1);
	RenderData.Cells[0].GroundType = EPSTileType::TilledSoil;
	RenderData.Cells[0].CropType = EPSCropType::TestCrop;
	TArray<UHierarchicalInstancedStaticMeshComponent*> Instances;
	Renderer->GetComponents(Instances);
	for (int32 Stage = 1; Stage <= 5; ++Stage)
	{
		RenderData.Cells[0].GrowthStage = static_cast<uint8>(Stage);
		Renderer->Rebuild(RenderData, 1, 100.0f);
		for (auto* Component : Instances)
			if (Component->GetFName() == TEXT("SeedInstances"))
				TestEqual(TEXT("Marker count matches growth stage"), Component->GetInstanceCount(), Stage);
	}
	Renderer->Rebuild(RenderData, 1, 100.0f);
	for (auto* Component : Instances)
		if (Component->GetFName() == TEXT("SeedInstances"))
			TestEqual(TEXT("Rebuild does not duplicate mature markers"), Component->GetInstanceCount(), 5);

	// Old saves have neither clock reference nor planting timestamps.
	UPSWorldSaveGame* Legacy = NewObject<UPSWorldSaveGame>();
	FPSChunkSaveData LegacyChunk;
	LegacyChunk.Coordinate = FIntPoint::ZeroValue;
	LegacyChunk.Cells.SetNum(16 * 16);
	LegacyChunk.Cells[0].GroundType = EPSTileType::TilledSoil;
	LegacyChunk.Cells[0].CropType = EPSCropType::TestCrop;
	Legacy->ModifiedChunks.Add(LegacyChunk);
	UGameplayStatics::SaveGameToSlot(Legacy, Slot, 0);
	Restored->LoadWorld();
	TestEqual(TEXT("Legacy seed starts at one"), Restored->GetCropStage(FIntPoint::ZeroValue), 1);
	RestoredClock->Tick(1440.0f);
	TestEqual(TEXT("Legacy seed can continue growing"), Restored->GetCropStage(FIntPoint::ZeroValue), 2);
	Legacy->GrowthClockHalfHour = 20;
	Legacy->ModifiedChunks[0].Cells[0].PlantedHalfHour = 16;
	Legacy->ModifiedChunks[0].Cells[0].GrowthStage = 3;
	UGameplayStatics::SaveGameToSlot(Legacy, Slot, 0);
	Restored->LoadWorld();
	TestEqual(TEXT("Older interval save does not move a crop backwards"), Restored->GetCropStage(FIntPoint::ZeroValue), 3);
	Restored->ResetWorld();
	TestTrue(TEXT("Reset clears growth buckets"), Restored->GrowingCrops.IsEmpty());
	RestoredClock->Tick(30.0f);
	TestEqual(TEXT("Reset crop does not return"), Restored->GetCropStage(FIntPoint::ZeroValue), 0);
	const int64 BeforeSkip = RestoredClock->GetHalfHourIndex();
	RestoredClock->AdvanceGameHours(6);
	TestEqual(TEXT("Six-hour shortcut advances the subsystem clock"), RestoredClock->GetHalfHourIndex(), BeforeSkip + 12);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	RestoredWorld->DestroyWorld(false);
	World->DestroyWorld(false);
	return true;
}
#endif
