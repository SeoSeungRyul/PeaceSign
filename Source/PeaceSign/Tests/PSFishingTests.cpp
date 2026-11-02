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
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "SceneView.h"
#include "Math/InverseRotationMatrix.h"
#include "GameFramework/SpringArmComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "RenderingThread.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"

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
		TEXT("---,Name,Image\n070201,더미물고기,Texture2D'/Game/Art/Fish/Icon_DummyFish.Icon_DummyFish'\n"));
	TestTrue(TEXT("Icon CSV imports without schema errors"), IconImportProblems.IsEmpty());
	const FPSIconDefinition* ImportedIcon = ImportedIconData->FindRow<FPSIconDefinition>(TEXT("070201"), TEXT("Test"));
	TestNotNull(TEXT("Imported icon row is addressable by IconID"), ImportedIcon);
	if (ImportedIcon) TestNotNull(TEXT("Imported soft texture path resolves"), ImportedIcon->Image.LoadSynchronous());

	UClass* ControllerClass = LoadClass<APSPlayerController>(nullptr, TEXT("/Game/Blueprints/BP_PlayerController.BP_PlayerController_C"));
	APSPlayerController* Controller = World->SpawnActorDeferred<APSPlayerController>(
		ControllerClass ? ControllerClass : APSPlayerController::StaticClass(), FTransform::Identity);
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
	TestEqual(TEXT("Starter fishing rod occupies hotbar slot four"), Controller->InventoryComponent->GetHotbarSlot(2).ItemType, EPSItemType::FishingRod);
	APawn* Pawn = World->SpawnActor<APawn>();
	USceneComponent* Root = NewObject<USceneComponent>(Pawn);
	Pawn->SetRootComponent(Root);
	Root->RegisterComponent();
	Controller->Possess(Pawn);
	Controller->GridWorld = Grid;
	const auto StartCast = [&](const FIntPoint Target)
	{
		if (!Controller->TryUseFishingRod(Target)) return false;
		Controller->UpdateFishing(Controller->CastWindupDuration);
		if (!Controller->ConfirmFishingCast()) return false;
		const float BeforeFlight = Controller->FishingStateTimeRemaining;
		Controller->UpdateFishing(Controller->CastFlightSeconds);
		TestTrue(TEXT("Bite wait does not elapse during bobber flight"), FMath::IsNearlyEqual(Controller->FishingStateTimeRemaining, BeforeFlight));
		return true;
	};
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore));
	TestFalse(TEXT("Rod cannot be used without equipping"), StartCast(Water));
	Controller->SelectHotbarSlot(2);
	TestEqual(TEXT("Third hotbar slot equips rod"), Controller->GetEquipment(), EPSEquipment::FishingRod);
	const int32 DurabilityBeforePreparing = Controller->InventoryComponent->GetHotbarSlot(2).CurrentDurability;
	TestTrue(TEXT("Use starts the casting animation"), Controller->TryUseFishingRod(Water));
	TestEqual(TEXT("Cast starts in windup"), Controller->GetFishingState(), EPSFishingState::CastWindup);
	TestFalse(TEXT("Windup cannot be confirmed early"), Controller->ConfirmFishingCast());
	TestTrue(TEXT("Fish is not selected before throw"), Controller->GetCurrentFishId().IsNone());
	Controller->UpdateFishing(Controller->CastWindupDuration);
	TestEqual(TEXT("Animation ends in timing minigame"), Controller->GetFishingState(), EPSFishingState::CastTiming);
	Controller->UpdateFishing(0.35f);
	TestTrue(TEXT("Marker reaches the horizontal centre after 0.35 seconds"), FMath::IsNearlyEqual(Controller->GetFishingCastPosition(), 0.5f, .0001f));
	Controller->HandleInteract();
	TestEqual(TEXT("F does not confirm the casting gauge"), Controller->GetFishingState(), EPSFishingState::CastTiming);
	Controller->UpdateFishing(1.4f);
	TestTrue(TEXT("Timing gauge keeps moving without confirmation"), FMath::IsNearlyEqual(Controller->GetFishingCastPosition(), 0.5f, .0001f));
	Controller->StopFishing();
	TestEqual(TEXT("Cancelling before throw preserves rod durability"), Controller->InventoryComponent->GetHotbarSlot(2).CurrentDurability, DurabilityBeforePreparing);

	TestTrue(TEXT("Equipped rod works on adjacent water"), StartCast(Water));
	TestEqual(TEXT("Selected fish preserves its CSV row ID"), Controller->GetCurrentFishId(), FName(TEXT("010201")));
	TestEqual(TEXT("Selection filters out wrong seasons and locations"), Controller->CurrentFish.Name.ToString(), FString(TEXT("붕어")));
	TestNotNull(TEXT("Selected fish resolves its related icon row"), Controller->ResolveFishIcon(Controller->CurrentFish.IconID));
	TestEqual(TEXT("Cast enters bite-wait state"), Controller->GetFishingState(), EPSFishingState::WaitingForBite);
	const FIntPoint OriginalLanding = Controller->FishingCell.GetValue();
	for (int32 Frame = 0; Frame < 180; ++Frame) Controller->UpdateFishing(0.0f);
	TestTrue(TEXT("Fishing stays active without elapsed time"), Controller->IsFishing());
	TestFalse(TEXT("Repeated cast does not restart fishing"), StartCast(Water));
	TestFalse(TEXT("Cannot retarget an active cast"), StartCast(Shore));
	TestTrue(TEXT("Invalid target does not cancel active fishing"), Controller->IsFishing());
	TestEqual(TEXT("Original fishing target is retained"), Controller->FishingCell.GetValue(), OriginalLanding);
	Controller->HandleFishingRod();
	Controller->UpdateFishing(.4f);
	TestFalse(TEXT("Use input cancels while waiting for a bite"), Controller->IsFishing());
	TestTrue(TEXT("Can cast again after cancellation"), StartCast(Water));
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
	TestEqual(TEXT("Special description follows maximum-size discovery"), Controller->GetItemDescription(CaughtFish).ToString().Contains(TEXT("최대 크기 설명")), Controller->HasMasteredFish(CaughtFish.ItemId));
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
	TestEqual(TEXT("Success result stays open after two seconds"), Controller->GetFishingState(), EPSFishingState::Success);
	Controller->UpdateFishing(.99f);
	TestTrue(TEXT("Success result remains until three seconds"), Controller->IsFishing());
	Controller->UpdateFishing(.02f);
	TestFalse(TEXT("Success result closes after three seconds"), Controller->IsFishing());
	TestTrue(TEXT("Bait can be added"), Controller->InventoryComponent->AddItem(EPSItemType::FishingBait, 1));
	TestTrue(TEXT("Bobber can be added"), Controller->InventoryComponent->AddItem(EPSItemType::FishingBobber, 1));
	TestTrue(TEXT("Baited cast starts"), StartCast(Water));
	TestTrue(TEXT("Bait reduces the maximum wait by five percent"), Controller->FishingStateDuration >= 1.0f && Controller->FishingStateDuration <= 9.5f);
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

	TestTrue(TEXT("Can start another cast"), StartCast(Water));
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore) + FVector(1, 0, 0));
	Controller->UpdateFishing(0.0f);
	TestFalse(TEXT("External movement cancels the fixed fishing position"), Controller->IsFishing());
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore - FIntPoint(1, 0)));
	TestTrue(TEXT("Controller permits two-cell cast"), StartCast(Water));
	Controller->SelectHotbarSlot(0);
	TestEqual(TEXT("Hotbar changes are blocked during fishing"), Controller->GetEquipment(), EPSEquipment::FishingRod);
	TestTrue(TEXT("Blocked hotbar change leaves fishing active"), Controller->IsFishing());
	Controller->StopFishing();
	TestTrue(TEXT("Can cast after explicit cancellation"), StartCast(Water));
	Controller->UnPossess();
	Controller->UpdateFishing(0.0f);
	TestFalse(TEXT("Losing pawn stops fishing"), Controller->IsFishing());
	Controller->Possess(Pawn);
	Pawn->SetActorLocation(Grid->CellToWorldCenter(Shore - FIntPoint(2, 0)));
	TestFalse(TEXT("Controller rejects three-cell casts"), StartCast(Water));
	UClass* CharacterClass = LoadClass<APSPlayerCharacter>(nullptr, TEXT("/Game/Blueprints/BP_PSPlayerCharacter.BP_PSPlayerCharacter_C"));
	TestNotNull(TEXT("The gameplay character Blueprint loads"), CharacterClass);
	APSPlayerCharacter* Character = World->SpawnActor<APSPlayerCharacter>(CharacterClass ? CharacterClass : APSPlayerCharacter::StaticClass());
	Controller->Possess(Character);
	Character->SetActorLocation(Grid->CellToWorldCenter(Shore));
	const FVector OriginalCameraOffset(12, -8, 31);
	Character->CameraBoom->TargetOffset = OriginalCameraOffset;
	const float OriginalCameraWidth = Character->Camera->OrthoWidth;
	const float OriginalCameraDistance = Character->CameraBoom->TargetArmLength;
	const FRotator OriginalCameraAngle = Character->CameraBoom->GetRelativeRotation();
	Character->CameraBoom->TickComponent(0, LEVELTICK_All, nullptr);
	APlayerCameraManager* CameraManager = World->SpawnActor<APlayerCameraManager>();
	CameraManager->bUseClientSideCameraUpdates = false;
	CameraManager->InitializeFor(Controller);
	Controller->PlayerCameraManager = CameraManager;
	Controller->SetViewTarget(Character);
	const auto CameraView = [Controller, CameraManager](const float DeltaTime)
	{
		Controller->UpdateCameraManager(DeltaTime);
		return CameraManager->GetCameraCacheView();
	};
	const auto ProjectCamera = [](FMinimalViewInfo View)
	{
		FSceneViewProjectionData Projection;
		const FIntRect Rect(0, 0, 1280, 720);
		Projection.SetViewRectangle(Rect);
		Projection.ViewOrigin = View.Location;
		Projection.ViewRotationMatrix = FInverseRotationMatrix(View.Rotation) * FMatrix(
			FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
		FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(View, AspectRatio_MaintainXFOV, Rect, Projection);
		return Projection;
	};
	const FMinimalViewInfo OriginalCameraView = CameraView(0);
	const FSceneViewProjectionData OriginalProjection = ProjectCamera(OriginalCameraView);
	FVector FocusLocation;
	UPaperFlipbook* CastingClip = NewObject<UPaperFlipbook>(World);
	{
		FScopedFlipbookMutator Mutator(CastingClip);
		Mutator.FramesPerSecond = 10;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FPaperFlipbookKeyFrame& Frame = Mutator.KeyFrames.AddDefaulted_GetRef();
			Frame.Sprite = NewObject<UPaperSprite>(World);
			Frame.FrameRun = 1;
		}
	}
	Character->FishingCastFlipbook = CastingClip;
	TestTrue(TEXT("Character can begin fishing"), Controller->TryUseFishingRod(Water));
	TestTrue(TEXT("Controller preparation timer follows the first two sprite durations"), FMath::IsNearlyEqual(Controller->FishingStateDuration, .2f));
	Controller->UpdateFishing(.1f);
	TestEqual(TEXT("Preparation does not finish before sprite two is complete"), Controller->GetFishingState(), EPSFishingState::CastWindup);
	Controller->UpdateFishing(.11f);
	TestEqual(TEXT("Completed preparation opens the timing gauge"), Controller->GetFishingState(), EPSFishingState::CastTiming);
	TestEqual(TEXT("Timing gauge holds sprite two"), Character->GetSprite()->GetPlaybackPositionInFrames(), 1);
	TestFalse(TEXT("Preparation keeps the camera on the player"), Controller->GetFishingFocusLocation(FocusLocation));
	TestTrue(TEXT("Character can confirm the throw"), Controller->ConfirmFishingCast());
	TestTrue(TEXT("A confirmed throw provides the shared camera and UI anchor"), Controller->GetFishingFocusLocation(FocusLocation));
	TestEqual(TEXT("Camera focus uses the actual landing cell"), FocusLocation,
		Grid->CellToWorldCenter(Controller->FishingCell.GetValue()) + FVector(0, 0, 8));
	const FVector FocusDelta = FocusLocation - Character->GetActorLocation();
	const FVector FocusedCameraLocation = OriginalCameraView.Location + FVector(FocusDelta.X, FocusDelta.Y, 0);
	const FMinimalViewInfo PartialCameraView = CameraView(.1f);
	TestFalse(TEXT("Rendered camera pans rather than jumping instantly"), PartialCameraView.Location.Equals(OriginalCameraView.Location));
	TestFalse(TEXT("Rendered camera has not completed its blend after 0.1 seconds"), PartialCameraView.Location.Equals(FocusedCameraLocation));
	APlayerState* CameraPauser = World->SpawnActor<APlayerState>();
	World->GetWorldSettings()->SetPauserPlayerState(CameraPauser);
	TestEqual(TEXT("Pause freezes the rendered fishing camera pan"), CameraView(1.f).Location, PartialCameraView.Location);
	World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	TestTrue(TEXT("The rendered Blueprint camera reaches the bobber while movement is locked"), CameraView(.3f).Location.Equals(FocusedCameraLocation, .1f));
	const FSceneViewProjectionData FishingProjection = ProjectCamera(CameraView(0));
	TestTrue(TEXT("Final orthographic projection preserves the camera pan instead of snapping to the pawn"),
		FVector2D(FishingProjection.ViewOrigin - OriginalProjection.ViewOrigin).Equals(FVector2D(FocusDelta), .1f));
	FVector2D FocusPixel;
	TestTrue(TEXT("Bobber projects onto the game screen"), FSceneView::ProjectWorldToScreen(FocusLocation,
		FishingProjection.GetConstrainedViewRect(), FishingProjection.ComputeViewProjectionMatrix(), FocusPixel));
	// The authored test camera has a small intentional offset; the bobber still stays near the centre.
	TestTrue(TEXT("Bobber is within forty pixels of screen centre after plane correction"),
		FocusPixel.Equals(FVector2D(640, 360), 40));
	TestEqual(TEXT("Fishing leaves the authored spring-arm offset untouched"), Character->CameraBoom->TargetOffset, OriginalCameraOffset);
	TestEqual(TEXT("Fishing preserves camera zoom"), Character->Camera->OrthoWidth, OriginalCameraWidth);
	TestEqual(TEXT("Fishing preserves camera distance"), Character->CameraBoom->TargetArmLength, OriginalCameraDistance);
	TestEqual(TEXT("Fishing preserves camera angle"), Character->CameraBoom->GetRelativeRotation(), OriginalCameraAngle);
	TestEqual(TEXT("Confirmation advances to throw sprite three"), Character->GetSprite()->GetPlaybackPositionInFrames(), 2);
	TestTrue(TEXT("Fishing locks character movement"), Character->IsMovementLocked());
	TestTrue(TEXT("Movement input starts retrieval"), Controller->CancelFishingForMovementInput());
	TestEqual(TEXT("Cancellation waits for retrieval"), Controller->GetFishingState(), EPSFishingState::Retrieving);
	TestTrue(TEXT("Retrieval keeps movement locked"), Character->IsMovementLocked());
	Controller->UpdateFishing(.4f);
	TestFalse(TEXT("Movement cancellation unlocks the character"), Character->IsMovementLocked());
	TestFalse(TEXT("Cancellation clears the bobber anchor"), Controller->GetFishingFocusLocation(FocusLocation));
	TestEqual(TEXT("Cancellation restores the actual original view"), CameraView(.4f).Location, OriginalCameraView.Location);
	TestTrue(TEXT("Cancellation restores the final projected view"),
		FVector2D(ProjectCamera(CameraView(0)).ViewOrigin).Equals(FVector2D(OriginalProjection.ViewOrigin), .1f));
	TestTrue(TEXT("Character can cast again after moving"), StartCast(Water));
	CameraView(.4f);
	Controller->BeginFishingBite();
	Controller->BeginFishingMinigame();
	TestFalse(TEXT("Movement cancellation does not consume minigame direction input"), Controller->CancelFishingForMovementInput());
	Controller->FishingSequence = {EPSFishingDirection::Up};
	Controller->FishingSequenceIndex = 0;
	Controller->SubmitFishingDirection(EPSFishingDirection::Up);
	TestTrue(TEXT("Success keeps movement locked during result motion"), Character->IsMovementLocked());
	TestFalse(TEXT("Success immediately starts returning from the bobber"), Controller->GetFishingFocusLocation(FocusLocation));
	TestEqual(TEXT("Rendered camera returns during the locked success animation"), CameraView(.4f).Location, OriginalCameraView.Location);
	Controller->UpdateFishing(2.0f);
	TestFalse(TEXT("Success result completion unlocks movement"), Character->IsMovementLocked());
	TestTrue(TEXT("Character can cast after success"), StartCast(Water));
	CameraView(.4f);
	Controller->BeginFishingBite();
	Controller->CompleteFishing(false);
	TestFalse(TEXT("Failure unlocks movement immediately"), Character->IsMovementLocked());
	TestFalse(TEXT("Failure clears the camera and UI anchor"), Controller->GetFishingFocusLocation(FocusLocation));
	TestEqual(TEXT("Failure restores the actual camera view"), CameraView(.4f).Location, OriginalCameraView.Location);
	Controller->UpdateFishing(2.0f);
	TestTrue(TEXT("Camera interruption fixture can cast"), StartCast(Water));
	CameraView(.1f);
	Controller->StopFishing();
	CameraView(.1f);
	TestTrue(TEXT("Can cast again before the camera has returned"), StartCast(Water));
	CameraView(.4f);
	Controller->UnPossess();
	FMinimalViewInfo UnpossessedView;
	Character->CalcCamera(.4f, UnpossessedView);
	TestEqual(TEXT("Losing the controller restores the original rendered view"), UnpossessedView.Location, OriginalCameraView.Location);
	Controller->UpdateFishing(0);
	Controller->Possess(Character);
	Character->FishingCameraBlendSeconds = 0;
	TestTrue(TEXT("Instant-camera fixture can cast"), StartCast(Water));
	const FVector InstantViewLocation = CameraView(0).Location;
	TestTrue(TEXT("Zero blend duration immediately focuses on the bobber"), Character->bWasFishingCameraFocused);
	TestFalse(TEXT("Zero blend duration actually moves the rendered view"), InstantViewLocation.Equals(OriginalCameraView.Location));
	Controller->StopFishing();
	TestEqual(TEXT("Zero blend duration immediately restores the rendered view"), CameraView(0).Location, OriginalCameraView.Location);
	Character->FishingCameraBlendSeconds = .35f;

	const int32 ExistingFish = Controller->InventoryComponent->CountItem(EPSItemType::Fish);
	TestTrue(TEXT("Fish stack can be filled"), Controller->InventoryComponent->AddItemVariant(
		EPSItemType::Fish, Controller->GetCurrentFishId(), 999 - ExistingFish));
	for (int32 Variant = 0; Variant < 9; ++Variant)
		TestTrue(TEXT("Remaining bag slots can be filled"), Controller->InventoryComponent->AddItem(EPSItemType::TestSeed, 999, Variant));
	int32 DropsBefore = 0;
	for (TActorIterator<APSWorldItemActor> It(World); It; ++It) ++DropsBefore;
	TestTrue(TEXT("Can cast with a full bag"), StartCast(Water));
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
	Grid->MiningGenerationVersion = 0;
	for (FPSTileCell& Cell : Grid->ModifiedChunks.FindChecked(ChunkCoord).Cells) Cell.GroundType = EPSTileType::Grass;
	for (int32 DY = -4; DY <= 4; ++DY)
		for (int32 DX = -4; DX <= 4; ++DX)
			if (PSGrid::CellToChunk(Water + FIntPoint(DX, DY), 8) == PSGrid::CellToChunk(Water, 8))
				TestTrue(TEXT("Legacy saved land suppresses the whole lake"), Grid->GetGroundTile(Water + FIntPoint(DX, DY)) != EPSTileType::Water);

	// Authored chunks make landing correction independent of nearby procedural lakes.
	Controller->Possess(Pawn);
	Grid->LoadedChunks.Empty(); Grid->ModifiedChunks.Empty(); Grid->ChunkSize = 16;
	for (int32 Y : {-1, 0}) for (int32 X : {-1, 0})
	{
		FPSChunkData& ChunkData = Grid->LoadedChunks.Add(FIntPoint(X, Y));
		ChunkData.Cells.SetNum(16 * 16);
		for (FPSTileCell& Cell : ChunkData.Cells) Cell.GroundType = EPSTileType::Grass;
	}
	FPSChunkData& CastChunk = Grid->LoadedChunks.FindChecked(FIntPoint::ZeroValue);
	for (int32 X = 1; X <= 10; ++X) CastChunk.Cells[X].GroundType = EPSTileType::Water;
	Pawn->SetActorLocation(Grid->CellToWorldCenter(FIntPoint::ZeroValue));
	for (int32 GradeIndex = 0; GradeIndex < 3; ++GradeIndex)
	{
		TestTrue(TEXT("Cast can begin toward an authored water strip"), Controller->TryUseFishingRod(FIntPoint(1, 0)));
		Controller->UpdateFishing(Controller->CastWindupDuration);
		Controller->UpdateFishing(Controller->CastTravelSeconds * (GradeIndex == 2 ? .5f : GradeIndex == 1 ? .3f : 0));
		TestTrue(TEXT("Each timing grade can be confirmed"), Controller->ConfirmFishingCast());
		TestEqual(TEXT("Timing selects its corresponding grade"), static_cast<int32>(Controller->GetFishingCastGrade()), GradeIndex);
		TestEqual(TEXT("Landing follows the chosen distance"), Controller->FishingCell.GetValue(), FIntPoint((GradeIndex + 1) * 2, 0));
		Controller->StopFishing();
	}
	CastChunk.Cells[3].GroundType = EPSTileType::Grass;
	TestTrue(TEXT("Can start a throw near an island"), Controller->TryUseFishingRod(FIntPoint(1, 0)));
	Controller->UpdateFishing(Controller->CastWindupDuration);
	Controller->UpdateFishing(Controller->CastTravelSeconds * .5f);
	TestTrue(TEXT("Long throw crosses an island"), Controller->ConfirmFishingCast());
	TestEqual(TEXT("Land along the path does not shorten the throw"), Controller->FishingCell.GetValue(), FIntPoint(6, 0));
	CastChunk.Cells[1].GroundType = EPSTileType::Grass;
	Controller->UpdateFishing(0);
	TestTrue(TEXT("Changing the aim tile after landing does not cancel fishing at a valid landing"), Controller->IsFishing());
	Controller->StopFishing();
	const auto ClearCastWater = [&]()
	{
		for (auto& Pair : Grid->LoadedChunks)
			for (FPSTileCell& Cell : Pair.Value.Cells) Cell.GroundType = EPSTileType::Grass;
	};
	ClearCastWater();
	CastChunk.Cells[1].GroundType = EPSTileType::Water;
	CastChunk.Cells[5 + 16].GroundType = EPSTileType::Water;
	Grid->LoadedChunks.FindChecked(FIntPoint(0, -1)).Cells[5 + 15 * 16].GroundType = EPSTileType::Water;
	for (int32 Repeat = 0; Repeat < 3; ++Repeat)
		TestEqual(TEXT("Dry destination corrects to the closest water with a stable tie-break"),
			Controller->ResolveFishingLandingCell(6).GetValue(), FIntPoint(5, -1));
	Grid->SetActorLocation(FVector(1000, -500, 0));
	Pawn->SetActorLocation(Grid->CellToWorldCenter(FIntPoint::ZeroValue));
	TestEqual(TEXT("Correction respects a translated grid origin"), Controller->ResolveFishingLandingCell(6).GetValue(), FIntPoint(5, -1));
	Grid->SetActorLocation(FVector::ZeroVector);
	Pawn->SetActorLocation(Grid->CellToWorldCenter(FIntPoint::ZeroValue));
	ClearCastWater();
	CastChunk.Cells[6].GroundType = EPSTileType::Water;
	TestEqual(TEXT("Water exactly six cells from the player is eligible for correction"), Controller->ResolveFishingLandingCell(4).GetValue(), FIntPoint(6, 0));
	ClearCastWater();
	CastChunk.Cells[5 + 5 * 16].GroundType = EPSTileType::Water;
	TestFalse(TEXT("Correction radius excludes diagonal water beyond six cells"), Controller->ResolveFishingLandingCell(6).IsSet());
	ClearCastWater();
	CastChunk.Cells[3].GroundType = EPSTileType::Water;
	Grid->WorldHalfExtentInCells = 4;
	TestEqual(TEXT("Out-of-world destination corrects to in-world water"), Controller->ResolveFishingLandingCell(6).GetValue(), FIntPoint(3, 0));
	Grid->WorldHalfExtentInCells = 50;
	ClearCastWater();
	CastChunk.Cells[1].GroundType = EPSTileType::Water;
	TestTrue(TEXT("Free a bag slot for cancellation bait fixture"), Controller->InventoryComponent->RemoveItem(
		EPSItemType::Fish, Controller->InventoryComponent->CountItem(EPSItemType::Fish)));
	TestTrue(TEXT("Cancellation bait is added"), Controller->InventoryComponent->AddItem(EPSItemType::FishingBait, 2));
	TestTrue(TEXT("Can prepare before nearby water is removed"), Controller->TryUseFishingRod(FIntPoint(1, 0)));
	Controller->UpdateFishing(Controller->CastWindupDuration);
	Controller->UpdateFishing(Controller->CastTravelSeconds * .5f);
	ClearCastWater();
	CastChunk.Cells[7].GroundType = EPSTileType::Water;
	const int32 NoWaterDurability = Controller->InventoryComponent->GetHotbarSlot(2).CurrentDurability;
	TestFalse(TEXT("No water within six cells cancels confirmation"), Controller->ConfirmFishingCast());
	TestFalse(TEXT("No-water cancellation returns to idle"), Controller->IsFishing());
	TestEqual(TEXT("No-water cancellation does not consume bait"), Controller->InventoryComponent->CountItem(EPSItemType::FishingBait), 2);
	TestEqual(TEXT("No-water cancellation does not consume rod durability"), Controller->InventoryComponent->GetHotbarSlot(2).CurrentDurability, NoWaterDurability);
	CastChunk.Cells[1].GroundType = EPSTileType::Water;
	TestTrue(TEXT("Can start pause fixture"), Controller->TryUseFishingRod(FIntPoint(1, 0)));
	Controller->UpdateFishing(Controller->CastWindupDuration);
	APlayerState* Pauser = World->SpawnActor<APlayerState>();
	World->GetWorldSettings()->SetPauserPlayerState(Pauser);
	Controller->UpdateFishing(.5f);
	TestEqual(TEXT("Pause freezes the casting marker"), Controller->GetFishingCastPosition(), 0.0f);
	TestFalse(TEXT("Pause rejects cast confirmation"), Controller->ConfirmFishingCast());
	World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	Controller->StopFishing();
	TestTrue(TEXT("Can start unmatched-season fixture"), Controller->TryUseFishingRod(FIntPoint(1, 0)));
	Controller->UpdateFishing(Controller->CastWindupDuration);
	Controller->FishingSeason = 3;
	const int32 BeforeRejectedThrow = Controller->InventoryComponent->GetHotbarSlot(2).CurrentDurability;
	TestFalse(TEXT("No candidate rejects the throw cleanly"), Controller->ConfirmFishingCast());
	TestFalse(TEXT("Rejected throw restores idle"), Controller->IsFishing());
	TestEqual(TEXT("Rejected throw does not consume durability"), Controller->InventoryComponent->GetHotbarSlot(2).CurrentDurability, BeforeRejectedThrow);

	// Unequal within-tier weights must not distort the designed difficulty distribution.
	UDataTable* ProbabilityFish = NewObject<UDataTable>(World); ProbabilityFish->RowStruct = FPSFishDefinition::StaticStruct();
	for (int32 Tier = 1; Tier <= 4; ++Tier)
	{
		FPSFishDefinition Fish; Fish.Difficulty = Tier; Fish.Location = {2}; Fish.Weight = Tier == 1 ? 100000.f : 1.f;
		ProbabilityFish->AddRow(*FString::Printf(TEXT("Tier%d"), Tier), Fish);
	}
	Controller->FishDataTable = ProbabilityFish;
	Controller->FishingCastDefinitions.Reset();
	for (const EPSFishingCastGrade Grade : {EPSFishingCastGrade::Green, EPSFishingCastGrade::Yellow, EPSFishingCastGrade::Red})
	{
		int32 Counts[4] = {};
		for (int32 Sample = 0; Sample < 6000; ++Sample)
		{
			FName Id; const FPSFishDefinition Fish = Controller->SelectFishDefinition(Id, 0, 2, Grade);
			if (!Id.IsNone() && Fish.Difficulty >= 1 && Fish.Difficulty <= 4) ++Counts[Fish.Difficulty - 1];
		}
		const FPSFishingCastDefinition Expected = PSFishing::GetDefaultCastDefinition(Grade);
		for (int32 Tier = 0; Tier < 4; ++Tier)
			TestTrue(FString::Printf(TEXT("Grade %d tier %d probability follows design"), static_cast<int32>(Grade), Tier + 1),
				FMath::Abs(Counts[Tier] / 6000.f - Expected.DifficultyWeights[Tier] / 100.f) < .04f);
	}
	FPSFishDefinition ZeroWeight; ZeroWeight.Weight = 0;
	UDataTable* EmptyWeightTable = NewObject<UDataTable>(World); EmptyWeightTable->RowStruct = FPSFishDefinition::StaticStruct();
	EmptyWeightTable->AddRow(TEXT("Disabled"), ZeroWeight);
	Controller->FishDataTable = EmptyWeightTable;
	FName DisabledId;
	Controller->SelectFishDefinition(DisabledId, 0, 2);
	TestTrue(TEXT("Disabled weights cannot produce a reward"), DisabledId.IsNone());
	Controller->FishDataTable = ImportedFishData;
	const FName RodIds[] = {PSItemIds::WoodenFishingRod, PSItemIds::PlasticFishingRod, PSItemIds::AluminumFishingRod,
		PSItemIds::FiberglassFishingRod, PSItemIds::CarbonFishingRod, PSItemIds::AsteriumFishingRod};
	const float ExpectedSeconds[] = {10, 12, 13, 14, 15, 16};
	for (int32 Index = 0; Index < 6; ++Index)
		TestTrue(TEXT("Rod tiers apply percentage-based bonus time"), FMath::IsNearlyEqual(
			10 * (1 + PSItems::GetDefinition(EPSItemType::FishingRod, RodIds[Index]).FishingTimeBonusRatio), ExpectedSeconds[Index]));
	TestTrue(TEXT("Marker returns to the left edge after a full cycle"), FMath::IsNearlyZero(PSFishing::GetCastPosition(1.4f, .7f)));
	const FVector4f CastEdges = PSFishing::GetCastBandEdges(.38f, .83f);
	for (const float X : {0.f, CastEdges.X - .001f, CastEdges.W + .001f, 1.f})
		TestEqual(TEXT("Both outer bands are green"), PSFishing::GetCastGrade(X, .38f, .83f), EPSFishingCastGrade::Green);
	for (const float X : {CastEdges.X, .3f, .7f, CastEdges.W})
		TestEqual(TEXT("Both middle bands are yellow including outer boundaries"), PSFishing::GetCastGrade(X, .38f, .83f), EPSFishingCastGrade::Yellow);
	for (const float X : {CastEdges.Y, .5f, CastEdges.Z})
		TestEqual(TEXT("The central band is red including both boundaries"), PSFishing::GetCastGrade(X, .38f, .83f), EPSFishingCastGrade::Red);
	const FVector4f CustomEdges = PSFishing::GetCastBandEdges(.2f, .6f);
	TestEqual(TEXT("Editable thresholds determine visible band edges"), CustomEdges, FVector4f(.1f, .3f, .7f, .9f));
	TestEqual(TEXT("Custom bands change the grade"), PSFishing::GetCastGrade(.35f, .2f, .6f), EPSFishingCastGrade::Red);
	const FVector4f ClampedEdges = PSFishing::GetCastBandEdges(5, -1);
	TestTrue(TEXT("Invalid threshold order still gives five ordered bands"),
		ClampedEdges.X < ClampedEdges.Y && ClampedEdges.Y < ClampedEdges.Z && ClampedEdges.Z < ClampedEdges.W);

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
	APSGridWorld* PondGrid = World->SpawnActor<APSGridWorld>();
	PondGrid->SaveSlotName = TEXT("FishingPondTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	PondGrid->MiningGenerationVersion = 0;
	PondGrid->ChunkSize = 7;
	PondGrid->WorldHalfExtentInCells = 40;
	PondGrid->FishingTestPondOrigin = FIntPoint(-4, 2);
	const FIntPoint IslandCell = PondGrid->FishingTestPondOrigin + FIntPoint(3, 3);
	const FIntPoint IslandChunk = PSGrid::CellToChunk(IslandCell, 7);
	FPSChunkData& OldPondChunk = PondGrid->GetOrCreateChunk(IslandChunk);
	OldPondChunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(IslandCell, 7), 7)].GroundType = EPSTileType::Water;
	PondGrid->bEnableFishingTestPond = true;
	PondGrid->ApplyFishingTestPond();
	int32 PondWater = 0, IslandGrass = 0;
	for (int32 Y = -1; Y <= 8; ++Y) for (int32 X = -1; X <= 8; ++X)
	{
		const FIntPoint Cell = PondGrid->FishingTestPondOrigin + FIntPoint(X, Y);
		const bool bIsland = X >= 3 && X <= 4 && Y >= 3 && Y <= 4;
		const bool bRim = X == -1 || X == 8 || Y == -1 || Y == 8;
		TestEqual(TEXT("Authored pond has a dry rim and a central 2x2 grass island"), PondGrid->GetGroundTile(Cell),
			bIsland || bRim ? EPSTileType::Grass : EPSTileType::Water);
		TestEqual(TEXT("The test area has no obstructing rocks"), PondGrid->GetWorldObjectType(Cell), EPSWorldObjectType::None);
		if (!bIsland && !bRim && PondGrid->GetGroundTile(Cell) == EPSTileType::Water) ++PondWater;
		if (bIsland && PondGrid->GetGroundTile(Cell) == EPSTileType::Grass) ++IslandGrass;
	}
	TestEqual(TEXT("8x8 pond leaves sixty water cells"), PondWater, 60);
	TestEqual(TEXT("The central island has four grass cells"), IslandGrass, 4);
	TestTrue(TEXT("Fishing is possible from the near bank"), PondGrid->CanFishFrom(
		PondGrid->FishingTestPondOrigin + FIntPoint(-1, 3), PondGrid->FishingTestPondOrigin + FIntPoint(0, 3)));
	const FPSChunkData IslandRender = PondGrid->BuildRenderChunkData(IslandChunk, PondGrid->GetOrCreateChunk(IslandChunk));
	TestEqual(TEXT("The island renderer uses grass rather than water"),
		IslandRender.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(IslandCell, 7), 7)].GroundType, EPSTileType::Grass);
	PondGrid->SaveWorld();
	PondGrid->LoadedChunks.Empty();
	PondGrid->ModifiedChunks.Empty();
	PondGrid->LoadWorld();
	TestEqual(TEXT("The test island survives saved terrain loading"), PondGrid->GetGroundTile(IslandCell), EPSTileType::Grass);
	UGameplayStatics::DeleteGameInSlot(PondGrid->SaveSlotName, 0);
	World->DestroyActor(PondGrid);
	// Exercise the new layout separately: the user's existing WBP is deliberately not regenerated.
	UPSFishingWidget* NewUI = CreateWidget<UPSFishingWidget>(World, UPSFishingWidget::StaticClass());
	if (TestNotNull(TEXT("New native fishing UI creates"), NewUI))
	{
		NewUI->TakeWidget();
		TestEqual(TEXT("Bite bubble stays inside a short viewport's top edge"),
			UPSFishingWidget::ClampPanelPosition(FVector2D(160, -100), FVector2D(150, 110), FVector2D(.5f, 1), FVector2D(320, 180)), FVector2D(160, 118));
		TestEqual(TEXT("Bite bubble stays inside the bottom and right edges"),
			UPSFishingWidget::ClampPanelPosition(FVector2D(1000, 1000), FVector2D(150, 110), FVector2D(.5f, 1), FVector2D(320, 180)), FVector2D(237, 172));
		const TArray<EPSFishingDirection> Directions = {EPSFishingDirection::Up, EPSFishingDirection::Left,
			EPSFishingDirection::Down, EPSFishingDirection::Right, EPSFishingDirection::Up, EPSFishingDirection::Left};
		UTexture2D* CurrentTexture = NewObject<UTexture2D>(World);
		UTexture2D* CompleteTexture = NewObject<UTexture2D>(World);
		NewUI->CurrentArrowBrushes.Right.SetResourceObject(CurrentTexture);
		NewUI->CompleteArrowBrushes.Down.SetResourceObject(CompleteTexture);
		NewUI->Refresh(EPSFishingState::Minigame, 8, 10, Directions, 3, FText::GetEmpty(), 0);
		TestEqual(TEXT("Seven real arrow widgets are authored"), NewUI->FishDirectionRow->GetChildrenCount(), 7);
		TestEqual(TEXT("The fourth slot uses its current-state arrow"), NewUI->DirectionImages[3]->GetBrush().GetResourceObject(), static_cast<UObject*>(CurrentTexture));
		TestEqual(TEXT("Completed arrows retain their direction and use completion art"), NewUI->DirectionImages[2]->GetBrush().GetResourceObject(), static_cast<UObject*>(CompleteTexture));
		TestEqual(TEXT("Completed arrows remain visible"), NewUI->DirectionImages[2]->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("Legacy check marks stay hidden"), NewUI->DirectionLabels[3]->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Only the current slot's marker is visible"), NewUI->DirectionMarkers[3]->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("Other slot markers are hidden"), NewUI->DirectionMarkers[2]->GetVisibility(), ESlateVisibility::Collapsed);
		NewUI->ShowWrongInput();
		TestEqual(TEXT("Wrong input flashes the current slot"), NewUI->DirectionBoxes[3]->Background.TintColor.GetSpecifiedColor(), NewUI->WrongSlotBrush.TintColor.GetSpecifiedColor());
		NewUI->NativeTick(FGeometry(), .21f);
		TestEqual(TEXT("The error flash expires without another input"), NewUI->DirectionBoxes[3]->Background.TintColor.GetSpecifiedColor(), NewUI->CurrentSlotBrush.TintColor.GetSpecifiedColor());
		NewUI->ShowWrongInput();
		NewUI->Refresh(EPSFishingState::Minigame, 8, 10, Directions, 4, FText::GetEmpty(), 0);
		TestEqual(TEXT("Correct input immediately clears the error flash"), NewUI->WrongInputRemaining, 0.f);
		NewUI->CurrentDirectionSlot = 0;
		NewUI->Refresh(EPSFishingState::Minigame, 8, 10, Directions, 0, FText::GetEmpty(), 0);
		TestEqual(TEXT("Current slot can be changed without editing logic"), NewUI->DirectionMarkers[0]->GetVisibility(), ESlateVisibility::HitTestInvisible);
		NewUI->CurrentDirectionSlot = 3;
		NewUI->Refresh(EPSFishingState::CastTiming, 0, 0, {}, 0, FText::GetEmpty(), 0, nullptr, 1, .75f, .2f, .6f);
		TestNotNull(TEXT("The casting marker supports an Image"), Cast<UImage>(NewUI->CastMarker));
		UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(NewUI->CastMarker->Slot);
		TestEqual(TEXT("Horizontal marker follows the normalized position"), MarkerSlot->GetAnchors().Minimum, FVector2D(.75f, .5f));
		const UBorder* Zones[] = {NewUI->CastGreen, NewUI->CastYellow, NewUI->CastRed, NewUI->CastYellowRight, NewUI->CastGreenRight};
		const float Boundaries[] = {0, CustomEdges.X, CustomEdges.Y, CustomEdges.Z, CustomEdges.W, 1};
		for (int32 Index = 0; Index < 5; ++Index)
		{
			UCanvasPanelSlot* ZoneSlot = Cast<UCanvasPanelSlot>(Zones[Index]->Slot);
			TestEqual(TEXT("Visible band starts at the actual grade boundary"), ZoneSlot->GetAnchors().Minimum, FVector2D(Boundaries[Index], 0));
			TestEqual(TEXT("Visible band ends at the actual grade boundary"), ZoneSlot->GetAnchors().Maximum, FVector2D(Boundaries[Index + 1], 1));
		}
		NewUI->Refresh(EPSFishingState::Success, 1, 1.5f, {}, 0, FText::FromString(TEXT("붕어")), 30);
		TestEqual(TEXT("Catch name is independently available"), NewUI->ResultNameLabel->GetText().ToString(), FString(TEXT("붕어")));
		TestEqual(TEXT("Catch size is independently available"), NewUI->ResultSizeLabel->GetText().ToString(), FString(TEXT("30 cm")));
		TestEqual(TEXT("Split result does not duplicate the old combined label"), NewUI->ResultLabel->GetVisibility(), ESlateVisibility::Collapsed);
		NewUI->Refresh(EPSFishingState::Failure, 1, 1.5f, {}, 0, FText::GetEmpty(), 0);
		TestEqual(TEXT("Failure hides the previous catch name"), NewUI->ResultNameLabel->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Failure still displays its message"), NewUI->ResultLabel->GetVisibility(), ESlateVisibility::HitTestInvisible);
		NewUI->Refresh(EPSFishingState::Idle, 0, 0, {}, 0, FText::GetEmpty(), 0);
		NewUI->ShowWrongInput();
		TestEqual(TEXT("An idle UI cannot retain an error flash"), NewUI->WrongInputRemaining, 0.f);
	}
	UClass* DesignerClass = LoadClass<UPSFishingWidget>(nullptr, TEXT("/Game/UI/Fishing/WBP_Fishing.WBP_Fishing_C"));
	TestNotNull(TEXT("Editable fishing Widget Blueprint loads"), DesignerClass);
	UPSFishingWidget* FishingUI = CreateWidget<UPSFishingWidget>(World, DesignerClass ? DesignerClass : UPSFishingWidget::StaticClass());
	TestNotNull(TEXT("Fishing UI creates"), FishingUI);
	if (FishingUI)
	{
		// Force the Slate tree to initialize, as AddToViewport does in normal play.
		FishingUI->TakeWidget();
		UWidget* DesignerRoot = FishingUI->WidgetTree->RootWidget;
		UCanvasPanelSlot* GameSlot = Cast<UCanvasPanelSlot>(FishingUI->Panel->Slot);
		const FVector2D AuthoredGameSize = GameSlot->GetSize();
		const FVector2D CastSize = Cast<UCanvasPanelSlot>(FishingUI->FishCastPanel->Slot)->GetSize();
		TestTrue(TEXT("The actual prototype asset has a wide casting panel"), CastSize.X > CastSize.Y * 3);
		TestTrue(TEXT("The actual prototype asset has a wide minigame panel"), GameSlot->GetSize().X > GameSlot->GetSize().Y * 3);
		TestNotNull(TEXT("The actual prototype asset includes the seventh arrow"), FishingUI->DirectionImages[6].Get());
		TestNotNull(TEXT("The actual prototype asset includes the right green band"), FishingUI->CastGreenRight.Get());
		TestNotNull(TEXT("The actual prototype asset includes the right yellow band"), FishingUI->CastYellowRight.Get());
		TestNotNull(TEXT("The actual prototype asset uses an image casting marker"), Cast<UImage>(FishingUI->CastMarker));
		TestNotNull(TEXT("Imported layout includes separate success content"), FishingUI->FishResultSuccessContent.Get());
		TestNotNull(TEXT("Imported layout includes separate failure content"), FishingUI->FishResultFailureContent.Get());
		TestTrue(TEXT("Current arrows use the imported orange artwork"),
			FishingUI->CurrentArrowBrushes.Up.GetResourceObject() && FishingUI->CurrentArrowBrushes.Up.GetResourceObject()->GetPathName().Contains(TEXT("/Arrow_Now/")));
		const FVector2f ArrowSourceSize = FishingUI->CurrentArrowBrushes.Up.GetImageSize();
		TestTrue(TEXT("Imported arrows retain nonzero source dimensions for ScaleBox layout"), ArrowSourceSize.X > 0 && ArrowSourceSize.Y > 0);
		TestTrue(TEXT("Completed arrows use the imported green artwork"),
			FishingUI->CompleteArrowBrushes.Up.GetResourceObject() && FishingUI->CompleteArrowBrushes.Up.GetResourceObject()->GetPathName().Contains(TEXT("/Arrow_O/")));
		TestNotNull(TEXT("Imported paper and wood background are present"), FishingUI->WidgetTree->FindWidget(TEXT("GameWoodFrame")));
		const FBox2f CastRedUV = FishingUI->CastRed->Background.GetUVRegion();
		TestTrue(TEXT("The red cast band crops only the source image's red segment"),
			FMath::IsNearlyEqual(CastRedUV.Min.X, 325.f / 784));
		GameSlot->SetSize(FVector2D(612, 177));
		FishingUI->Refresh(EPSFishingState::Minigame, 8.0f, 10.0f,
			{EPSFishingDirection::Up, EPSFishingDirection::Left, EPSFishingDirection::Down}, 0,
			FText::FromString(TEXT("붕어")), 18, nullptr);
		TestEqual(TEXT("Active fishing UI is visible"), FishingUI->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("Fishing UI connects seven optional direction slots"), FishingUI->DirectionImages.Num(), 7);
		TestNotNull(TEXT("Fishing standalone arrow is loaded"), FishingUI->UpArrowBrush.GetResourceObject());
		TestEqual(TEXT("Current fishing arrow image is shown in slot four"), FishingUI->DirectionImages[3]->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestNotNull(TEXT("Current arrow uses the atlas texture"), FishingUI->DirectionImages[3]->GetBrush().GetResourceObject());
		TestEqual(TEXT("Refresh preserves the designer tree"), FishingUI->WidgetTree->RootWidget.Get(), DesignerRoot);
		TestEqual(TEXT("Refresh preserves designer panel dimensions"), GameSlot->GetSize(), FVector2D(612, 177));
		GameSlot->SetSize(AuthoredGameSize);
		const FSlateBrush OriginalUpArrow = FishingUI->UpArrowBrush;
		const FSlateBrush OriginalCurrentUpArrow = FishingUI->CurrentArrowBrushes.Up;
		FishingUI->CurrentArrowBrushes.Up = FSlateBrush();
		FishingUI->UpArrowBrush.SetResourceObject(NewObject<UTexture2D>(World));
		FishingUI->Refresh(EPSFishingState::Minigame, 8, 10, {EPSFishingDirection::Up}, 0, FText::GetEmpty(), 0);
		const FBox2f ReplacementUV = FishingUI->DirectionImages[3]->GetBrush().GetUVRegion();
		TestEqual(TEXT("A replacement arrow image uses its full UV rectangle"), ReplacementUV.Max, FVector2f(1));
		FishingUI->UpArrowBrush = OriginalUpArrow;
		FishingUI->CurrentArrowBrushes.Up = OriginalCurrentUpArrow;
		FishingUI->Refresh(EPSFishingState::Minigame, 8, 10,
			{EPSFishingDirection::Up, EPSFishingDirection::Left, EPSFishingDirection::Down}, 0, FText::GetEmpty(), 0);
		const auto Preview = [&](const TCHAR* Name)
		{
			if (!FParse::Param(FCommandLine::Get(), TEXT("FishingUIPreview")) || FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) return;
			FWidgetRenderer Renderer(true);
			UTextureRenderTarget2D* Target = Renderer.DrawWidget(FishingUI->TakeWidget(), FVector2D(1920, 1080));
			FlushRenderingCommands();
			FBufferArchive PNG;
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FishingUI");
			IFileManager::Get().MakeDirectory(*Directory, true);
			TestTrue(TEXT("Fishing UI preview renders"), Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG)
				&& FFileHelper::SaveArrayToFile(PNG, *(Directory / (FString(Name) + TEXT(".png")))));
		};
		FishingUI->Refresh(EPSFishingState::Minigame, 8, 10,
			{EPSFishingDirection::Up, EPSFishingDirection::Left, EPSFishingDirection::Down, EPSFishingDirection::Right,
			 EPSFishingDirection::Down, EPSFishingDirection::Up, EPSFishingDirection::Left}, 3, FText::GetEmpty(), 0);
		Preview(TEXT("Minigame"));
		FishingUI->ShowWrongInput();
		Preview(TEXT("WrongInput"));
		FishingUI->NativeTick(FGeometry(), .21f);
		TestEqual(TEXT("Minigame still shows its time bar"), FishingUI->TimeBar->GetVisibility(), ESlateVisibility::HitTestInvisible);
		FishingUI->Refresh(EPSFishingState::WaitingForBite, 8, 10, {}, 0, FText::GetEmpty(), 0);
		TestEqual(TEXT("Waiting for a bite hides the countdown bar"), FishingUI->TimeBar->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Waiting for a bite hides the empty minigame panel"), FishingUI->Panel->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("No waiting UI remains on screen"), FishingUI->GetVisibility(), ESlateVisibility::Collapsed);
		Preview(TEXT("Waiting"));
		FishingUI->Refresh(EPSFishingState::CastTiming, 0, 0, {}, 0, FText::GetEmpty(), 0, nullptr, 1, .85f);
		TestEqual(TEXT("Cast timing shows its own panel"), FishingUI->FishCastPanel->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("Cast timing hides minigame panel"), FishingUI->Panel->GetVisibility(), ESlateVisibility::Collapsed);
		Preview(TEXT("Casting"));
		FishingUI->Refresh(EPSFishingState::BiteWindow, 5, 5, {}, 0, FText::GetEmpty(), 0, nullptr);
		TestEqual(TEXT("Bite shows the yellow exclamation bubble"), FishingUI->FishBitePanel->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("Bite reaction has no waiting countdown bar"), FishingUI->TimeBar->GetVisibility(), ESlateVisibility::Collapsed);
		Preview(TEXT("Bite"));
		// Preview uses the supplied decorative fish; gameplay icons continue to come from the Icon table.
		UTexture2D* PreviewFish = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Art/Fish/Icon_DummyFish.Icon_DummyFish"));
		FishingUI->Refresh(EPSFishingState::Success, .25f, 1.5f, {}, 0, FText::FromString(TEXT("붕어")), 30, PreviewFish);
		TestEqual(TEXT("Result popup uses the lower-left anchor"), Cast<UCanvasPanelSlot>(FishingUI->FishResultPanel->Slot)->GetAnchors().Minimum, FVector2D(0, 1));
		TestEqual(TEXT("Result sits close to the screen bottom"), Cast<UCanvasPanelSlot>(FishingUI->FishResultPanel->Slot)->GetPosition(), FVector2D(24, -24));
		Preview(TEXT("Result"));
		FishingUI->Refresh(EPSFishingState::Failure, 1, 1, {}, 0, FText::GetEmpty(), 0);
		TestEqual(TEXT("Failure hides success paper and labels together"), FishingUI->FishResultSuccessContent->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Failure shows the standalone message panel"), FishingUI->FishResultFailureContent->GetVisibility(), ESlateVisibility::HitTestInvisible);
		Preview(TEXT("Failure"));
		FishingUI->Refresh(EPSFishingState::Idle, 0, 0, {}, 0, FText::GetEmpty(), 0, nullptr);
		TestEqual(TEXT("Idle fishing UI is hidden"), FishingUI->GetVisibility(), ESlateVisibility::Collapsed);
		FishingUI->ShowNotice(FText::FromString(TEXT("물에서 해야 해요")));
		TestNotNull(TEXT("X has a separate overhead panel"), FishingUI->FishNoticeCrossPanel.Get());
		TestEqual(TEXT("Notice uses the lower-left anchor"), Cast<UCanvasPanelSlot>(FishingUI->FishNoticePanel->Slot)->GetAnchors().Minimum, FVector2D(0, 1));
		TestEqual(TEXT("Notice is near the bottom"), Cast<UCanvasPanelSlot>(FishingUI->FishNoticePanel->Slot)->GetPosition(), FVector2D(24, -24));
		TestEqual(TEXT("X is visible with the notice"), FishingUI->FishNoticeCrossPanel->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("Unavailable action displays its reason"), FishingUI->NoticeLabel->GetText().ToString(), FString(TEXT("물에서 해야 해요")));
		Preview(TEXT("Notice"));
		FishingUI->ShowNotice(FText::FromString(TEXT("낚시대가 필요해요")), EPSFishingNoticeType::RodRequired);
		TestEqual(TEXT("Rod notice preserves the original lettering image"), FishingUI->NoticeArtwork->GetBrush().GetResourceObject(), FishingUI->RodNoticeBrush.GetResourceObject());
		TestEqual(TEXT("Rod image text is not duplicated by a TextBlock"), FishingUI->NoticeLabel->GetVisibility(), ESlateVisibility::Collapsed);
		Preview(TEXT("NoticeRod"));
		FishingUI->ShowNotice(FText::FromString(TEXT("물에서 해야 해요")), EPSFishingNoticeType::WaterRequired);
		TestEqual(TEXT("Water notice preserves the original lettering image"), FishingUI->NoticeArtwork->GetBrush().GetResourceObject(), FishingUI->WaterNoticeBrush.GetResourceObject());
		TestEqual(TEXT("Water image text is not duplicated by a TextBlock"), FishingUI->NoticeLabel->GetVisibility(), ESlateVisibility::Collapsed);
		Preview(TEXT("NoticeWater"));
		FishingUI->ShowNotice(FText::FromString(TEXT("지금 잡을 수 있는 물고기가 없어요")));
		TestEqual(TEXT("General notices restore editable text after a baked banner"), FishingUI->NoticeLabel->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("General text notices still use the shared font"), FishingUI->NoticeLabel->GetFont().FontObject.Get(), static_cast<const UObject*>(UPSUIWidget::GetProjectFont()));
		FishingUI->Refresh(EPSFishingState::Success, 3, 3, {}, 0, FText::FromString(TEXT("붕어")), 30, PreviewFish);
		TestEqual(TEXT("Success clears the previous notice banner"), FishingUI->FishNoticePanel->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Success clears the previous overhead X"), FishingUI->FishNoticeCrossPanel->GetVisibility(), ESlateVisibility::Collapsed);
	}
	World->DestroyActor(Renderer);
	UGameplayStatics::DeleteGameInSlot(Grid->SaveSlotName, 0);
	UGameplayStatics::DeleteGameInSlot(Controller->FishingJournalComponent->SaveSlotName, 0);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
