#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../UI/PSInventoryWidget.h"
#include "../UI/PSHotbarWidget.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Inventory/PSInventorySaveGame.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Slate/WidgetRenderer.h"
#include "RenderingThread.h"
#include "TextureResource.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSInventoryPreviewTest, "PeaceSign.Inventory.DataAndUI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSInventoryPreviewTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	UPSInventoryComponent* Inventory = NewObject<UPSInventoryComponent>(World);
	Inventory->bAutoSave = false;
	Inventory->SaveSlotName = TEXT("InventoryTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Inventory->InitializeDefaults();
	UPSInventoryWidget* Widget = CreateWidget<UPSInventoryWidget>(World);
	if (!TestNotNull(TEXT("Inventory creates"), Widget)) { World->DestroyWorld(false); return false; }
	// No local player exists in this isolated automation world.
	if (!Widget->WidgetTree->RootWidget) Widget->NativeOnInitialized();
	Widget->SetInventoryComponent(Inventory);
	UPSHotbarWidget* Hotbar = CreateWidget<UPSHotbarWidget>(World);
	if (!TestNotNull(TEXT("Always-visible hotbar creates"), Hotbar)) { World->DestroyWorld(false); return false; }
	if (!Hotbar->WidgetTree->RootWidget) Hotbar->NativeOnInitialized();
	Hotbar->SetInventoryComponent(Inventory);
	TestNotNull(TEXT("Hotbar builds its visual tree"), Hotbar->WidgetTree->RootWidget.Get());
	UCanvasPanel* HotbarCanvas = Cast<UCanvasPanel>(Hotbar->WidgetTree->RootWidget);
	TestNotNull(TEXT("Hotbar uses a full-screen positioning canvas"), HotbarCanvas);
	UCanvasPanelSlot* HotbarFrameSlot = HotbarCanvas && HotbarCanvas->GetChildrenCount() > 0
		? Cast<UCanvasPanelSlot>(HotbarCanvas->GetChildAt(0)->Slot) : nullptr;
	TestNotNull(TEXT("Hotbar frame has an anchored canvas slot"), HotbarFrameSlot);
	if (HotbarFrameSlot)
	{
		TestEqual(TEXT("Hotbar is anchored to the bottom center"), HotbarFrameSlot->GetAnchors().Minimum, FVector2D(0.5f, 1.0f));
		TestEqual(TEXT("Hotbar touches the bottom edge"), HotbarFrameSlot->GetPosition(), FVector2D::ZeroVector);
	}
	const FPSItemStack* FirstHotbarItem = Hotbar->GetItem(0);
	TestNotNull(TEXT("Hotbar displays its first physical item"), FirstHotbarItem);
	if (FirstHotbarItem) TestEqual(TEXT("First physical hotbar item is the hoe"), FirstHotbarItem->ItemType, EPSItemType::Hoe);
	TestEqual(TEXT("Hotbar owns ten independent slots"), Inventory->HotbarSlots.Num(), UPSInventoryComponent::HotbarSlotCount);
	TestEqual(TEXT("Bag owns fifty expandable slots"), Inventory->BagSlots.Num(), UPSInventoryComponent::MaxBagSlotCount);
	TestEqual(TEXT("Ten bag slots start unlocked"), Inventory->GetUnlockedBagSlotCount(), 10);
	TestEqual(TEXT("Starter hoe is real inventory data"), Inventory->CountItem(EPSItemType::Hoe), 1);
	TestEqual(TEXT("Starter seeds are real inventory data"), Inventory->CountItem(EPSItemType::TestSeed), 24);
	TestEqual(TEXT("Starter fishing rod remains usable from hotbar slot three"), Inventory->GetHotbarSlot(2).ItemType, EPSItemType::FishingRod);
	TestNull(TEXT("Initial bag is independent and empty"), Widget->GetItem(0));
	TestTrue(TEXT("Hotbar item can move into bag"), Inventory->MoveItem(
		EPSInventoryArea::Hotbar, 0, EPSInventoryArea::Bag, 0));
	const EPSItemType FirstType = Widget->GetItem(0)->ItemType;
	TestTrue(TEXT("Move to empty slot"), Widget->MoveItem(0, 9));
	TestNull(TEXT("Source cleared only after move"), Widget->GetItem(0));
	TestEqual(TEXT("Destination retains item"), Widget->GetItem(9)->ItemType, FirstType);
	TestFalse(TEXT("Locked destination rejects move"), Widget->MoveItem(9, 10));
	TestFalse(TEXT("Outside grid rejects move"), Widget->MoveItem(9, -1));
	TestEqual(TEXT("Rejected moves preserve source"), Widget->GetItem(9)->ItemType, FirstType);
	TestTrue(TEXT("Seed can move from hotbar into bag"), Inventory->MoveItem(
		EPSInventoryArea::Hotbar, 1, EPSInventoryArea::Bag, 1));
	TestTrue(TEXT("Occupied bag slots swap"), Widget->MoveItem(9, 1));
	TestEqual(TEXT("Swap destination"), Widget->GetItem(1)->ItemType, FirstType);
	TestEqual(TEXT("Swap source"), Widget->GetItem(9)->ItemType, EPSItemType::TestSeed);
	TestEqual(TEXT("Swap retains stack count"), Widget->GetItem(9)->Quantity, 24);
	TestFalse(TEXT("Cannot move an empty slot"), Widget->MoveItem(0, 2));

	Inventory->InitializeDefaults();
	Inventory->HotbarSlots[1].Quantity = 990;
	Inventory->BagSlots[0].ItemType = EPSItemType::TestSeed;
	Inventory->BagSlots[0].CropId = 0;
	Inventory->BagSlots[0].Quantity = 20;
	TestTrue(TEXT("Equal items merge across bag and hotbar"), Inventory->MoveItem(
		EPSInventoryArea::Bag, 0, EPSInventoryArea::Hotbar, 1));
	TestEqual(TEXT("Merge fills hotbar destination to max stack"), Inventory->GetHotbarSlot(1).Quantity, 999);
	TestEqual(TEXT("Merge leaves overflow in bag source"), Inventory->GetBagSlot(0).Quantity, 11);
	TestTrue(TEXT("Remove item spans multiple stacks"), Inventory->RemoveItem(EPSItemType::TestSeed, 1009));
	TestEqual(TEXT("Remove item keeps the remaining amount"), Inventory->CountItem(EPSItemType::TestSeed), 1);
	TestFalse(TEXT("Failed removal does not change inventory"), Inventory->RemoveItem(EPSItemType::TestSeed, 2));
	TestEqual(TEXT("Failed removal preserves amount"), Inventory->CountItem(EPSItemType::TestSeed), 1);
	TestTrue(TEXT("Items can be added atomically"), Inventory->AddItem(EPSItemType::Wood, 999));
	TestEqual(TEXT("Added items enter bag instead of hotbar"), Inventory->GetBagSlot(0).ItemType, EPSItemType::Wood);
	TestFalse(TEXT("Invalid quantity is rejected"), Inventory->AddItem(EPSItemType::Wood, 0));
	TestTrue(TEXT("A different crop seed can be added"), Inventory->AddItem(EPSItemType::TestSeed, 4, 1));
	TestEqual(TEXT("Crop-specific count stays separate"), Inventory->CountItem(EPSItemType::TestSeed, 1), 4);
	const int32 CropOneSlot = Inventory->BagSlots.IndexOfByPredicate([](const FPSItemStack& Slot)
	{
		return Slot.ItemType == EPSItemType::TestSeed && Slot.CropId == 1;
	});
	TestTrue(TEXT("Different crop seeds do not merge"), CropOneSlot != INDEX_NONE);
	TestTrue(TEXT("Only hotbar slots can be consumed by selected-slot farming"),
		Inventory->MoveItem(EPSInventoryArea::Bag, CropOneSlot, EPSInventoryArea::Hotbar, 4));
	TestTrue(TEXT("A selected hotbar stack can consume one item"), Inventory->RemoveFromHotbarSlot(4, 1));
	TestEqual(TEXT("Selected hotbar consumption is isolated"), Inventory->CountItem(EPSItemType::TestSeed, 1), 3);

	Inventory->InitializeDefaults();
	TestTrue(TEXT("Explicit inventory save succeeds"), Inventory->SaveInventory());
	UPSInventoryComponent* LoadedInventory = NewObject<UPSInventoryComponent>(World);
	LoadedInventory->bAutoSave = false;
	LoadedInventory->SaveSlotName = Inventory->SaveSlotName;
	TestTrue(TEXT("Inventory save reloads"), LoadedInventory->LoadInventory());
	TestEqual(TEXT("Saved seeds reload"), LoadedInventory->CountItem(EPSItemType::TestSeed), 24);
	TestEqual(TEXT("Saved hotbar seed keeps crop ID"), LoadedInventory->GetHotbarSlot(1).CropId, 0);
	TestTrue(TEXT("Saved bag remains separate"), LoadedInventory->GetBagSlot(0).IsEmpty());
	UPSInventorySaveGame* LegacySave = NewObject<UPSInventorySaveGame>();
	LegacySave->Slots.SetNum(UPSInventoryComponent::MaxBagSlotCount);
	for (int32 Index = 0; Index < UPSInventoryComponent::HotbarSlotCount; ++Index)
		LegacySave->Slots[Index] = Inventory->HotbarSlots[Index];
	LegacySave->Slots[2].Clear();
	LegacySave->UnlockedSlotCount = 10;
	LegacySave->DataVersion = 0;
	TestTrue(TEXT("Legacy inventory fixture saves"), UGameplayStatics::SaveGameToSlot(LegacySave, Inventory->SaveSlotName, 0));
	TestTrue(TEXT("Legacy inventory reloads"), LoadedInventory->LoadInventory());
	TestEqual(TEXT("Legacy first row migrates to hotbar"), LoadedInventory->GetHotbarSlot(0).ItemType, EPSItemType::Hoe);
	TestEqual(TEXT("Legacy fixed-key fishing migrates a rod to hotbar slot three"), LoadedInventory->GetHotbarSlot(2).ItemType, EPSItemType::FishingRod);
	TestTrue(TEXT("Legacy migration creates a separate empty bag"), LoadedInventory->GetBagSlot(0).IsEmpty());
	UGameplayStatics::DeleteGameInSlot(Inventory->SaveSlotName, 0);

	// Restore the initial layout before taking visual review images.
	Inventory->InitializeDefaults();
	Widget->SelectItem(0);
	if (FParse::Param(FCommandLine::Get(), TEXT("PSInventoryRender")))
	{
		auto* Renderer = new FWidgetRenderer(true);
		for (const FIntPoint Size : {FIntPoint(1280, 800), FIntPoint(800, 600)})
		{
			UTextureRenderTarget2D* Target = Renderer->DrawWidget(Widget->TakeWidget(), FVector2D(Size));
			FlushRenderingCommands();
			TArray<FColor> Pixels;
			FReadSurfaceDataFlags Flags;
			Flags.SetLinearToGamma(false); // Slate already applied display gamma.
			if (TestTrue(TEXT("UMG image rendered"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
			{
				TArray64<uint8> Bytes;
				FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Bytes);
				const FString Filename = FPaths::ProjectSavedDir() / (Size.X == 1280 ? TEXT("InventoryPreview.png") : TEXT("InventoryPreviewSmall.png"));
				TestTrue(TEXT("UMG image saved"), FFileHelper::SaveArrayToFile(Bytes, *Filename));
			}
		}
		BeginCleanup(Renderer);
		FlushRenderingCommands();
	}
	Widget->ReleaseSlateResources(true);
	World->DestroyWorld(false);
	return true;
}
#endif
