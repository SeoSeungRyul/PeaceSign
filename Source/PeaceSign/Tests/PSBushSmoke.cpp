#if WITH_DEV_AUTOMATION_TESTS
#include "../Gathering/PSBushWorldComponent.h"
#include "../Gathering/PSBushInteractionComponent.h"
#include "../World/PSGridWorld.h"
#include "../PSPlayerController.h"
#include "../PSPlayerCharacter.h"
#include "../Inventory/PSInventoryComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	struct FBushSmoke
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<APSGridWorld> Grid;
		TWeakObjectPtr<AStaticMeshActor> BushVisual;
		FTimerHandle Timer;
		FIntPoint Cell;
		FRotator RestRotation;
		int32 Phase = 0;
		double Started = FPlatformTime::Seconds();
		double PhaseAt = 0;
		void Finish(const bool bSuccess)
		{
			World->GetTimerManager().ClearTimer(Timer);
			UE_LOG(LogTemp, Display, TEXT("BUSH_SMOKE %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"));
			FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
		}
		void Capture(const TCHAR* Name)
		{
			IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir() / TEXT("Automation/Bushes")), true);
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/Bushes") / Name, true, false);
		}
		void Tick()
		{
			if (!World.IsValid()) return;
			if (FPlatformTime::Seconds() - Started > 40) { Finish(false); return; }
			auto* Controller = Cast<APSPlayerController>(World->GetFirstPlayerController());
			auto* Player = Controller ? Cast<APSPlayerCharacter>(Controller->GetPawn()) : nullptr;
			if (!Player) return;
			if (Phase == 0)
			{
				for (TActorIterator<APSGridWorld> It(World.Get()); It; ++It) { Grid = *It; break; }
				if (!Grid.IsValid() || !Grid->Bushes->Texture || Grid->Bushes->GetRecords().IsEmpty()) { Finish(false); return; }
				Cell = Grid->Bushes->GetRecords()[0].Cell;
				for (const auto& Bush : Grid->Bushes->GetRecords())
					if (Grid->GetGroundTile(Bush.Cell) != EPSTileType::Grass) { Finish(false); return; }
				Player->SetActorLocation(Grid->CellToWorldCenter(Cell + FIntPoint(-1, 0)) + FVector(0, 0, 100));
				// Find an actual land approach, avoiding water and stones.
				for (const FIntPoint Side : {FIntPoint(-1,0), FIntPoint(1,0), FIntPoint(0,-1), FIntPoint(0,1)})
				{
					if (Grid->GetGroundTile(Cell + Side) != EPSTileType::Grass || Grid->GetWorldObjectType(Cell + Side) != EPSWorldObjectType::None) continue;
					Player->SetActorLocation(Grid->CellToWorldCenter(Cell + Side) + FVector(0, 0, 100));
					if (Grid->Bushes->CanReach(Player, Cell)) break;
				}
				PhaseAt = FPlatformTime::Seconds();
				Phase = 1;
			}
			else if (Phase == 1 && FPlatformTime::Seconds() - PhaseAt > 1)
			{
				Capture(TEXT("GrassBush.png"));
				const FVector VisualLocation = Grid->CellToWorldCenter(Cell) + FVector(0, 0, Grid->GetCellSize() * 0.7f);
				for (TActorIterator<AStaticMeshActor> It(World.Get()); It; ++It)
					if (It->GetActorLocation().Equals(VisualLocation)) { BushVisual = *It; break; }
				if (!BushVisual.IsValid()) { Finish(false); return; }
				RestRotation = BushVisual->GetActorRotation();
				auto* Interaction = Controller->BushInteraction.Get();
				if (!Interaction->Press(Grid.Get(), true, Cell) || !Interaction->IsActive()) { Finish(false); return; }
				Interaction->Advance(0.1f);
				const bool bShook = !BushVisual->GetActorRotation().Equals(RestRotation, 0.5f);
				Interaction->Release(true);
				const bool bReset = BushVisual->GetActorRotation().Equals(RestRotation);
				UE_LOG(LogTemp, Display, TEXT("BUSH_SHAKE moved=%d reset_on_cancel=%d"), bShook, bReset);
				if (!bShook || !bReset || !Interaction->Press(Grid.Get(), true, Cell) || !Interaction->IsActive())
				{
					Finish(false);
					return;
				}
				PhaseAt = FPlatformTime::Seconds();
				Phase = 2;
			}
			else if (Phase == 2 && FPlatformTime::Seconds() - PhaseAt > 0.35)
			{
				Capture(TEXT("BushHarvestProgress.png"));
				Phase = 3;
			}
			else if (Phase == 3 && !Controller->BushInteraction->IsActive())
			{
				Controller->BushInteraction->Release(true);
				const int32 Quantity = Controller->GetInventoryComponent()->CountItem(EPSItemType::GatheredFruit);
				UE_LOG(LogTemp, Display, TEXT("BUSH_REWARD quantity=%d"), Quantity);
				Capture(TEXT("BushHarvested.png"));
				PhaseAt = FPlatformTime::Seconds();
				Phase = 4;
			}
			else if (Phase == 4 && FPlatformTime::Seconds() - PhaseAt > 1)
			{
				const int32 Quantity = Controller->GetInventoryComponent()->CountItem(EPSItemType::GatheredFruit);
				const auto* Bush = Grid->Bushes->Find(Cell);
				Finish(Bush && Bush->RemainingHarvests == 2 && Quantity >= 1 && Quantity <= 3
					&& BushVisual.IsValid() && BushVisual->GetActorRotation().Equals(RestRotation)
					&& Controller->GetInventoryComponent()->GetDiscoveredGatheringItems().Num() == 1);
			}
		}
	};
	FAutoConsoleCommandWithWorld BushSmoke(TEXT("PS.Bush.Smoke"), TEXT("Test the grass bush in an isolated -UserDir."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			FString UserDir;
			if (!World || !World->IsGameWorld() || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir)) return;
			auto State = MakeShared<FBushSmoke>();
			State->World = World;
			World->GetTimerManager().SetTimer(State->Timer, FTimerDelegate::CreateLambda([State]() { State->Tick(); }), 0.025f, true);
		}));
}
#endif
