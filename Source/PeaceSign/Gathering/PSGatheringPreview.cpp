#include "../Inventory/PSInventoryComponent.h"

#if !UE_BUILD_SHIPPING
#include "../PSPlayerController.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	FAutoConsoleCommandWithWorld GatheringPreviewCommand(TEXT("PS.Gathering.PreviewItems"),
		TEXT("Add missing logs/seeds, expand required bag rows and open inventory. Development only."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			auto* Controller = World && World->IsGameWorld() ? Cast<APSPlayerController>(World->GetFirstPlayerController()) : nullptr;
			auto* Inventory = Controller ? Controller->GetInventoryComponent() : nullptr;
			if (Inventory)
			{
				int32 Occupied = 0;
				for (int32 Index = 0; Index < Inventory->GetUnlockedBagSlotCount(); ++Index)
					Occupied += Inventory->GetBagSlot(Index).IsEmpty() ? 0 : 1;
				UE_LOG(LogTemp, Display, TEXT("GATHERING_PREVIEW_BEFORE occupied=%d bagSlots=%d"), Occupied, Inventory->GetUnlockedBagSlotCount());
			}
			if (!Inventory || !Inventory->GrantGatheringPreviewItems())
			{
				UE_LOG(LogTemp, Warning, TEXT("GATHERING_PREVIEW_FAILED: start Play first, or free bag space; no items replaced."));
				return;
			}
			Controller->CloseInventory();
			Controller->HandleInventory();
			UE_LOG(LogTemp, Display, TEXT("GATHERING_PREVIEW_READY bagSlots=%d; twelve log/seed variants available."), Inventory->GetUnlockedBagSlotCount());
			if (FParse::Param(FCommandLine::Get(), TEXT("PSGatheringPreviewCapture")))
			{
				// Core ticker continues while the inventory pauses the game world.
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
				{
					FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("GatheringPreviewInGame.png"), true, false);
					return false;
				}), 3.0f);
			}
		}));
}
#endif
