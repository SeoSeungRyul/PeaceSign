#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerController.h"
#include "../World/PSGridWorld.h"
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../Inventory/PSInventoryComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSWorldResetInventoryTest, "PeaceSign.Inventory.WorldResetPreservesItemsAndSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSWorldResetInventoryTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	const FString Slot = TEXT("ResetInventoryTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot; Grid->FinishSpawning(FTransform::Identity);
	Grid->Gathering->bEnableTestArea = false;
	auto* Controller = World->SpawnActor<APSPlayerController>(); Controller->GridWorld = Grid;
	auto* Inventory = Controller->GetInventoryComponent();
	Inventory->bAutoSave = false; Inventory->SaveSlotName = Slot + TEXT("Inventory"); Inventory->ResetToDefaults();
	Inventory->AddItemVariant(EPSItemType::Wood, TEXT("030101"), 17);
	Inventory->AddItemVariant(EPSItemType::TreeSeed, TEXT("030201"), 9);
	Inventory->GrantGatheringPreviewItems();
	Inventory->AddItemVariant(EPSItemType::Fish, TEXT("ResetFixture.Fish"), 3);
	TestTrue(TEXT("Fixture has some lettuce before reset"),
		Inventory->AddItemVariant(EPSItemType::TestCrop, PSItemIds::Lettuce, 15, PSItemIds::LettuceCropId));
	const int32 Capacity = Inventory->GetUnlockedBagSlotCount();
	TestTrue(TEXT("Fixture moves a fishing rod into the bag"),
		Inventory->MoveItem(EPSInventoryArea::Hotbar, 2, EPSInventoryArea::Bag, Capacity - 1));
	bool bDestroyed = false;
	TestTrue(TEXT("Fixture damages the hotbar axe"), Inventory->ConsumeHotbarDurability(8, 2, bDestroyed));
	TestTrue(TEXT("Fixture damages the bag fishing rod"), Inventory->ConsumeItemDurability(EPSItemType::FishingRod, 3, bDestroyed));
	Controller->SelectHotbarSlot(8);
	const int32 Selected = Controller->GetSelectedHotbarSlot();
	const EPSEquipment Equipment = Controller->GetEquipment();
	TArray<FPSItemStack> Hotbar, Bag;
	for (int32 Index=0;Index<UPSInventoryComponent::HotbarSlotCount;++Index) Hotbar.Add(Inventory->GetHotbarSlot(Index));
	for (int32 Index=0;Index<UPSInventoryComponent::MaxBagSlotCount;++Index) Bag.Add(Inventory->GetBagSlot(Index));
	Hotbar[8].CurrentDurability = PSItems::GetDefinition(Hotbar[8]).MaxDurability;
	Bag[Capacity - 1].CurrentDurability = PSItems::GetDefinition(Bag[Capacity - 1]).MaxDurability;
	for (FPSItemStack& Stack : Bag)
		if (Stack.ItemId == PSItemIds::Lettuce) Stack.Quantity = 100;
	TestTrue(TEXT("Baseline inventory saves"), Inventory->SaveInventory());
	Inventory->bAutoSave = true;
	const auto CheckStack = [this](const FPSItemStack& Actual, const FPSItemStack& Expected)
	{
		TestEqual(TEXT("Item type survives R"),Actual.ItemType,Expected.ItemType);
		TestEqual(TEXT("Item identity survives R"),Actual.ItemId,Expected.ItemId);
		TestEqual(TEXT("Expected item quantity after R"),Actual.Quantity,Expected.Quantity);
		TestEqual(TEXT("Crop identity survives R"),Actual.CropId,Expected.CropId);
		TestEqual(TEXT("Expected durability after R"),Actual.CurrentDurability,Expected.CurrentDurability);
	};
	for (int32 Attempt=0;Attempt<2;++Attempt)
	{
		FPSTreeRecord Tree; Tree.Cell=FIntPoint(6,6);
		Grid->Gathering->Restore({Tree},{},true,true);
		Grid->Gathering->Strike(Tree.Cell,PSGathering::GetAxe(PSGathering::AxeId(9)));
		TestTrue(TEXT("Fixture has world drops before reset"),!Grid->Gathering->GetDrops().IsEmpty());
		Controller->HandleResetWorld();
		TestTrue(TEXT("World reset still clears world drops"),Grid->Gathering->GetDrops().IsEmpty());
		TestEqual(TEXT("Expanded bag capacity survives R"),Inventory->GetUnlockedBagSlotCount(),Capacity);
		TestEqual(TEXT("Selected hotbar slot survives R"),Controller->GetSelectedHotbarSlot(),Selected);
		TestEqual(TEXT("Selected equipment survives R"),Controller->GetEquipment(),Equipment);
		for (int32 Index=0;Index<Hotbar.Num();++Index) CheckStack(Inventory->GetHotbarSlot(Index),Hotbar[Index]);
		for (int32 Index=0;Index<Bag.Num();++Index) CheckStack(Inventory->GetBagSlot(Index),Bag[Index]);
	}
	auto* Reloaded = NewObject<UPSInventoryComponent>(World);
	Reloaded->bAutoSave=false; Reloaded->SaveSlotName=Inventory->SaveSlotName;
	TestTrue(TEXT("Inventory reloads after world reset"),Reloaded->LoadInventory());
	TestEqual(TEXT("Saved bag capacity was not reset"),Reloaded->GetUnlockedBagSlotCount(),Capacity);
	for (int32 Index=0;Index<Hotbar.Num();++Index) CheckStack(Reloaded->GetHotbarSlot(Index),Hotbar[Index]);
	for (int32 Index=0;Index<Bag.Num();++Index) CheckStack(Reloaded->GetBagSlot(Index),Bag[Index]);
	Inventory->bAutoSave=false;

	TestTrue(TEXT("Remove lettuce before filling the bag"), Inventory->RemoveItem(EPSItemType::TestCrop, 100, PSItemIds::LettuceCropId));
	int32 EmptySlots = 0;
	for (int32 Index = 0; Index < Capacity; ++Index)
		if (Inventory->GetBagSlot(Index).IsEmpty()) ++EmptySlots;
	const int32 StoneStackSize = PSItems::GetDefinition(EPSItemType::Stone).MaxStack;
	TestTrue(TEXT("Fill every available bag slot"), Inventory->AddItem(EPSItemType::Stone, EmptySlots * StoneStackSize));
	Controller->HandleResetWorld();
	Controller->HandleResetWorld();
	const auto& Drops = Grid->Gathering->GetDrops();
	if (TestEqual(TEXT("Repeated R leaves just one lettuce drop when the bag is full"), Drops.Num(), 1))
	{
		TestEqual(TEXT("The overflow item is lettuce"), Drops[0].Item.ItemId, PSItemIds::Lettuce);
		TestEqual(TEXT("The full lettuce supply is available to pick up"), Drops[0].Item.Quantity, 100);
	}
	TestEqual(TEXT("A full bag stays the same size"), Inventory->GetUnlockedBagSlotCount(), Capacity);

	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot,0); UGameplayStatics::DeleteGameInSlot(Slot+TEXT("Inventory"),0);
	return true;
}
#endif
