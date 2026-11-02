#if WITH_DEV_AUTOMATION_TESTS
#include "../PSPlayerController.h"
#include "../World/PSGridWorld.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "../Fishing/PSFishingJournalComponent.h"
#include "../Fishing/PSFishingBobber.h"
#include "../UI/PSFishingJournalWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"

namespace
{
	// Run against the real map, local player, viewport and frame loop with isolated saves:
	// -game -UserDir=<temporary-directory> -ExecCmds="PS.Fishing.CameraSmoke"
	struct FFishingCameraSmoke
	{
		TWeakObjectPtr<UWorld> World;
		FTimerHandle Timer;
		int32 Phase = 0;
		double Started = FPlatformTime::Seconds();
		double PhaseStarted = 0;
		FIntPoint Water;
		bool bLongCast = false;
		bool bFlightCaptured = false;

		void Finish(const bool bSuccess)
		{
			if (World.IsValid()) World->GetTimerManager().ClearTimer(Timer);
			UE_LOG(LogTemp, Display, TEXT("FISHING_CAMERA_SMOKE %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"));
			FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
		}

		bool CheckCentre(APSPlayerController* Controller, const FVector& Target, const TCHAR* Label)
		{
			int32 Width = 0, Height = 0;
			Controller->GetViewportSize(Width, Height);
			FVector2D Pixel;
			const bool bProjected = Controller->ProjectWorldLocationToScreen(Target, Pixel);
			const double Error = FVector2D::Distance(Pixel, FVector2D(Width * .5, Height * .5));
			UE_LOG(LogTemp, Display, TEXT("FISHING_CAMERA_SMOKE %s target=%s pixel=%s size=%dx%d error=%.2f camera=%s"),
				Label, *Target.ToString(), *Pixel.ToString(), Width, Height, Error,
				*Controller->PlayerCameraManager->GetCameraLocation().ToString());
			return bProjected && Width > 0 && Height > 0 && Error < 5;
		}

		bool CheckJournal(APSPlayerController* Controller)
		{
			const TArray<FName> Ids = Controller->GetFishIds();
			if (Ids.IsEmpty()) return false;
			const FPSFishDefinition* Fish = Controller->FindFishDefinition(Ids[0]);
			Controller->GetFishingJournalComponent()->RecordCatch(Ids[0], Fish->MaxSize, 1, Fish->MaxSize);
			Controller->HandleInventory();
			Controller->ToggleFishingJournal();
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UPSFishingJournalWidget::StaticClass(), true);
			if (Widgets.Num() != 1 || !Controller->IsPaused()) return false;
			auto* Journal = CastChecked<UPSFishingJournalWidget>(Widgets[0]);
			Journal->SelectFish(Ids[0]);
			auto* Name = Cast<UTextBlock>(Journal->GetWidgetFromName(TEXT("FishName")));
			auto* Star = Cast<UImage>(Journal->GetWidgetFromName(TEXT("FishStar")));
			bool bValid = Name && Name->GetText().ToString().Contains(Fish->Name.ToString()) && Star && Star->GetVisibility() == ESlateVisibility::HitTestInvisible;
			FWidgetRenderer Renderer(true);
			UTextureRenderTarget2D* Target = Renderer.DrawWidget(Journal->TakeWidget(), FVector2D(1280, 720));
			FlushRenderingCommands();
			FBufferArchive PNG;
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("FishingCameraSmoke");
			IFileManager::Get().MakeDirectory(*Directory, true);
			bValid &= Target && FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG)
				&& FFileHelper::SaveArrayToFile(PNG, *(Directory / TEXT("Journal.png")));
			Controller->CloseFishingJournal();
			bValid &= !Controller->IsPaused();
			UE_LOG(LogTemp, Display, TEXT("FISHING_JOURNAL_SMOKE %s species=%d"), bValid ? TEXT("PASS") : TEXT("FAIL"), Ids.Num());
			return bValid;
		}

		void Tick()
		{
			if (!World.IsValid() || FPlatformTime::Seconds() - Started > 35) { Finish(false); return; }
			APSPlayerController* Controller = Cast<APSPlayerController>(World->GetFirstPlayerController());
			if (!Controller || !Controller->GetPawn() || !Controller->PlayerCameraManager) return;
			APSGridWorld* Grid = nullptr;
			for (TActorIterator<APSGridWorld> It(World.Get()); It; ++It) { Grid = *It; break; }
			if (!Grid) return;
			// Capturing a frame can block wall time without advancing simulation.
			const double Elapsed = World->GetTimeSeconds() - PhaseStarted;
			if (Phase == 0)
			{
				Controller->SelectHotbarSlot(2);
				const FIntPoint Player = Grid->WorldToCell(Controller->GetPawn()->GetActorLocation());
				bool bFound = false;
				for (int32 DY = -2; DY <= 2 && !bFound; ++DY) for (int32 DX = -2; DX <= 2 && !bFound; ++DX)
				{
					Water = Player + FIntPoint(DX, DY);
					bFound = Grid->CanFishFrom(Player, Water);
				}
				if (!bFound || !CheckCentre(Controller, Controller->GetPawn()->GetActorLocation(), TEXT("initial-player"))
					|| !Controller->TryUseFishingRod(Water)) { Finish(false); return; }
				Phase = 1;
			}
			else if (Phase == 1 && Controller->GetFishingState() == EPSFishingState::CastTiming)
			{
				if (bLongCast && (Controller->GetFishingCastPosition() < .45f || Controller->GetFishingCastPosition() > .55f)) return;
				if (!Controller->ConfirmFishingCast()) { Finish(false); return; }
				UE_LOG(LogTemp, Display, TEXT("FISHING_CAMERA_SMOKE throw-grade=%d"), static_cast<int32>(Controller->GetFishingCastGrade()));
				bFlightCaptured = false;
				Phase = 2; PhaseStarted = World->GetTimeSeconds();
			}
			else if (Phase == 2 && !bFlightCaptured && Elapsed >= .15 && Elapsed < .6)
			{
				for (TActorIterator<APSFishingBobber> It(World.Get()); It; ++It)
				{
					UE_LOG(LogTemp, Display, TEXT("FISHING_CAMERA_SMOKE flight elapsed=%.3f position=%s"), Elapsed, *It->GetActorLocation().ToString());
					FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("FishingCameraSmoke/Flight.png"), true, false);
					bFlightCaptured = true; break;
				}
			}
			else if (Phase == 2 && Elapsed >= .6)
			{
				FVector Bobber;
				if (!Controller->GetFishingFocusLocation(Bobber) || !CheckCentre(Controller, Bobber, bLongCast ? TEXT("long-bobber") : TEXT("bobber")))
					{ Finish(false); return; }
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("FishingCameraSmoke/Bobber.png"), false, false);
				Phase = bLongCast ? 4 : 3;
				if (bLongCast && !Controller->CancelFishingForMovementInput()) { Finish(false); return; }
				PhaseStarted = World->GetTimeSeconds();
			}
			else if (Phase == 3 && Controller->GetFishingState() == EPSFishingState::BiteWindow)
			{
				if (Elapsed < .2) return;
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("FishingCameraSmoke/Bite.png"), true, false);
				Phase = 5; PhaseStarted = World->GetTimeSeconds();
			}
			else if (Phase == 5 && Elapsed >= .3)
			{
				if (!Controller->CancelFishingForMovementInput()) { Finish(false); return; }
				Phase = 4; PhaseStarted = World->GetTimeSeconds();
			}
			else if (Phase == 4 && Elapsed >= .6)
			{
				if (!CheckCentre(Controller, Controller->GetPawn()->GetActorLocation(), TEXT("returned-player"))) { Finish(false); return; }
				if (bLongCast) { Finish(CheckJournal(Controller)); return; }
				bLongCast = true;
				if (!Controller->TryUseFishingRod(Water)) { Finish(false); return; }
				Phase = 1; PhaseStarted = World->GetTimeSeconds();
			}
		}
	};

	FAutoConsoleCommandWithWorld CameraSmokeCommand(TEXT("PS.Fishing.CameraSmoke"),
		TEXT("Test real-map fishing camera projection with an isolated -UserDir; exits when done."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			FString UserDirectory;
			if (!World || !World->IsGameWorld() || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory) || UserDirectory.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("Camera smoke test requires a game world and an isolated -UserDir."));
				return;
			}
			const TSharedRef<FFishingCameraSmoke> State = MakeShared<FFishingCameraSmoke>();
			State->World = World;
			World->GetTimerManager().SetTimer(State->Timer, FTimerDelegate::CreateLambda([State]() { State->Tick(); }), .02f, true);
		}));
}
#endif
