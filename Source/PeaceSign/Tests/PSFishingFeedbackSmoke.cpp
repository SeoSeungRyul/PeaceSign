#if WITH_DEV_AUTOMATION_TESTS
#include "../PSPlayerController.h"
#include "../UI/PSFishingWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "Engine/Font.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	struct FFishingFeedbackSmoke
	{
		TWeakObjectPtr<UWorld> World;
		FTimerHandle Timer;
		double Started = FPlatformTime::Seconds();
		double PhaseStarted = Started;
		int32 Phase = 0;
		FString Screenshot;

		void Finish(bool bSuccess)
		{
			if (World.IsValid()) World->GetTimerManager().ClearTimer(Timer);
			UE_LOG(LogTemp, Display, TEXT("FISHING_FEEDBACK_SMOKE %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"));
			FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
		}

		void Capture(const TCHAR* Name)
		{
			Screenshot = FPaths::ProjectSavedDir() / TEXT("FishingFeedbackSmoke") / (FString(Name) + TEXT(".png"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Screenshot), true);
			FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
			PhaseStarted = FPlatformTime::Seconds();
		}

		void Tick()
		{
			if (!World.IsValid() || FPlatformTime::Seconds() - Started > 20) { Finish(false); return; }
			auto* Controller = Cast<APSPlayerController>(World->GetFirstPlayerController());
			if (!Controller || !Controller->GetPawn()) return;
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UPSFishingWidget::StaticClass(), true);
			if (Widgets.Num() != 1) return;
			auto* UI = CastChecked<UPSFishingWidget>(Widgets[0]);
			const double Elapsed = FPlatformTime::Seconds() - PhaseStarted;
			if (Phase == 0)
			{
				TArray<UUserWidget*> GameUI;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), GameUI, UPSUIWidget::StaticClass(), true);
				int32 FontCount = 0;
				bool bFontsMatch = true;
				for (UUserWidget* Widget : GameUI)
					if (Widget->WidgetTree) Widget->WidgetTree->ForEachWidgetAndDescendants([&](UWidget* Child)
					{
						if (auto* Text = Cast<UTextBlock>(Child))
						{
							++FontCount;
							bFontsMatch &= Text->GetFont().FontObject == UPSUIWidget::GetProjectFont();
						}
					});
				UE_LOG(LogTemp, Display, TEXT("UI_PROJECT_FONT_SMOKE widgets=%d texts=%d match=%d"), GameUI.Num(), FontCount, bFontsMatch);
				if (FontCount < 6 || !bFontsMatch) { Finish(false); return; }
				UI->ShowNotice(FText::FromString(TEXT("물가에서 가까운 물을 향해 사용해 주세요")), EPSFishingNoticeType::WaterRequired);
				Phase = 1; PhaseStarted = FPlatformTime::Seconds();
			}
			else if (Phase == 1 && Elapsed >= .2)
			{
				UWidget* Cross = UI->WidgetTree->FindWidget(TEXT("FishNoticeCrossPanel"));
				UWidget* Notice = UI->WidgetTree->FindWidget(TEXT("FishNoticePanel"));
				if (!Cross || !Notice) { Finish(false); return; }
				FVector2D PlayerPosition;
				if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller,
					Controller->GetPawn()->GetActorLocation() + FVector(0, 0, 70), PlayerPosition, true)) { Finish(false); return; }
				auto* Slot = CastChecked<UCanvasPanelSlot>(Cross->Slot);
				const FVector2D CrossBottom = Slot->GetPosition();
				UE_LOG(LogTemp, Display, TEXT("FISHING_FEEDBACK_SMOKE player=%s cross=%s notice=%s"),
					*PlayerPosition.ToString(), *CrossBottom.ToString(), *CastChecked<UCanvasPanelSlot>(Notice->Slot)->GetPosition().ToString());
				if (FMath::Abs(CrossBottom.X - PlayerPosition.X) > 1 || CrossBottom.Y > PlayerPosition.Y - 100) { Finish(false); return; }
				Capture(TEXT("Notice")); Phase = 2;
			}
			else if (Phase == 2 && Elapsed >= 2.1 && IFileManager::Get().FileExists(*Screenshot))
			{
				const auto Ids = Controller->GetFishIds();
				if (Ids.IsEmpty()) { Finish(false); return; }
				UI->Refresh(EPSFishingState::Success, 3, 3, {}, 0, FText::FromString(TEXT("붕어")), 30, Controller->GetFishIcon(Ids[0]));
				Phase = 3; PhaseStarted = FPlatformTime::Seconds();
			}
			else if (Phase == 3 && Elapsed >= .2) { Capture(TEXT("Success")); Phase = 4; }
			else if (Phase == 4 && Elapsed >= .3 && IFileManager::Get().FileExists(*Screenshot))
			{
				UI->Refresh(EPSFishingState::Failure, 3, 3, {}, 0, FText::GetEmpty(), 0);
				Phase = 5; PhaseStarted = FPlatformTime::Seconds();
			}
			else if (Phase == 5 && Elapsed >= .2) { Capture(TEXT("Failure")); Phase = 6; }
			else if (Phase == 6 && Elapsed >= .3 && IFileManager::Get().FileExists(*Screenshot)) Finish(true);
		}
	};

	FAutoConsoleCommandWithWorld FeedbackSmokeCommand(TEXT("PS.Fishing.FeedbackSmoke"),
		TEXT("Verify fishing notice projection and capture feedback with an isolated -UserDir."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			FString UserDirectory;
			if (!World || !World->IsGameWorld() || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory) || UserDirectory.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("Feedback smoke requires a game world and isolated -UserDir.")); return;
			}
			const TSharedRef<FFishingFeedbackSmoke> State = MakeShared<FFishingFeedbackSmoke>(); State->World = World;
			World->GetTimerManager().SetTimer(State->Timer, FTimerDelegate::CreateLambda([State]() { State->Tick(); }), .02f, true);
		}));
}
#endif
