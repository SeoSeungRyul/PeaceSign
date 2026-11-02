#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerController.h"
#include "../UI/PSFishingJournalWidget.h"
#include "../UI/PSInventoryWidget.h"
#include "../UI/PSHotbarWidget.h"
#include "../Fishing/PSFishingJournalComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "Slate/WidgetRenderer.h"
#include "RenderingThread.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSBookJournalTest, "PeaceSign.UI.BookJournal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSBookJournalTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
	auto* Controller = World->SpawnActor<APSPlayerController>();
	Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->Player->PlayerController = Controller;
	World->AddController(Controller);
	Controller->GetFishingJournalComponent()->bAutoSave = false;
	Controller->FishDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataBase/DT_Fish"));
	Controller->FishIconDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataBase/DT_FishIcon"));
	UClass* Class = LoadClass<UPSFishingJournalWidget>(nullptr, TEXT("/Game/UI/Encyclopedia/WBP_Encyclopedia.WBP_Encyclopedia_C"));
	auto* UI = Class ? CreateWidget<UPSFishingJournalWidget>(Controller, Class) : nullptr;
	if (!TestNotNull(TEXT("Approved book Blueprint loads"), UI)) return false;
	TestEqual(TEXT("Journal resolves the local gameplay controller"), UI->GetOwningPlayer(), static_cast<APlayerController*>(Controller));
	UI->RefreshJournal();
	const TArray<FName> Ids = Controller->GetFishIds();
	TestTrue(TEXT("Journal reads real fish database"), Ids.Num() > 0);
	TestEqual(TEXT("Page has sixteen grid tracks"), UI->FishList->GetChildrenCount(), 16);
	TestTrue(TEXT("Undiscovered fish name is concealed"), UI->FishName->GetText().ToString().Contains(TEXT("???")));
	TestTrue(TEXT("Undiscovered fish image is concealed"), UI->FishImage->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Special description starts concealed"), UI->FishSpecial->GetText().IsEmpty());
	if (!Ids.IsEmpty())
	{
		const FPSFishDefinition* Fish = Controller->FindFishDefinition(Ids[0]);
		Controller->GetFishingJournalComponent()->RecordCatch(Ids[0], Fish->MinSize, 1, Fish->MaxSize);
		UI->RefreshJournal();
		TestTrue(TEXT("Discovery reveals database name"), UI->FishName->GetText().ToString().Contains(Fish->Name.ToString()));
		TestEqual(TEXT("Description reads database row"), UI->FishDetails->GetText().ToString(), Fish->Description.ToString());
		TestEqual(TEXT("Below maximum uses gray star"), UI->FishStar->GetBrush().GetResourceObject(), static_cast<UObject*>(UI->StarTextures[0]));
		TestTrue(TEXT("Discovery retains special lock"), UI->FishLock->GetVisibility() == ESlateVisibility::HitTestInvisible);
		Controller->GetFishingJournalComponent()->RecordCatch(Ids[0], Fish->MaxSize, 1, Fish->MaxSize);
		UI->RefreshJournal();
		TestEqual(TEXT("Mastery uses yellow star"), UI->FishStar->GetBrush().GetResourceObject(), static_cast<UObject*>(UI->StarTextures[1]));
		TestEqual(TEXT("Mastery reveals database special description"), UI->FishSpecial->GetText().ToString(), Fish->DescriptionPlus.ToString());
		TestTrue(TEXT("Mastery removes special lock"), UI->FishLock->GetVisibility() == ESlateVisibility::Collapsed);
	}
	for (int32 Category = 1; Category <= 3; ++Category)
	{
		UI->SelectCategory(Category);
		TestEqual(TEXT("Unavailable categories have no invented entries"), UI->FishList->GetChildrenCount(), 0);
		TestTrue(TEXT("Unavailable category clears fish details"), UI->FishDetails->GetText().IsEmpty());
		const TCHAR* Names[] = {TEXT("CategoryFish"), TEXT("CategoryGathering"), TEXT("CategoryFood"), TEXT("CategoryArtifact")};
		for (int32 Index = 0; Index < 4; ++Index) TestEqual(TEXT("Only active category extends outward"),
			UI->WidgetTree->FindWidget(Names[Index])->GetRenderTransform().Translation.X, Index == Category ? 64.0 : 0.0);
	}
	UI->SelectCategory(0);
	TestEqual(TEXT("Fish tab restores database grid"), UI->FishList->GetChildrenCount(), 16);
	auto* Expanded = NewObject<UDataTable>(Controller); Expanded->RowStruct = FPSFishDefinition::StaticStruct();
	FPSFishDefinition Extra; Extra.Name = FText::FromString(TEXT("Pagination fixture"));
	for (int32 Index = 0; Index < 16; ++Index) Expanded->AddRow(*FString::Printf(TEXT("Fixture%02d"), Index), Extra);
	Expanded->AddRow(TEXT("ZZ_AdditionalFish"), Extra); Controller->FishDataTable = Expanded;
	UI->RefreshJournal(); UI->NextPage();
	TestEqual(TEXT("Seventeenth fish has a second page"), UI->Page, 1);
	TestEqual(TEXT("Sparse last page preserves sixteen grid tracks"), UI->FishList->GetChildrenCount(), 16);
	TestEqual(TEXT("Sparse last page selects final species"), UI->SelectedFish, FName(TEXT("ZZ_AdditionalFish")));
	UI->NextPage(); TestEqual(TEXT("Cannot page beyond last page"), UI->Page, 1);
	UI->PreviousPage(); UI->PreviousPage(); TestEqual(TEXT("Cannot page before first page"), UI->Page, 0);
	UI->ReleaseSlateResources(true); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}

// Runs in the real map with an isolated UserDir. Core ticker continues while the book pauses the world.
struct FPSBookJournalSmoke
{
	TWeakObjectPtr<UWorld> World;
	double Started = FPlatformTime::Seconds();
	int32 Phase = 0;
	bool Result = false;
	double CaptureStarted = 0;
	bool Check(bool Valid, const TCHAR* Message)
	{
		UE_LOG(LogTemp, Display, TEXT("BOOK_JOURNAL_SMOKE %s %s"), Valid ? TEXT("PASS") : TEXT("FAIL"), Message); return Valid;
	}
	bool Render(UUserWidget* UI, FIntPoint Size, const TCHAR* Name)
	{
		FWidgetRenderer Renderer(true);
		auto Slate = UI->TakeWidget();
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(Slate, FVector2D(Size));
		// ScaleBox normalizes against the last painted geometry; settle it after each resize.
		for (int32 Pass = 0; Pass < 3 && Target; ++Pass) Renderer.DrawWidget(Target, Slate, FVector2D(Size), 1.f / 60.f);
		FlushRenderingCommands();
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		if (!Target || !Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) return false;
		TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/BookJournal");
		IFileManager::Get().MakeDirectory(*Directory, true);
		return FFileHelper::SaveArrayToFile(PNG, *(Directory / Name));
	}
	FBox2D NotebookBounds(UUserWidget* UI)
	{
		FBox2D Bounds(ForceInit);
		const FGeometry Root = UI->GetCachedGeometry();
		UI->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			auto* Image = Cast<UImage>(Widget);
			if (!Image) return;
			const UObject* Resource = Image->GetBrush().GetResourceObject();
			if (Image->GetFName() != TEXT("BookBackground") && Image->GetFName() != TEXT("BookBinder") && (!Resource || Resource->GetFName() != TEXT("Inven_Background"))) return;
			const FGeometry Geometry = Image->GetCachedGeometry();
			Bounds += Root.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
			Bounds += Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
		});
		return Bounds;
	}
	bool Tick(float)
	{
		auto* Controller = World.IsValid() ? Cast<APSPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!Controller || !Controller->GetPawn())
		{
			if (FPlatformTime::Seconds() - Started < 30) return true;
			FPlatformMisc::RequestExitWithStatus(false, 1); return false;
		}
		if (Phase > 0)
		{
			if (FPlatformTime::Seconds() - CaptureStarted < .7) return true;
			if (Phase == 1)
			{
				FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::I, FModifierKeysState(), 0, false, 0, 0));
				FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::I, FModifierKeysState(), 0, false, 0, 0));
				Result &= Check(!Controller->bJournalOpen && !Controller->bInventoryOpen && !Controller->IsPaused(), TEXT("I closes whole book and releases its pause"));
				Result &= Check(Controller->HotbarWidget && Controller->HotbarWidget->IsInViewport(), TEXT("Closing book restores gameplay hotbar"));
				Controller->CloseInventory(); Controller->HandleInventory(); Controller->ToggleFishingJournal();
				Phase = 2; CaptureStarted = FPlatformTime::Seconds(); return true;
			}
			if (Phase == 2)
			{
				FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
				FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
				Result &= Check(!Controller->bJournalOpen && !Controller->bInventoryOpen && !Controller->IsPaused(), TEXT("Esc closes whole book"));
				Controller->CloseInventory(); Controller->HandleInventory(); Controller->ToggleFishingJournal();
				Controller->FishingJournalWidget->SelectFish(Controller->GetFishIds()[0]);
				Phase = 3; CaptureStarted = FPlatformTime::Seconds(); return true;
			}
			if (Phase == 3)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/BookJournal/Viewport.png"), true, false);
				Phase = 4; CaptureStarted = FPlatformTime::Seconds(); return true;
			}
			UE_LOG(LogTemp, Display, TEXT("BOOK_JOURNAL_SMOKE_RESULT %s species=%d"), Result ? TEXT("PASS") : TEXT("FAIL"), Controller->GetFishIds().Num());
			Controller->CloseInventory(); FPlatformMisc::RequestExitWithStatus(false, Result ? 0 : 1); return false;
		}
		bool Valid = true;
		for (const FInputKeyBinding& Binding : Controller->InputComponent->KeyBindings) Valid &= Check(Binding.Chord.Key != EKeys::F6, TEXT("No F6 binding"));
		Controller->ToggleFishingJournal(); Valid &= Check(!Controller->bJournalOpen, TEXT("Journal cannot open from gameplay"));
		Controller->HandleInventory(); Valid &= Check(Controller->bInventoryOpen && Controller->IsPaused(), TEXT("I opens and pauses the inventory book"));
		Valid &= Render(Controller->InventoryWidget, FIntPoint(1280, 800), TEXT("Inventory.png"));
		const FBox2D InventoryBounds = NotebookBounds(Controller->InventoryWidget);
		// Find the button actually attached over the artist's Encyclopedia image.
		UImage* TabImage = Cast<UImage>(Controller->InventoryWidget->GetWidgetFromName(TEXT("Button_Encyclopedia")));
		UButton* Tab = nullptr;
		if (TabImage) for (UWidget* Child : TabImage->GetParent()->GetAllChildren()) if (auto* Button = Cast<UButton>(Child)) Tab = Button;
		if (!Check(Tab != nullptr, TEXT("Inventory Encyclopedia tab is interactive"))) Valid = false;
		if (Tab) Tab->OnClicked.Broadcast();
		Valid &= Check(Controller->bJournalOpen && Controller->bInventoryOpen && Controller->IsPaused(), TEXT("Inventory tab opens journal without losing the book pause"));
		auto* Journal = Controller->FishingJournalWidget.Get();
		if (!Journal) { FPlatformMisc::RequestExitWithStatus(false, 1); return false; }
		const TArray<FName> Ids = Controller->GetFishIds();
		auto* Records = Controller->GetFishingJournalComponent(); Records->bAutoSave = false; Records->ResetJournal();
		Journal->RefreshJournal(); Valid &= Render(Journal, FIntPoint(1300, 960), TEXT("Undiscovered.png"));
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			const auto* Fish = Controller->FindFishDefinition(Ids[Index]);
			Records->RecordCatch(Ids[Index], Index == 0 ? Fish->MaxSize : Fish->MinSize, 1, Fish->MaxSize);
		}
		Journal->RefreshJournal();
		Valid &= Render(Journal, FIntPoint(1280, 800), TEXT("JournalMatched.png"));
		const FBox2D JournalBounds = NotebookBounds(Journal);
		UE_LOG(LogTemp, Display, TEXT("NOTEBOOK_BOUNDS inventory=%s journal=%s"), *InventoryBounds.ToString(), *JournalBounds.ToString());
		Valid &= Check(InventoryBounds.bIsValid && JournalBounds.bIsValid && InventoryBounds.Min.Equals(JournalBounds.Min, 1.0) && InventoryBounds.Max.Equals(JournalBounds.Max, 1.0), TEXT("Inventory and journal notebook bounds match at the same resolution"));
		Valid &= Render(Journal, FIntPoint(1920, 1080), TEXT("Fish.png"));
		Valid &= Render(Journal, FIntPoint(800, 600), TEXT("FishSmall.png"));
		for (int32 Category = 1; Category <= 3; ++Category)
		{
			CastChecked<UButton>(Journal->GetWidgetFromName(Category == 1 ? TEXT("CategoryGathering") : Category == 2 ? TEXT("CategoryFood") : TEXT("CategoryArtifact")))->OnClicked.Broadcast();
			Valid &= Check(Journal->Category == Category && Journal->FishList->GetChildrenCount() == 0, TEXT("Side tab changes category and clears fish"));
		}
		Valid &= Render(Journal, FIntPoint(1300, 960), TEXT("EmptyCategory.png"));
		CastChecked<UButton>(Journal->GetWidgetFromName(TEXT("CategoryFish")))->OnClicked.Broadcast();
		Journal->SelectFish(Ids[1]); Valid &= Render(Journal, FIntPoint(1300, 960), TEXT("Discovered.png"));
		CastChecked<UButton>(Journal->GetWidgetFromName(TEXT("InventoryTab")))->OnClicked.Broadcast();
		Valid &= Check(!Controller->bJournalOpen && Controller->bInventoryOpen && Controller->IsPaused(), TEXT("Inventory tab returns without unpausing"));
		Controller->ToggleFishingJournal();
		Journal->NativeOnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::F6, FModifierKeysState(), 0, false, 0, 0));
		Valid &= Check(Controller->bJournalOpen, TEXT("F6 does not close or toggle the journal"));
		Result = Valid;
		Phase = 1; CaptureStarted = FPlatformTime::Seconds(); return true;
	}
};
namespace
{
	FAutoConsoleCommandWithWorld BookJournalSmoke(TEXT("PS.UI.BookJournalSmoke"), TEXT("Verify approved book flow and render previews; requires an isolated -UserDir."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			FString UserDirectory;
			if (!World || !World->IsGameWorld() || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory) || UserDirectory.IsEmpty()) return;
			auto State = MakeShared<FPSBookJournalSmoke>(); State->World = World;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State](float Delta) { return State->Tick(Delta); }), .1f);
		}));
}
#endif
