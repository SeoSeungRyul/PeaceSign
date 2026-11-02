#if WITH_DEV_AUTOMATION_TESTS
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../Gathering/PSGatheringActionComponent.h"
#include "../World/PSGridWorld.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace
{
	struct FGatheringSmoke
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<APSGridWorld> Grid;
		FIntPoint Target;
		FTimerHandle Timer;
		int32 Phase = 0;
		double Started = FPlatformTime::Seconds();
		double CapturedAt = 0;
		FString Screenshot;
		void Finish(const bool bSuccess)
		{
			if (World.IsValid()) World->GetTimerManager().ClearTimer(Timer);
			UE_LOG(LogTemp, Display, TEXT("GATHERING_SMOKE %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"));
			FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
		}
		void Capture(const TCHAR* Name)
		{
			Screenshot = FPaths::ProjectSavedDir() / TEXT("Automation/Gathering") / Name;
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Screenshot), true);
			FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
			CapturedAt = FPlatformTime::Seconds();
		}
		void Tick()
		{
			if (!World.IsValid() || FPlatformTime::Seconds() - Started > 40) { Finish(false); return; }
			auto* Controller = Cast<APSPlayerController>(World->GetFirstPlayerController());
			auto* Pawn = Controller ? Cast<APSPlayerCharacter>(Controller->GetPawn()) : nullptr;
			if (!Pawn) return;
			if (Phase == 0)
			{
				for (TActorIterator<APSGridWorld> It(World.Get()); It; ++It) { Grid = *It; break; }
				if (!Grid.IsValid()) { Finish(false); return; }
				bool bFound = false;
				for (const auto& Tree : Grid->Gathering->GetRecords())
				{
					for (const FIntPoint Delta : {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)})
					{
						if (!Grid->Gathering->CanReach(Tree.Cell + Delta, Tree.Cell)) continue;
						Target = Tree.Cell;
						Pawn->SetActorLocation(Grid->CellToWorldCenter(Tree.Cell + Delta) + FVector(0, 0, 100));
						bFound = true; break;
					}
					if (bFound) break;
				}
				if (!bFound) { Finish(false); return; }
				Controller->SelectHotbarSlot(8);
				Grid->Gathering->RefreshActors();
				Phase = 1;
				CapturedAt = FPlatformTime::Seconds();
			}
			else if (Phase == 1 && FPlatformTime::Seconds() - CapturedAt > 1)
			{
				Capture(TEXT("Trees.png")); Phase = 2;
			}
			else if (Phase == 2 && !Controller->GatheringAction->IsGatheringActive())
			{
				if (Grid->Gathering->GetTree(Target).Health <= 0) { CapturedAt = FPlatformTime::Seconds(); Phase = 3; return; }
				Pawn->StatsComponent->Stamina = 100;
				if (!Controller->GatheringAction->HandleTree(Grid.Get(), Target) || !Controller->GatheringAction->IsGatheringActive()) { Finish(false); return; }
			}
			else if (Phase == 3 && FPlatformTime::Seconds() - CapturedAt > 1)
			{
				Capture(TEXT("StumpAndRewards.png")); Phase = 4;
			}
			else if (Phase == 4 && FPlatformTime::Seconds() - CapturedAt > 1 && IFileManager::Get().FileExists(*Screenshot))
			{
				const auto& Drops = Grid->Gathering->GetDrops();
				const bool bLogs = Drops.ContainsByPredicate([](const FPSGatheringDrop& Drop)
					{ return Drop.Item.ItemType == EPSItemType::Wood && Drop.Item.Quantity >= 3 && Drop.Item.Quantity <= 6; });
				Finish(!Pawn->IsMovementLocked() && Drops.Num() >= 1 && Drops.Num() <= 2 && bLogs);
			}
		}
	};
	FAutoConsoleCommandWithWorld GatheringSmokeCommand(TEXT("PS.Gathering.Smoke"), TEXT("Run gathering gameplay and screenshots with an isolated -UserDir."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			FString UserDir;
			if (!World || !World->IsGameWorld() || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty()) return;
			auto State = MakeShared<FGatheringSmoke>();
			State->World = World;
			World->GetTimerManager().SetTimer(State->Timer, FTimerDelegate::CreateLambda([State]() { State->Tick(); }), 0.05f, true);
		}));
}
#endif
