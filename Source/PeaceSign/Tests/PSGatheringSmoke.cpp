#if WITH_DEV_AUTOMATION_TESTS
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../Gathering/PSGatheringActionComponent.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSWorldItemActor.h"
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
#include "Camera/CameraComponent.h"
#include "PaperSpriteComponent.h"
#include "../Gathering/PSGatheringTreeActor.h"

namespace
{
	struct FGatheringSmoke
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<APSGridWorld> Grid;
		FIntPoint Target;
		FVector ActionLocation;
		bool bArtPreview = FParse::Param(FCommandLine::Get(), TEXT("PSTreeSpritesOverview"));
		bool bMovedToTree = false;
		FTimerHandle Timer;
		int32 Phase = 0;
		int32 DroppedLogs = 0;
		int32 DroppedSeeds = 0;
		TArray<FVector> PickupLocations;
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
						ActionLocation = Grid->CellToWorldCenter(Tree.Cell + Delta) + FVector(0, 0, 100);
						if (!bArtPreview) Pawn->SetActorLocation(ActionLocation);
						bFound = true; break;
					}
					if (bFound) break;
				}
				if (!bFound) { Finish(false); return; }
				Controller->SelectHotbarSlot(8);
				Grid->Gathering->RefreshActors();
				if (bArtPreview)
				{
					int32 Placed = 0;
					FVector Center = FVector::ZeroVector;
					for (const auto& Tree : Grid->Gathering->GetRecords())
						if (Tree.PlacementId.ToString().StartsWith(TEXT("Showcase.")))
						{
							++Placed; Center += Grid->CellToWorldCenter(Tree.Cell);
							bool bReachable = false;
							for (const FIntPoint Delta : {FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
								bReachable |= Grid->GetWorldObjectType(Tree.Cell+Delta)==EPSWorldObjectType::None
									&& Grid->Gathering->CanReach(Tree.Cell+Delta,Tree.Cell,Pawn);
							if (!bReachable) { UE_LOG(LogTemp,Error,TEXT("TREE_GROVE_NO_APPROACH %s"),*Tree.PlacementId.ToString()); Finish(false); return; }
						}
					if (Placed != 6) { Finish(false); return; }
					Center /= 6;
					int32 Sprites = 0;
					for (TActorIterator<APSGatheringTreeActor> It(World.Get()); It; ++It)
						if (auto* Sprite = It->FindComponentByClass<UPaperSpriteComponent>()) Sprites += Sprite->GetSprite() ? 1 : 0;
					if (Sprites < 6) { Finish(false); return; }
					if (auto* Camera = Pawn->FindComponentByClass<UCameraComponent>())
					{
						Center.X += Grid->GetCellSize()*2;
						Center.Z = Camera->GetComponentLocation().Z;
						Camera->SetAbsolute(true,false,false); Camera->SetWorldLocation(Center); Camera->SetOrthoWidth(2400);
						if (FVector::DotProduct(Camera->GetForwardVector(), -FVector::UpVector)<.99f) { Finish(false); return; }
					}
				}
				Phase = 1;
				CapturedAt = FPlatformTime::Seconds();
			}
			else if (Phase == 1 && FPlatformTime::Seconds() - CapturedAt > 1)
			{
				Capture(TEXT("Trees.png")); Phase = 2;
			}
			else if (Phase == 2 && !Controller->GatheringAction->IsGatheringActive())
			{
				if (bArtPreview && !bMovedToTree) { Pawn->SetActorLocation(ActionLocation); bMovedToTree = true; }
				if (Grid->Gathering->GetTree(Target).Health <= 0) { CapturedAt = FPlatformTime::Seconds(); Phase = 3; return; }
				Pawn->StatsComponent->Stamina = 100;
				if (!Controller->GatheringAction->HandleTree(Grid.Get(), Target) || !Controller->GatheringAction->IsGatheringActive()) { Finish(false); return; }
			}
			else if (Phase == 3 && FPlatformTime::Seconds() - CapturedAt > 1)
			{
				if (Grid->Gathering->OccupiesTreeCell(Target)) { Finish(false); return; }
				Capture(TEXT("ClearedTreeAndRewards.png")); Phase = 4;
			}
			else if (Phase == 4 && FPlatformTime::Seconds() - CapturedAt > 1 && IFileManager::Get().FileExists(*Screenshot))
			{
				const auto& Drops = Grid->Gathering->GetDrops();
				for (const auto& Drop : Drops)
				{
					if (Drop.Item.ItemType == EPSItemType::Wood) DroppedLogs += Drop.Item.Quantity;
					if (Drop.Item.ItemType == EPSItemType::TreeSeed) DroppedSeeds += Drop.Item.Quantity;
					if (!Grid->Gathering->GetItemIcon(Drop.Item)) { Finish(false); return; }
				}
				if (Pawn->IsMovementLocked() || Drops.Num() > 2 || DroppedLogs < 3 || DroppedLogs > 6)
					{ Finish(false); return; }
				for (TActorIterator<APSWorldItemActor> It(World.Get()); It; ++It)
					PickupLocations.Add(It->GetActorLocation());
				if (PickupLocations.Num() != Drops.Num()) { Finish(false); return; }
				Phase = 5;
				CapturedAt = FPlatformTime::Seconds();
			}
			else if (Phase == 5 && FPlatformTime::Seconds() - CapturedAt > 0.5)
			{
				if (!PickupLocations.IsEmpty())
				{
					FVector Location = PickupLocations.Pop();
					Location.Z = Pawn->GetActorLocation().Z;
					Pawn->SetActorLocation(Location);
					CapturedAt = FPlatformTime::Seconds();
					return;
				}
				const auto* Inventory = Controller->GetInventoryComponent();
				const int32 Logs = Inventory->CountItem(EPSItemType::Wood);
				const int32 Seeds = Inventory->CountItem(EPSItemType::TreeSeed);
				UE_LOG(LogTemp, Display, TEXT("GATHERING_PICKUP logs=%d/%d seeds=%d/%d"), Logs, DroppedLogs, Seeds, DroppedSeeds);
				Finish(Grid->Gathering->GetDrops().IsEmpty() && Logs == DroppedLogs && Seeds == DroppedSeeds);
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
