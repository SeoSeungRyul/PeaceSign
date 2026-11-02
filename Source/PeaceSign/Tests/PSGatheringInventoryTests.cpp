#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerController.h"
#include "../Data/PSGameDataTables.h"
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../World/PSGridWorld.h"
#include "../UI/PSInventoryWidget.h"
#include "../UI/PSHotbarWidget.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Slate/WidgetRenderer.h"
#include "RenderingThread.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSGatheringInventoryTest, "PeaceSign.Gathering.InventoryIconsAndPickup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSGatheringInventoryTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	const FString Slot = TEXT("GatheringInventory_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot;
	Grid->FinishSpawning(FTransform::Identity);
	auto* Gathering = Grid->Gathering.Get();
	Gathering->ItemTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataBase/DT_Item.DT_Item"));
	Gathering->ItemIconTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataBase/DT_GatheringIcon.DT_GatheringIcon"));
	Gathering->TreeCutDropTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataBase/DT_TreeCutDrop.DT_TreeCutDrop"));
	Gathering->TreeShakeDropTable = NewObject<UDataTable>(World);
	Gathering->TreeShakeDropTable->RowStruct = FPSTreeShakeDropRow::StaticStruct();
	auto* Controller = World->SpawnActor<APSPlayerController>();
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->Player->PlayerController = Controller;
	World->AddController(Controller);
	Controller->GridWorld = Grid;
	auto* Inventory = Controller->GetInventoryComponent();
	Inventory->bAutoSave = false;
	Inventory->SaveSlotName = Slot + TEXT("Inventory");
	TSet<UTexture2D*> UniqueIcons;
	for (int32 Species = 0; Species < 6; ++Species)
	{
		Inventory->ResetToDefaults();
		const auto Definition = PSGathering::GetTree(static_cast<EPSTreeSpecies>(Species));
		const FName SeedID(*FString::Printf(TEXT("03020%d"), Species + 1));
		const FName LogID(*FString::Printf(TEXT("03010%d"), Species + 1));
		// Guarantee a seed for pickup coverage; probability selection is tested separately.
		FPSTreeShakeDropRow SeedRow;
		SeedRow.ID = Definition.ShakeDropID; SeedRow.ItemID = SeedID; SeedRow.Count = 1; SeedRow.DropRate = 1;
		Gathering->TreeShakeDropTable->EmptyTable();
		Gathering->TreeShakeDropTable->AddRow(TEXT("GuaranteedSeed"), SeedRow);
		FPSTreeRecord Tree; Tree.Cell = FIntPoint(6, 6); Tree.Species = static_cast<EPSTreeSpecies>(Species); Tree.Health = 10;
		Gathering->Restore({Tree}, {}, true, true);
		TestTrue(TEXT("Shaking creates inventory-ready seed"), Gathering->Shake(Tree.Cell));
		const auto SeedDrops = Gathering->GetDrops();
		if (!TestEqual(TEXT("One seed reward"), SeedDrops.Num(), 1)) continue;
		TestTrue(TEXT("Seed picked up into bag"), Gathering->CollectDrop(SeedDrops[0].Id, Inventory));
		TestFalse(TEXT("Already collected reward cannot be duplicated"), Gathering->CollectDrop(SeedDrops[0].Id, Inventory));
		TestEqual(TEXT("Seed keeps species ID"), Inventory->GetBagSlot(0).ItemId, SeedID);
		TestTrue(TEXT("CSV-backed felling creates rewards"), Gathering->Strike(Tree.Cell, PSGathering::GetAxe(PSGathering::AxeId(1))));
		const auto CutDrops = Gathering->GetDrops();
		for (const auto& Drop : CutDrops) TestTrue(TEXT("Felling reward enters bag"), Gathering->CollectDrop(Drop.Id, Inventory));
		TestTrue(TEXT("All felling drops collected"), Gathering->GetDrops().IsEmpty());
		int32 LogSlot = INDEX_NONE;
		for (int32 Index = 0; Index < Inventory->GetUnlockedBagSlotCount(); ++Index)
			if (Inventory->GetBagSlot(Index).ItemId == LogID) { LogSlot = Index; break; }
		if (TestTrue(TEXT("Log and seed use distinct inventory stacks"), LogSlot > 0))
		{
			for (const FPSItemStack Stack : {Inventory->GetBagSlot(0), Inventory->GetBagSlot(LogSlot)})
			{
				UTexture2D* Icon = Controller->GetItemIcon(Stack);
				if (TestNotNull(TEXT("Gathering stack resolves its inventory icon"), Icon))
				{
					// A cold editor load temporarily reports the 32px compilation placeholder.
#if WITH_EDITOR
					FTextureCompilingManager::Get().FinishCompilation({Icon});
#endif
					UniqueIcons.Add(Icon);
					TestEqual(TEXT("UI icon width"), Icon->GetSizeX(), 256);
					TestEqual(TEXT("UI icon height"), Icon->GetSizeY(), 256);
					TestEqual(TEXT("Texture uses UI group"), static_cast<int32>(Icon->LODGroup), static_cast<int32>(TEXTUREGROUP_UI));
				}
			}
		}
		const FPSItemStack SavedSeed = Inventory->GetBagSlot(0);
		TestTrue(TEXT("Seed moves from bag to hotbar"), Inventory->MoveItem(EPSInventoryArea::Bag, 0, EPSInventoryArea::Hotbar, 9));
		TestTrue(TEXT("Gathering inventory saves"), Inventory->SaveInventory());
		Inventory->ResetToDefaults();
		TestTrue(TEXT("Gathering inventory reloads"), Inventory->LoadInventory());
		TestEqual(TEXT("Reloaded seed keeps ID"), Inventory->GetHotbarSlot(9).ItemId, SeedID);
		TestEqual(TEXT("Reloaded seed keeps quantity"), Inventory->GetHotbarSlot(9).Quantity, SavedSeed.Quantity);
		TestNotNull(TEXT("Hotbar item retains icon after reload"), Controller->GetItemIcon(Inventory->GetHotbarSlot(9)));
	}
	TestEqual(TEXT("All twelve gathering items have separate art"), UniqueIcons.Num(), 12);
	Inventory->ResetToDefaults();
	TestTrue(TEXT("Bag can be filled"), Inventory->AddItem(EPSItemType::Stone, 9990));
	FPSGatheringDrop Drop; Drop.Id = FGuid::NewGuid(); Drop.Item.ItemType = EPSItemType::Wood;
	Drop.Item.ItemId = TEXT("030101"); Drop.Item.Quantity = 4;
	Gathering->Restore({}, {Drop}, true, true);
	TestFalse(TEXT("Full bag retains world reward"), Gathering->CollectDrop(Drop.Id, Inventory));
	TestEqual(TEXT("Full bag does not lose reward"), Gathering->GetDrops().Num(), 1);
	TestTrue(TEXT("Make room"), Inventory->RemoveItem(EPSItemType::Stone, 999));
	TestTrue(TEXT("Reward collected after making room"), Gathering->CollectDrop(Drop.Id, Inventory));
	// RemoveItem frees stacks from the end of the bag.
	const FPSItemStack CollectedLogs = Inventory->GetBagSlot(9);
	TestEqual(TEXT("Collected logs keep the item ID"), CollectedLogs.ItemId, FName(TEXT("030101")));
	TestEqual(TEXT("All four logs enter inventory"), CollectedLogs.Quantity, 4);
	FPSItemStack Legacy = CollectedLogs; Legacy.ItemId = TEXT("Log.Oak");
	TestEqual(TEXT("Old saved alias uses the same artwork"), Controller->GetItemIcon(Legacy), Controller->GetItemIcon(CollectedLogs));
	FPSItemStack Missing = Legacy; Missing.ItemId = TEXT("unknown");
	TestNull(TEXT("Unknown artwork falls back safely"), Controller->GetItemIcon(Missing));

	// Exercise both real UI paths with all twelve icons visible at once.
	Inventory->ResetToDefaults();
	for (int32 Species = 0; Species < 6; ++Species)
	{
		Inventory->RemoveFromHotbarSlot(Species, Inventory->GetHotbarSlot(Species).Quantity);
		Inventory->AddItemVariant(EPSItemType::Wood, FName(*FString::Printf(TEXT("03010%d"), Species + 1)), 3 + Species % 4);
		Inventory->AddItemVariant(EPSItemType::TreeSeed, FName(*FString::Printf(TEXT("03020%d"), Species + 1)), 1 + Species % 2);
		Inventory->MoveItem(EPSInventoryArea::Bag, Species + 1, EPSInventoryArea::Hotbar, Species);
	}
	UClass* UIClass = LoadClass<UPSInventoryWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_Inventory.WBP_Inventory_C"));
	auto* UI = UIClass ? CreateWidget<UPSInventoryWidget>(Controller, UIClass) : nullptr;
	auto* Hotbar = CreateWidget<UPSHotbarWidget>(Controller);
	if (TestNotNull(TEXT("Authored inventory creates with gameplay owner"), UI) && TestNotNull(TEXT("Hotbar creates"), Hotbar))
	{
		UI->SetInventoryComponent(Inventory); Hotbar->SetInventoryComponent(Inventory); UI->AttachHotbar(Hotbar);
		UI->SelectItem(0);
		for (int32 Species = 0; Species < 6; ++Species)
		{
			TestNotNull(TEXT("Bag UI displays log artwork"), UI->GetItemIcon(Inventory->GetBagSlot(Species)));
			TestNotNull(TEXT("Hotbar UI displays seed artwork"), Hotbar->GetItemIcon(Inventory->GetHotbarSlot(Species)));
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("PSGatheringInventoryRender")))
		{
			auto* Renderer = new FWidgetRenderer(true);
			const auto Slate = UI->TakeWidget();
#if WITH_EDITOR
			// NativeConstruct runs in TakeWidget and loads the notebook artwork.
			FTextureCompilingManager::Get().FinishAllCompilation();
#endif
			FlushRenderingCommands();
			for (const FIntPoint Size : {FIntPoint(1280, 800), FIntPoint(1920, 1080)})
			{
				auto* Target = Renderer->DrawWidget(Slate, FVector2D(Size));
				for (int32 Pass = 0; Pass < 3 && Target; ++Pass) Renderer->DrawWidget(Target, Slate, FVector2D(Size), 1.f / 60.f);
				FlushRenderingCommands();
				TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
				if (TestTrue(TEXT("Inventory artwork renders"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
				{
					TArray64<uint8> Bytes; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Bytes);
					TestTrue(TEXT("Inventory preview saves"), FFileHelper::SaveArrayToFile(Bytes,
						*(FPaths::ProjectSavedDir() / FString::Printf(TEXT("GatheringInventory-%d.png"), Size.X))));
				}
			}
			BeginCleanup(Renderer); FlushRenderingCommands();
		}
		UI->ReleaseSlateResources(true);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(Slot + TEXT("Inventory"), 0);
	return true;
}
#endif
