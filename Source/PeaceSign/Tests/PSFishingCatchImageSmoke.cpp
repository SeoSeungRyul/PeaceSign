#if WITH_DEV_AUTOMATION_TESTS
#include "../PSPlayerController.h"
#include "../Fishing/PSFishingBobber.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	// Exercise first and cached presentations in a real viewport, using isolated saves.
	struct FFishingCatchImageSmoke
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<APSFishingBobber> First;
		TWeakObjectPtr<APSFishingBobber> Second;
		FTimerHandle Timer;
		double Started = FPlatformTime::Seconds();
		double PhaseStarted = Started;
		double FirstScale = 0;
		int32 Phase = 0;
		FString Screenshot;

		void Finish(const bool bSuccess)
		{
			if (World.IsValid()) World->GetTimerManager().ClearTimer(Timer);
			if (First.IsValid()) First->Destroy();
			if (Second.IsValid()) Second->Destroy();
			UE_LOG(LogTemp, Display, TEXT("FISHING_CATCH_IMAGE_SMOKE %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"));
			FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
		}

		void Tick()
		{
			if (!World.IsValid() || FPlatformTime::Seconds() - Started > 20) { Finish(false); return; }
			auto* Controller = Cast<APSPlayerController>(World->GetFirstPlayerController());
			if (!Controller || !Controller->GetPawn() || !Controller->PlayerCameraManager) return;
			const double Elapsed = FPlatformTime::Seconds() - PhaseStarted;
			if (Phase == 0)
			{
				const TArray<FName> Ids = Controller->GetFishIds();
				if (Ids.IsEmpty()) { Finish(false); return; }
				UTexture2D* Texture = Controller->GetFishIcon(Ids[0]);
				if (!Texture) { Finish(false); return; }
				First = World->SpawnActor<APSFishingBobber>();
				if (!First.IsValid()) { Finish(false); return; }
				First->ShowCatch(Texture);
				FirstScale = First->CatchImage->GetComponentScale().GetMax();
				if (!FMath::IsNearlyEqual(FirstScale * FMath::Max(Texture->GetSizeX(), Texture->GetSizeY()),
					static_cast<double>(First->CatchImageWorldSize), .01)) { Finish(false); return; }
				const FRotator View = Controller->PlayerCameraManager->GetCameraRotation();
				First->SetActorLocation(Controller->GetPawn()->GetActorLocation() + View.RotateVector(FVector::UpVector) * 70
					- View.RotateVector(FVector::RightVector) * 45 + FVector(0, 0, 25));
				Phase = 1; PhaseStarted = FPlatformTime::Seconds();
			}
			else if (Phase == 1 && Elapsed >= .25)
			{
				const TArray<FName> Ids = Controller->GetFishIds();
				Second = World->SpawnActor<APSFishingBobber>();
				if (!Second.IsValid()) { Finish(false); return; }
				Second->ShowCatch(Controller->GetFishIcon(Ids[0]));
				const double SecondScale = Second->CatchImage->GetComponentScale().GetMax();
				if (!FMath::IsNearlyEqual(FirstScale, SecondScale, .00001)) { Finish(false); return; }
				const FRotator View = Controller->PlayerCameraManager->GetCameraRotation();
				Second->SetActorLocation(First->GetActorLocation() + View.RotateVector(FVector::RightVector) * 90);
				UE_LOG(LogTemp, Display, TEXT("FISHING_CATCH_IMAGE_SMOKE firstScale=%.6f secondScale=%.6f"), FirstScale, SecondScale);
				Screenshot = FPaths::ProjectSavedDir() / TEXT("FishingCatchSmoke/FirstAndSecond.png");
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Screenshot), true);
				FScreenshotRequest::RequestScreenshot(Screenshot, false, false);
				Phase = 2; PhaseStarted = FPlatformTime::Seconds();
			}
			else if (Phase == 2 && Elapsed >= .3 && IFileManager::Get().FileExists(*Screenshot)) Finish(true);
		}
	};

	FAutoConsoleCommandWithWorld CatchImageSmokeCommand(TEXT("PS.Fishing.CatchImageSmoke"),
		TEXT("Compare first and repeated overhead catch images using an isolated -UserDir; exits when done."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			FString UserDirectory;
			if (!World || !World->IsGameWorld() || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory) || UserDirectory.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("Catch image smoke test requires a game world and an isolated -UserDir."));
				return;
			}
			const TSharedRef<FFishingCatchImageSmoke> State = MakeShared<FFishingCatchImageSmoke>();
			State->World = World;
			World->GetTimerManager().SetTimer(State->Timer, FTimerDelegate::CreateLambda([State]() { State->Tick(); }), .02f, true);
		}));
}
#endif
