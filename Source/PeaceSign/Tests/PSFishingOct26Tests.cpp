#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../Fishing/PSFishingWaterRegion.h"
#include "../Fishing/PSFishingBobber.h"
#include "../Fishing/PSFishingJournalComponent.h"
#include "../UI/PSFishingJournalWidget.h"
#include "../World/PSGridWorld.h"
#include "Components/BoxComponent.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CapsuleComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "RenderingThread.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSFishingOct26Test, "PeaceSign.Fishing.October26", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSFishingOct26Test::RunTest(const FString&)
{
	const FVector4f Edges = FPSFishingCastBands().GetEdges();
	TestTrue(TEXT("Asymmetric design ratios produce four exact edges"), Edges.Equals(FVector4f(.2f, .35f, .65f, .85f), .00001f));
	TestEqual(TEXT("Right yellow extends past the old mirrored boundary"), PSFishing::GetCastGrade(.82f, Edges), EPSFishingCastGrade::Yellow);
	TestEqual(TEXT("Red starts at 35 percent"), PSFishing::GetCastGrade(Edges.Y, Edges), EPSFishingCastGrade::Red);
	TestEqual(TEXT("Red ends inclusively at 65 percent"), PSFishing::GetCastGrade(Edges.Z, Edges), EPSFishingCastGrade::Red);
	FPSFishingCastBands Invalid; Invalid.Red = NAN;
	TestEqual(TEXT("Invalid editor ratios safely fall back"), Invalid.GetEdges(), Edges);
	FPSFishingCastBands Custom; Custom.GreenLeft = 40;
	TestTrue(TEXT("Edited ratios normalize as a whole"), FMath::IsNearlyEqual(Custom.GetEdges().X, 1.f / 3));

	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
	auto* CatchVisual = World->SpawnActor<APSFishingBobber>();
	const FVector ThrowStart(0, 0, 25), ThrowEnd(400, 0, 8);
	const auto ArcPosition = [&](const float T)
	{
		CatchVisual->UpdateVisual(ThrowStart, ThrowEnd, false, 0, 0, T);
		return CatchVisual->GetActorLocation();
	};
	TestEqual(TEXT("Flight starts at the rod tip"), ArcPosition(0), ThrowStart);
	TestEqual(TEXT("Flight lands exactly on the selected water"), ArcPosition(1), ThrowEnd);
	TestTrue(TEXT("Flight rises above its straight path"), ArcPosition(.5f).Z > FMath::Max(ThrowStart.Z, ThrowEnd.Z));
	const double MiddleSpeed = FVector::Distance(ArcPosition(.45f), ArcPosition(.55f));
	TestTrue(TEXT("Departure is faster than the middle"), FVector::Distance(ArcPosition(0), ArcPosition(.1f)) > MiddleSpeed);
	TestTrue(TEXT("Arrival is faster than the middle"), FVector::Distance(ArcPosition(.9f), ArcPosition(1)) > MiddleSpeed);
	const FVector MidFlight = ArcPosition(.4f);
	CatchVisual->UpdateVisual(ThrowStart, ThrowEnd, false, 0, .5f, .4f);
	TestTrue(TEXT("Mid-flight cancellation retrieves from its current path"), CatchVisual->GetActorLocation().Equals(FMath::Lerp(MidFlight, ThrowStart, .5f)));
	const auto TopDownPosition = [&](const float T)
	{
		CatchVisual->UpdateVisual(ThrowStart, ThrowEnd, false, 0, 0, T, FVector::ForwardVector);
		return CatchVisual->GetActorLocation();
	};
	TestTrue(TEXT("Arc is visible in the top-down camera plane"), TopDownPosition(.5f).X > FMath::Lerp(ThrowStart, ThrowEnd, .5f).X);
	const double TopDownMiddleSpeed = FVector::Dist2D(TopDownPosition(.45f), TopDownPosition(.55f));
	TestTrue(TEXT("Top-down departure is faster than the middle"), FVector::Dist2D(TopDownPosition(0), TopDownPosition(.1f)) > TopDownMiddleSpeed);
	TestTrue(TEXT("Top-down arrival is faster than the middle"), FVector::Dist2D(TopDownPosition(.9f), TopDownPosition(1)) > TopDownMiddleSpeed);
	TestEqual(TEXT("Top-down flight still lands exactly on water"), TopDownPosition(1), ThrowEnd);
	const float CrestTime = CatchVisual->FloatPeriod * .25f;
	const FVector Crest = ThrowEnd + FVector::ForwardVector * CatchVisual->FloatAmplitude;
	const FVector TopDownMidFlight = TopDownPosition(.5f);
	CatchVisual->UpdateVisual(ThrowStart, ThrowEnd, false, CrestTime, 0, .5f, FVector::ForwardVector);
	TestEqual(TEXT("Float motion does not affect flight"), CatchVisual->GetActorLocation(), TopDownMidFlight);
	CatchVisual->UpdateVisual(ThrowStart, ThrowEnd, false, CrestTime, 0, 1, FVector::ForwardVector);
	TestTrue(TEXT("Landed bobber rises along screen-up"), CatchVisual->GetActorLocation().Equals(Crest));
	CatchVisual->UpdateVisual(ThrowStart, ThrowEnd, false, CrestTime * 3, 0, 1, FVector::ForwardVector);
	TestTrue(TEXT("Landed bobber falls back below its resting point"), CatchVisual->GetActorLocation().Equals(ThrowEnd - FVector::ForwardVector * CatchVisual->FloatAmplitude));
	CatchVisual->UpdateVisual(ThrowStart, ThrowEnd, false, CrestTime, .5f, 1, FVector::ForwardVector);
	TestTrue(TEXT("Retrieval starts from the bobber's floating position"), CatchVisual->GetActorLocation().Equals(FMath::Lerp(Crest, ThrowStart, .5f)));
#if WITH_EDITOR
	// Force a fresh editor texture build rather than testing only already-cached assets.
	UTexture2D* ColdFish = NewObject<UTexture2D>(World);
	ColdFish->LODGroup = TEXTUREGROUP_UI;
	ColdFish->CompressionSettings = TC_EditorIcon;
	ColdFish->MipGenSettings = TMGS_NoMipmaps;
	TArray<FColor> ColdPixels;
	ColdPixels.Init(FColor(40, 100, 200, 255), 2048 * 1024);
	ColdFish->Source.Init(2048, 1024, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(ColdPixels.GetData()));
	ColdFish->BeginCachePlatformData();
	ColdFish->UpdateResource();
	const bool bCompilingAtFirstCatch = ColdFish->IsCompiling();
	const FIntPoint FirstReportedSize(ColdFish->GetSizeX(), ColdFish->GetSizeY());
	CatchVisual->ShowCatch(ColdFish);
	const double FirstCatchScale = CatchVisual->CatchImage->GetComponentScale().GetMax();
	FTextureCompilingManager::Get().FinishCompilation({ColdFish});
	const double FirstCatchWidth = FirstCatchScale * FMath::Max(ColdFish->GetSizeX(), ColdFish->GetSizeY());
	CatchVisual->ShowCatch(ColdFish);
	const double SecondCatchScale = CatchVisual->CatchImage->GetComponentScale().GetMax();
	AddInfo(FString::Printf(TEXT("First catch: compiling=%d reported=%dx%d final=%dx%d firstScale=%.6f secondScale=%.6f"),
		bCompilingAtFirstCatch, FirstReportedSize.X, FirstReportedSize.Y, ColdFish->GetSizeX(), ColdFish->GetSizeY(), FirstCatchScale, SecondCatchScale));
	TestTrue(TEXT("First uncached catch is bounded after the real texture arrives"), FMath::IsNearlyEqual(FirstCatchWidth, static_cast<double>(CatchVisual->CatchImageWorldSize), .01));
	TestTrue(TEXT("First and subsequent catches have identical scale"), FMath::IsNearlyEqual(FirstCatchScale, SecondCatchScale, .00001));
#endif
	UTexture2D* DummyFish = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Art/Fish/Icon_DummyFish.Icon_DummyFish"));
	if (TestNotNull(TEXT("Artist dummy fish texture loads"), DummyFish))
	{
		CatchVisual->ShowCatch(DummyFish);
		const float LargeDisplayWidth = CatchVisual->CatchImage->GetComponentScale().X * FMath::Max(DummyFish->GetSizeX(), DummyFish->GetSizeY());
		UTexture2D* SmallFish = UTexture2D::CreateTransient(32, 16);
		CatchVisual->ShowCatch(SmallFish);
		const float SmallDisplayWidth = CatchVisual->CatchImage->GetComponentScale().X * 32;
		TestTrue(TEXT("Changing icon resolution preserves overhead display size"), FMath::IsNearlyEqual(LargeDisplayWidth, SmallDisplayWidth));
		TestTrue(TEXT("Overhead image fits its configured world size"), FMath::IsNearlyEqual(LargeDisplayWidth, CatchVisual->CatchImageWorldSize));
		CatchVisual->ShowCatch(nullptr);
		TestTrue(TEXT("Missing catch art is hidden"), CatchVisual->CatchImage->bHiddenInGame);
	}
	auto* Controller = World->SpawnActor<APSPlayerController>();
	Controller->FishingJournalComponent->bAutoSave = false;
	Controller->FishingJournalComponent->SaveSlotName = TEXT("FishingOct26_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Character = World->SpawnActor<APSPlayerCharacter>(); Controller->Possess(Character);
	Controller->FishDataTable = NewObject<UDataTable>(Controller); Controller->FishDataTable->RowStruct = FPSFishDefinition::StaticStruct();
	FPSFishDefinition Fish; Fish.Name = FText::FromString(TEXT("Test Fish")); Fish.DescriptionPlus = FText::FromString(TEXT("LOCKED_SPECIAL_TEXT")); Fish.MaxSize = 40;
	Controller->FishDataTable->AddRow(TEXT("test"), Fish);
	FPSItemStack Item; Item.ItemType = EPSItemType::Fish; Item.ItemId = TEXT("test"); Item.Quantity = 1;
	Controller->FishingJournalComponent->RecordCatch(TEXT("test"), 39, 1, 40);
	TestFalse(TEXT("Below maximum does not unlock special description"), Controller->GetItemDescription(Item).ToString().Contains(TEXT("LOCKED_SPECIAL_TEXT")));
	Controller->FishingJournalComponent->RecordCatch(TEXT("test"), 40, 1, 40);
	TestTrue(TEXT("Maximum size unlocks mastery"), Controller->HasMasteredFish(TEXT("test")));
	TestTrue(TEXT("Maximum size reveals special description"), Controller->GetItemDescription(Item).ToString().Contains(TEXT("LOCKED_SPECIAL_TEXT")));
	TestTrue(TEXT("Journal saves mastery"), Controller->FishingJournalComponent->SaveJournal());
	Controller->FishingJournalComponent->ResetJournal();
	TestTrue(TEXT("Journal restores mastery"), Controller->FishingJournalComponent->LoadJournal());
	Fish.MaxSize = 50; Controller->FishDataTable->AddRow(TEXT("test"), Fish);
	TestTrue(TEXT("Previously earned star survives balance changes"), Controller->HasMasteredFish(TEXT("test")));
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->Player->PlayerController = Controller;
	World->AddController(Controller);
	UClass* JournalClass = LoadClass<UPSFishingJournalWidget>(nullptr, TEXT("/Game/UI/Encyclopedia/WBP_Encyclopedia.WBP_Encyclopedia_C"));
	TestNotNull(TEXT("Editable journal asset loads"), JournalClass);
	auto* JournalUI = CreateWidget<UPSFishingJournalWidget>(Controller, JournalClass ? JournalClass : UPSFishingJournalWidget::StaticClass());
	if (TestNotNull(TEXT("Book journal can be created"), JournalUI))
	{
		TestEqual(TEXT("Journal resolves its owning gameplay controller"), JournalUI->GetOwningPlayer(), static_cast<APlayerController*>(Controller));
		JournalUI->RefreshJournal();
		TestEqual(TEXT("Mastered species shows its star"), JournalUI->FishStar->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(TEXT("Journal special area includes the unlocked special text"), JournalUI->FishSpecial->GetText().ToString().Contains(TEXT("LOCKED_SPECIAL_TEXT")));
		if (FParse::Param(FCommandLine::Get(), TEXT("FishingUIPreview")) && !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
		{
			FWidgetRenderer Renderer(true);
			UTextureRenderTarget2D* Target = Renderer.DrawWidget(JournalUI->TakeWidget(), FVector2D(1280, 720));
			FlushRenderingCommands();
			FBufferArchive PNG;
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FishingUI");
			IFileManager::Get().MakeDirectory(*Directory, true);
			TestTrue(TEXT("Book journal preview renders"), Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG)
				&& FFileHelper::SaveArrayToFile(PNG, *(Directory / TEXT("Journal.png"))));
		}
		JournalUI->SelectFish(TEXT("undiscovered"));
		TestFalse(TEXT("Undiscovered entry cannot leak the special text"), JournalUI->FishSpecial->GetText().ToString().Contains(TEXT("LOCKED_SPECIAL_TEXT")));
	}

	auto* Grid = World->SpawnActor<APSGridWorld>(); Grid->SaveSlotName = TEXT("FishingOct26Grid_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Controller->GridWorld = Grid;
	FIntPoint Water = FIntPoint::ZeroValue;
	bool bWater = false;
	for (int32 Y = -30; Y < 30 && !bWater; ++Y)
		for (int32 X = -30; X < 30 && !bWater; ++X)
			if (Grid->GetGroundTile(FIntPoint(X, Y)) == EPSTileType::Water) { Water = FIntPoint(X, Y); bWater = true; }
	if (TestTrue(TEXT("Region fixture finds water"), bWater))
	{
		auto* River = World->SpawnActor<APSFishingWaterRegion>(); River->SetActorLocation(Grid->CellToWorldCenter(Water)); River->Bounds->SetBoxExtent(FVector(40)); River->Location = EPSFishingLocation::River;
		TestEqual(TEXT("River area overrides the lake default"), Grid->GetFishingLocationId(Water), 1);
		auto* Sea = World->SpawnActor<APSFishingWaterRegion>(); Sea->SetActorLocation(River->GetActorLocation()); Sea->Bounds->SetBoxExtent(FVector(40)); Sea->Priority = 1; Sea->Location = EPSFishingLocation::Sea;
		TestEqual(TEXT("Overlapping higher priority area wins"), Grid->GetFishingLocationId(Water), 0);
		Sea->Destroy(); River->Destroy();
		TestEqual(TEXT("Removing regions restores the lake fallback"), Grid->GetFishingLocationId(Water), 2);
		TestEqual(TEXT("Region never turns land into fishable water"), Grid->GetFishingLocationId(FIntPoint::ZeroValue), INDEX_NONE);
	}

	UPaperFlipbook* Clip = NewObject<UPaperFlipbook>(World);
	{ FScopedFlipbookMutator Mutator(Clip); Mutator.FramesPerSecond = 1; auto& Frame = Mutator.KeyFrames.AddDefaulted_GetRef(); Frame.Sprite = NewObject<UPaperSprite>(World); Frame.FrameRun = 4; }
	Character->FishingSuccessFlipbook = Clip;
	Controller->FishingState = EPSFishingState::Minigame; Controller->CurrentFish = Fish; Controller->CurrentFishId = TEXT("test"); Controller->FishingPawn = Character;
	Character->SetMovementLocked(true);
	Controller->CompleteFishing(true);
	Controller->UpdateFishing(2);
	TestTrue(TEXT("Four-second success clip stays locked during result display"), Character->IsMovementLocked());
	Character->GetSprite()->Stop(); Controller->UpdateFishing(.01f);
	TestFalse(TEXT("Actual clip completion unlocks controls"), Character->IsMovementLocked());
	TestEqual(TEXT("Result survives early animation completion"), Controller->GetFishingState(), EPSFishingState::Success);
	Controller->UpdateFishing(1);
	TestEqual(TEXT("Finished success returns to idle"), Controller->GetFishingState(), EPSFishingState::Idle);
	Controller->FishingState = EPSFishingState::Minigame;
	Controller->CompleteFishing(false);
	Controller->UpdateFishing(2.99f);
	TestEqual(TEXT("Failure result remains until three seconds"), Controller->GetFishingState(), EPSFishingState::Failure);
	Controller->UpdateFishing(.02f);
	TestEqual(TEXT("Failure result closes after three seconds"), Controller->GetFishingState(), EPSFishingState::Idle);
	Character->FishingRetrieveFlipbook = Clip;
	Controller->FishingState = EPSFishingState::WaitingForBite; Controller->FishingPawn = Character; Character->SetMovementLocked(true);
	Controller->BeginFishingRetrieve(); Controller->UpdateFishing(1);
	TestEqual(TEXT("Retrieval waits for its own clip"), Controller->GetFishingState(), EPSFishingState::Retrieving);
	TestTrue(TEXT("Retrieval blocks repeated cancellation input"), Controller->AreFishingControlsLocked());
	TestFalse(TEXT("Repeat movement cannot restart retrieval"), Controller->CancelFishingForMovementInput());
	Character->GetSprite()->Stop(); Controller->UpdateFishing(.01f);
	TestEqual(TEXT("Finished retrieval restores idle"), Controller->GetFishingState(), EPSFishingState::Idle);
	TestFalse(TEXT("Finished retrieval unlocks movement"), Character->IsMovementLocked());
	Controller->StopFishing(false, false);
	UGameplayStatics::DeleteGameInSlot(Controller->FishingJournalComponent->SaveSlotName, 0);
	GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}
#endif
