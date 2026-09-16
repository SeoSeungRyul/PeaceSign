#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../UI/PSInventoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Slate/WidgetRenderer.h"
#include "RenderingThread.h"
#include "TextureResource.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSInventoryPreviewTest, "PeaceSign.Inventory.Preview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSInventoryPreviewTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	UPSInventoryWidget* Widget = CreateWidget<UPSInventoryWidget>(World);
	if (!TestNotNull(TEXT("Inventory creates"), Widget)) { World->DestroyWorld(false); return false; }
	// No local player exists in this isolated automation world.
	if (!Widget->WidgetTree->RootWidget) Widget->NativeOnInitialized();
	TestEqual(TEXT("Ten sample slots initialized"), Widget->PreviewItems.Num(), 10);
	const FText FirstName = Widget->GetItem(0)->Name;
	const FText SecondName = Widget->GetItem(1)->Name;
	TestTrue(TEXT("Move to empty slot"), Widget->MoveItem(0, 9));
	TestNull(TEXT("Source cleared only after move"), Widget->GetItem(0));
	TestTrue(TEXT("Destination retains item"), Widget->GetItem(9)->Name.EqualTo(FirstName));
	TestFalse(TEXT("Locked destination rejects move"), Widget->MoveItem(9, 10));
	TestFalse(TEXT("Outside grid rejects move"), Widget->MoveItem(9, -1));
	TestTrue(TEXT("Rejected moves preserve source"), Widget->GetItem(9)->Name.EqualTo(FirstName));
	TestTrue(TEXT("Occupied slots swap"), Widget->MoveItem(9, 1));
	TestTrue(TEXT("Swap destination"), Widget->GetItem(1)->Name.EqualTo(FirstName));
	TestTrue(TEXT("Swap source"), Widget->GetItem(9)->Name.EqualTo(SecondName));
	TestEqual(TEXT("Swap retains stack count"), Widget->GetItem(9)->Quantity, 24);
	TestFalse(TEXT("Cannot move an empty slot"), Widget->MoveItem(0, 2));
	// Restore the initial layout before taking visual review images.
	Widget->MoveItem(1, 9);
	Widget->MoveItem(9, 0);
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
