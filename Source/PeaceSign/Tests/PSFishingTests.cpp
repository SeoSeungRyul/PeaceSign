#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSGridWorld.h"
#include "../World/PSTileChunkActor.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSWorldItemActor.h"
#include "../Skills/PSPlayerSkillComponent.h"
#include "../Fishing/PSFishingJournalComponent.h"
#include "../UI/PSFishingWidget.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSFishingTest, "PeaceSign.Fishing.LakesAndRod", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSFishingTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
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
	if (!TestTrue(TEXT("A shoreline exists"), bFoundShore))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}
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
	TestEqual(TEXT("Procedural water is a lake fishing location"), Grid->GetFishingLocationId(Water), 2);
	TestEqual(TEXT("Land has no fishing location"), Grid->GetFishingLocationId(Shore), INDEX_NONE);

	UDataTable* ImportedFishData = NewObject<UDataTable>(World);
	ImportedFishData->RowStruct = FPSFishDefinition::StaticStruct();
	const FString FishCsv = TEXT("---,Name,Difficulty,Season,Location,MinSize,MaxSize,Description,DescriptionPlus,IconID,Weight\n")
		TEXT("010201,붕어,1,0,\"(1,2)\",10,40,기본 설명,최대 크기 설명,070201,1\n")
		TEXT("010202,여름물고기,1,1,\"(2)\",10,40,여름 설명,,070201,1000\n")
		TEXT("010203,바다물고기,1,0,\"(0)\",10,40,바다 설명,,070201,1000\n");
	const TArray<FString> FishImportProblems = ImportedFishData->CreateTableFromCSVString(FishCsv);
	TestTrue(TEXT("Fishing CSV imports without schema errors"), FishImportProblems.IsEmpty());
	const FPSFishDefinition* ImportedFish = ImportedFishData->FindRow<FPSFishDefinition>(TEXT("010201"), TEXT("Test"));
	TestNotNull(TEXT("Imported fishing row is addressable by ID"), ImportedFish);
	if (ImportedFish)
	{
		TestEqual(TEXT("CSV imports the fish name"), ImportedFish->Name.ToString(), FString(TEXT("붕어")));
		TestEqual(TEXT("CSV imports both fishing locations"), ImportedFish->Location.Num(), 2);
		TestEqual(TEXT("CSV imports the icon relationship"), ImportedFish->IconID, FName(TEXT("070201")));
	}
	UDataTable* ImportedIconData = NewObject<UDataTable>(World);
	ImportedIconData->RowStruct = FPSIconDefinition::StaticStruct();
	const TArray<FString> IconImportProblems = ImportedIconData->CreateTableFromCSVString(
		TEXT("---,Name,Image\n070201,더미물고기,Texture2D'/Game/UI/Fishing/T_FishingUIAtlas.T_FishingUIAtlas'\n"));
	TestTrue(TEXT("Icon CSV imports without schema errors"), IconImportProblems.IsEmpty());
	const FPSIconDefinition* ImportedIcon = ImportedIconData->FindRow<FPSIconDefinition>(TEXT("070201"), TEXT("Test"));
	TestNotNull(TEXT("Imported icon row is addressable by IconID"), ImportedIcon);
	if (ImportedIcon) TestNotNull(TEXT("Imported soft texture path resolves"), ImportedIcon->Image.LoadSynchronous());

	APSPlayerController* Controller = World->SpawnActorDeferred<APSPlayerController>(
		APSPlayerController::StaticClass(), FTransform::Identity);
	Controller->InventoryComponent->SaveSlotName = TEXT("FishingInventoryTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Controller->InventoryComponent->bAutoSave = false;
	Controller->FishDataTable = ImportedFishData;
	Controller->FishIconDataTable = ImportedIconData;
	Controller->SkillComponent->SaveSlotName = TEXT("FishingSkillTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Controller->SkillComponent->bAutoSave = false;
	Controller->FishingJournalComponent->SaveSlotName = TEXT("FishingJournalTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Controller->FishingJournalComponent->bAutoSave = false;
	Controller->FinishSpawning(FTransform::Identity);
	Controller->InventoryComponent->ResetToDefaults();
	Controller->SkillComponent->ResetSkills();
	FName FilteredFishId;
	Controller->SelectFishDefinition(FilteredFishId, 1, 2);
	TestEqual(TEXT("Summer lake filter selects the matching row"), FilteredFishId, FName(TEXT("010202")));
	Controller->SelectFishDefinition(FilteredFishId, 0, 0);
	TestEqual(TEXT("Spring sea filter selects the matching row"), FilteredFishId, FName(TEXT("010203")));
	Controller->SelectFishDefinition(FilteredFishId, 3, 2);
	TestTrue(TEXT("No matching season leaves the fish ID empty"), FilteredFishId.IsNone());
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
	TestEqual(TEXT("Selected fish preserves its CSV row ID"), Controller->GetCurrentFishId(), FName(TEXT("010201")));
	TestEqual(TEXT("Selection filters out wrong seasons and locations"), Controller->CurrentFish.Name.ToString(), FString(TEXT("붕어")));
	TestNotNull(TEXT("Selected fish resolves its related icon row"), Controller->ResolveFishIcon(Controller->CurrentFish.IconID));
	TestEqual(TEXT("Cast enters bite-wait state"), Controller->GetFishingState(), EPSFishingState::WaitingForBite);
	for (int32 Frame = 0; Frame < 180; ++Frame) Controller->UpdateFishing(0.0f);
	TestTrue(TEXT("Fishing stays active without elapsed time"), Controller->IsFishing());
	TestFalse(TEXT("Repeated cast does not restart fishing"), Controller->TryUseFishingRod(Water));
	TestFalse(TEXT("Cannot retarget an active cast"), Controller->TryUseFishingRod(Shore));
	TestTrue(TEXT("Invalid target does not cancel active fishing"), Controller->IsFishing());
	TestEqual(TEXT("Original fishing target is retained"), Controller->FishingCell.GetValue(), Water);
	Controller->HandleFishingRod();
	TestFalse(TEXT("Use input cancels while waiting for a bite"), Controller->IsFishing());
	TestTrue(TEXT("Can cast again after cancellation"), Controller->TryUseFishingRod(Water));
	Controller->FishingStateTimeRemaining = 0.01f;
	Controller->UpdateFishing(0.02f);
	TestEqual(TEXT("Bite delay enters the five-second bite window"), Controller->GetFishingState(), EPSFishingState::BiteWindow);
	TestEqual(TEXT("Bite window duration follows the design"), Controller->FishingStateDuration, 5.0f);
	Controller->HandleFishingRod();
	TestEqual(TEXT("Fresh use input starts the minigame"), Controller->GetFishingState(), EPSFishingState::Minigame);
	const FIntPoint InputRange = PSFishing::GetInputCountRange(Controller->CurrentFish.Difficulty);
	TestTrue(TEXT("Difficulty controls sequence length"), Controller->FishingSequence.Num() >= InputRange.X && Controller->FishingSequence.Num() <= InputRange.Y);
	const EPSFishingDirection Correct = Controller->FishingSequence[0];
	const EPSFishingDirection Wrong = static_cast<EPSFishingDirection>((static_cast<int32>(Correct) + 1) % 4);
	const float TimeBeforeWrongInput = Controller->FishingStateTimeRemaining;
	TestFalse(TEXT("Wrong direction does not progress"), Controller->SubmitFishingDirection(Wrong));
	TestEqual(TEXT("Wrong direction retains current answer"), Controller->FishingSequenceIndex, 0);
	TestEqual(TEXT("Wrong direction removes half a second"), Controller->FishingStateTimeRemaining, TimeBeforeWrongInput - 0.5f);
	const TArray<EPSFishingDirection> Answers = Controller->FishingSequence;
	for (const EPSFishingDirection Answer : Answers) TestTrue(TEXT("Correct direction advances"), Controller->SubmitFishingDirection(Answer));
	TestEqual(TEXT("Completing all directions succeeds"), Controller->GetFishingState(), EPSFishingState::Success);
	TestNotNull(TEXT("Successful catch caches the related icon"), Controller->GetCurrentFishIcon());
	TestEqual(TEXT("Success immediately awards one fish"), Controller->InventoryComponent->CountItem(EPSItemType::Fish), 1);
	bool bCaughtFishKeepsRowId = false;
	for (int32 Index = 0; Index < Controller->InventoryComponent->GetUnlockedBagSlotCount(); ++Index)
	{
		const FPSItemStack FishStack = Controller->InventoryComponent->GetBagSlot(Index);
		bCaughtFishKeepsRowId |= FishStack.ItemType == EPSItemType::Fish && FishStack.ItemId == TEXT("010201");
	}
	TestTrue(TEXT("Fishing reward preserves its DataTable row ID"), bCaughtFishKeepsRowId);
	FPSItemStack CaughtFish;
	for (int32 Index = 0; Index < Controller->InventoryComponent->GetUnlockedBagSlotCount(); ++Index)
	{
		const FPSItemStack Candidate = Controller->InventoryComponent->GetBagSlot(Index);
		if (Candidate.ItemType == EPSItemType::Fish) { CaughtFish = Candidate; break; }
	}
	TestEqual(TEXT("Inventory resolves fish-specific names"), Controller->GetItemDisplayName(CaughtFish).ToString(), FString(TEXT("붕어")));
	TestTrue(TEXT("Inventory resolves both fish descriptions"), Controller->GetItemDescription(CaughtFish).ToString().Contains(TEXT("최대 크기 설명")));
	TestNotNull(TEXT("Inventory resolves the same icon table row"), Controller->GetItemIcon(CaughtFish));
	const FPSFishJournalRecord FirstRecord = Controller->FishingJournalComponent->GetRecord(TEXT("010201"));
	TestEqual(TEXT("Successful catch records one fish in the journal"), FirstRecord.TimesCaught, 1);
	TestTrue(TEXT("Successful catch records its generated size"), FirstRecord.LargestSizeCm >= 10 && FirstRecord.LargestSizeCm <= 40);
	Controller->FishingJournalComponent->RecordCatch(TEXT("010201"), 55, 2);
	TestEqual(TEXT("Journal accumulates catch quantity"), Controller->FishingJournalComponent->GetRecord(TEXT("010201")).TimesCaught, 3);
	TestEqual(TEXT("Journal keeps the largest size"), Controller->FishingJournalComponent->GetRecord(TEXT("010201")).LargestSizeCm, 55);
	TestTrue(TEXT("Fishing journal saves"), Controller->FishingJournalComponent->SaveJournal());
	UPSFishingJournalComponent* LoadedJournal = NewObject<UPSFishingJournalComponent>(Controller);
	LoadedJournal->SaveSlotName = Controller->FishingJournalComponent->SaveSlotName;
	LoadedJournal->bAutoSave = false;
	TestTrue(TEXT("Fishing journal loads"), LoadedJournal->LoadJournal());
	TestEqual(TEXT("Loaded journal restores the largest size"), LoadedJournal->GetRecord(TEXT("010201")).LargestSizeCm, 55);
	TestEqual(TEXT("Successful catch awards two fishing XP"),
		Controller->SkillComponent->GetSkillState(EPSPlayerSkillField::Fishing).Experience, 2);
	Controller->UpdateFishing(2.0f);
	TestFalse(TEXT("Result closes after its display time"), Controller->IsFishing());
	TestTrue(TEXT("Bait can be added"), Controller->InventoryComponent->AddItem(EPSItemType::FishingBait, 1));
	TestTrue(TEXT("Bobber can be added"), Controller->InventoryComponent->AddItem(EPSItemType::FishingBobber, 1));
	TestTrue(TEXT("Baited cast starts"), Controller->TryUseFishingRod(Water));
	TestEqual(TEXT("Bait fixes bite wait to two seconds"), Controller->FishingStateDuration, 2.0f);
	TestEqual(TEXT("One bait is consumed on cast"), Controller->InventoryComponent->CountItem(EPSItemType::FishingBait), 0);
	Controller->FishingStateTimeRemaining = 0.01f;
	Controller->UpdateFishing(0.02f);
	TestEqual(TEXT("Owned bobber automatically starts the minigame"), Controller->GetFishingState(), EPSFishingState::Minigame);
	int32 BobberDurability = INDEX_NONE;
	for (int32 Index = 0; Index < Controller->InventoryComponent->GetUnlockedBagSlotCount(); ++Index)
	{
		const FPSItemStack Bobber = Controller->InventoryComponent->GetBagSlot(Index);
		if (Bobber.ItemType == EPSItemType::FishingBobber) BobberDurability = Bobber.CurrentDurability;
	}
	TestEqual(TEXT("Automatic hook consumes one bobber durability"), BobberDurability, 99);
	Controller->StopFishing();
	TestTrue(TEXT("Bobber fixture is removable"), Controller->InventoryComponent->RemoveItem(EPSItemType::FishingBobber, 1));

	TestTrue(TEXT("Can start another cast"), Controller->TryUseFishingRod(Water));
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore) + FVector(1, 0, 0));
	Controller->UpdateFishing(0.0f);
	TestFalse(TEXT("External movement cancels the fixed fishing position"), Controller->IsFishing());
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore - FIntPoint(1, 0)));
	TestTrue(TEXT("Controller permits two-cell cast"), Controller->TryUseFishingRod(Water));
	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("Hotbar changes are blocked during fishing"), Controller->GetEquipment(), EPSEquipment::FishingRod);
	TestTrue(TEXT("Blocked hotbar change leaves fishing active"), Controller->IsFishing());
	Controller->StopFishing();
	TestTrue(TEXT("Can cast after explicit cancellation"), Controller->TryUseFishingRod(Water));
	Controller->UnPossess();
	Controller->UpdateFishing(0.0f);
	TestFalse(TEXT("Losing pawn stops fishing"), Controller->IsFishing());
	Controller->Possess(Pawn);
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore - FIntPoint(2, 0)));
	TestFalse(TEXT("Controller rejects three-cell casts"), Controller->TryUseFishingRod(Water));
	APSPlayerCharacter* Character = World->SpawnActor<APSPlayerCharacter>();
	Controller->Possess(Character);
	Character->SetActorLocation(Grid->CellToWorldCenter(Shore));
	TestTrue(TEXT("Character can begin fishing"), Controller->TryUseFishingRod(Water));
	TestTrue(TEXT("Fishing locks character movement"), Character->IsMovementLocked());
	TestTrue(TEXT("Movement input cancels bite waiting"), Controller->CancelFishingForMovementInput());
	TestFalse(TEXT("Movement cancellation unlocks the character"), Character->IsMovementLocked());
	TestTrue(TEXT("Character can cast again after moving"), Controller->TryUseFishingRod(Water));
	Controller->BeginFishingBite();
	Controller->BeginFishingMinigame();
	TestFalse(TEXT("Movement cancellation does not consume minigame direction input"), Controller->CancelFishingForMovementInput());
	Controller->FishingSequence = {EPSFishingDirection::Up};
	Controller->FishingSequenceIndex = 0;
	Controller->SubmitFishingDirection(EPSFishingDirection::Up);
	TestTrue(TEXT("Success keeps movement locked during result motion"), Character->IsMovementLocked());
	Controller->UpdateFishing(2.0f);
	TestFalse(TEXT("Success result completion unlocks movement"), Character->IsMovementLocked());
	TestTrue(TEXT("Character can cast after success"), Controller->TryUseFishingRod(Water));
	Controller->BeginFishingBite();
	Controller->CompleteFishing(false);
	TestFalse(TEXT("Failure unlocks movement immediately"), Character->IsMovementLocked());
	Controller->UpdateFishing(2.0f);

	const int32 ExistingFish = Controller->InventoryComponent->CountItem(EPSItemType::Fish);
	TestTrue(TEXT("Fish stack can be filled"), Controller->InventoryComponent->AddItemVariant(
		EPSItemType::Fish, Controller->GetCurrentFishId(), 999 - ExistingFish));
	for (int32 Variant = 0; Variant < 9; ++Variant)
		TestTrue(TEXT("Remaining bag slots can be filled"), Controller->InventoryComponent->AddItem(EPSItemType::TestSeed, 999, Variant));
	int32 DropsBefore = 0;
	for (TActorIterator<APSWorldItemActor> It(World); It; ++It) ++DropsBefore;
	TestTrue(TEXT("Can cast with a full bag"), Controller->TryUseFishingRod(Water));
	Controller->BeginFishingBite();
	Controller->BeginFishingMinigame();
	Controller->FishingSequence = {EPSFishingDirection::Right};
	Controller->FishingSequenceIndex = 0;
	Controller->SubmitFishingDirection(EPSFishingDirection::Right);
	int32 DropsAfter = 0;
	for (TActorIterator<APSWorldItemActor> It(World); It; ++It) ++DropsAfter;
	TestEqual(TEXT("Full inventory drops the fishing reward"), DropsAfter, DropsBefore + 1);
	TestEqual(TEXT("Failed inventory insert does not exceed the stack limit"), Controller->InventoryComponent->CountItem(EPSItemType::Fish), 999);
	Controller->UpdateFishing(2.0f);

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
	UPSFishingWidget* FishingUI = CreateWidget<UPSFishingWidget>(World);
	TestNotNull(TEXT("Fishing UI creates"), FishingUI);
	if (FishingUI)
	{
		// Force the Slate tree to initialize, as AddToViewport does in normal play.
		FishingUI->TakeWidget();
		FishingUI->Refresh(EPSFishingState::Minigame, 8.0f, 10.0f,
			{EPSFishingDirection::Up, EPSFishingDirection::Left, EPSFishingDirection::Down}, 0,
			FText::FromString(TEXT("붕어")), 18, nullptr);
		TestEqual(TEXT("Active fishing UI is visible"), FishingUI->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("Fishing UI has six direction slots"), FishingUI->DirectionLabels.Num(), 6);
		TestNotNull(TEXT("Fishing arrow atlas is loaded"), FishingUI->DirectionAtlas.Get());
		TestEqual(TEXT("Current fishing arrow image is shown in slot three"), FishingUI->DirectionImages[2]->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestNotNull(TEXT("Current arrow uses the atlas texture"), FishingUI->DirectionImages[2]->GetBrush().GetResourceObject());
		FishingUI->Refresh(EPSFishingState::Idle, 0, 0, {}, 0, FText::GetEmpty(), 0, nullptr);
		TestEqual(TEXT("Idle fishing UI is hidden"), FishingUI->GetVisibility(), ESlateVisibility::Collapsed);
	}
	World->DestroyActor(Renderer);
	UGameplayStatics::DeleteGameInSlot(Grid->SaveSlotName, 0);
	UGameplayStatics::DeleteGameInSlot(Controller->FishingJournalComponent->SaveSlotName, 0);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
