#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../Inventory/PSInventoryComponent.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSGatheringPreviewTest, "PeaceSign.Gathering.PreviewGrantPreservesInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSGatheringPreviewTest::RunTest(const FString& Parameters)
{
	auto* Inventory = NewObject<UPSInventoryComponent>();
	Inventory->bAutoSave = false;
	Inventory->SaveSlotName = TEXT("GatheringPreview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Inventory->ResetToDefaults();
	const FPSItemStack OriginalTool = Inventory->GetHotbarSlot(0);
	TestEqual(TEXT("Normal new games keep a ten-slot bag"), Inventory->GetUnlockedBagSlotCount(), 10);
	TestEqual(TEXT("Preview items are never granted by default"), Inventory->CountItem(EPSItemType::Wood), 0);
	TestTrue(TEXT("Preview grant succeeds"), Inventory->GrantGatheringPreviewItems());
	TestEqual(TEXT("Only two bag rows are needed for twelve variants"), Inventory->GetUnlockedBagSlotCount(), 20);
	TestEqual(TEXT("Six log variants"), Inventory->CountItem(EPSItemType::Wood), 6);
	TestEqual(TEXT("Six seed variants"), Inventory->CountItem(EPSItemType::TreeSeed), 6);
	TestEqual(TEXT("Hotbar tool kept"), Inventory->GetHotbarSlot(0).ItemType, OriginalTool.ItemType);
	TestEqual(TEXT("Hotbar tool durability kept"), Inventory->GetHotbarSlot(0).CurrentDurability, OriginalTool.CurrentDurability);
	TestEqual(TEXT("Starter crop seeds kept"), Inventory->CountItem(EPSItemType::TestSeed), 24);
	TestTrue(TEXT("Repeated preview succeeds"), Inventory->GrantGatheringPreviewItems());
	TestEqual(TEXT("Repeated command does not duplicate logs"), Inventory->CountItem(EPSItemType::Wood), 6);
	TestTrue(TEXT("Expanded preview saves"), Inventory->SaveInventory());
	Inventory->ResetToDefaults();
	TestTrue(TEXT("Expanded preview reloads"), Inventory->LoadInventory());
	TestEqual(TEXT("Expanded capacity survives reload"), Inventory->GetUnlockedBagSlotCount(), 20);
	TestEqual(TEXT("Preview seeds survive reload"), Inventory->CountItem(EPSItemType::TreeSeed), 6);

	Inventory->ResetToDefaults();
	TestTrue(TEXT("Existing bag fills"), Inventory->AddItem(EPSItemType::Stone, 9990));
	TestTrue(TEXT("Occupied bag expands without overwriting"), Inventory->GrantGatheringPreviewItems());
	TestEqual(TEXT("Existing full bag retains every stone"), Inventory->CountItem(EPSItemType::Stone), 9990);
	TestEqual(TEXT("Three rows accommodate existing ten stacks plus twelve new variants"), Inventory->GetUnlockedBagSlotCount(), 30);

	Inventory->ResetToDefaults();
	Inventory->UnlockedBagSlotCount = UPSInventoryComponent::MaxBagSlotCount;
	TestTrue(TEXT("Maximum bag fills"), Inventory->AddItem(EPSItemType::Stone, 49950));
	TestFalse(TEXT("Maximum full bag rejects grant atomically"), Inventory->GrantGatheringPreviewItems());
	TestEqual(TEXT("No items lost on rejection"), Inventory->CountItem(EPSItemType::Stone), 49950);
	TestEqual(TEXT("No partial logs on rejection"), Inventory->CountItem(EPSItemType::Wood), 0);
	TestEqual(TEXT("No partial seeds on rejection"), Inventory->CountItem(EPSItemType::TreeSeed), 0);

	Inventory->ResetToDefaults();
	Inventory->AddItemVariant(EPSItemType::Wood, TEXT("Log.Oak"), 17);
	TestTrue(TEXT("Existing legacy log participates in preview"), Inventory->GrantGatheringPreviewItems());
	TestEqual(TEXT("Owned oak stack is not changed"), Inventory->GetBagSlot(0).Quantity, 17);
	TestEqual(TEXT("Only the other five log variants are added"), Inventory->CountItem(EPSItemType::Wood), 22);
	UGameplayStatics::DeleteGameInSlot(Inventory->SaveSlotName, 0);
	return true;
}
#endif
