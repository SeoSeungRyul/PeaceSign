#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../Data/PSGameDataTables.h"
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../World/PSGridWorld.h"
#include "../Inventory/PSInventoryComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSGameDataTablesTest, "PeaceSign.Gathering.CSVTablesAndWeightedDrops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSGameDataTablesTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	const auto Import = [this, World](const TCHAR* Name, UScriptStruct* Struct)
	{
		auto* Table = NewObject<UDataTable>(World); Table->RowStruct = Struct;
		FString CSV;
		if (!FFileHelper::LoadFileToString(CSV, *(FPaths::ProjectDir() / TEXT("SourceData/TreeTables") / (FString(Name) + TEXT(".csv")))))
			{ AddError(TEXT("Source CSV could not be read")); return Table; }
		const auto Errors = Table->CreateTableFromCSVString(CSV);
		for (const auto& Error : Errors) AddError(Error);
		return Table;
	};
	auto* Items = Import(TEXT("Item"), FPSItemTableRow::StaticStruct());
	auto* Cut = Import(TEXT("TreeCutDrop"), FPSTreeCutDropRow::StaticStruct());
	auto* Shake = Import(TEXT("TreeShakeDrop"), FPSTreeShakeDropRow::StaticStruct());
	TestEqual(TEXT("Item imports all 66 rows"), Items->GetRowNames().Num(), 66);
	TestEqual(TEXT("Cut imports 42 unique rows, no overwritten duplicate IDs"), Cut->GetRowNames().Num(), 42);
	TestEqual(TEXT("Shake imports 48 unique rows"), Shake->GetRowNames().Num(), 48);
	FPSItemTableRow Item;
	TestTrue(TEXT("Leading-zero item IDs resolve"), UPSGameDataLibrary::FindItem(Items, TEXT("030101"), Item));
	TestEqual(TEXT("Korean UTF-8 name survives CSV"), Item.Name.ToString(), FString(TEXT("참나무 원목")));
	TestEqual(TEXT("Missing price remains unauthored"), Item.Sellp, -1);
	TestTrue(TEXT("Discard flag imports as a bool"), Item.Isdiscard);
	TestTrue(TEXT("Legacy aliases resolve through the Item table"), UPSGameDataLibrary::FindItem(Items, TEXT("Log.Oak"), Item));
	FString Error;
	TestTrue(TEXT("Original workbook drop groups validate"), UPSGameDataLibrary::ValidateTreeTables(Items, Cut, Shake, Error));
	TArray<FPSItemStack> Rewards, Again;
	TSet<int32> LogCounts, SeedCounts;
	TSet<FName> ShakeItems;
	bool bSawBlank = false;
	for (int32 Seed = 0; Seed < 1000; ++Seed)
	{
		if (!UPSGameDataLibrary::RollTreeCutDrop(Items, Cut, TEXT("100101"), Seed, Rewards, Error)) { AddError(Error); break; }
		int32 Logs = 0, Seeds = 0;
		for (const auto& Reward : Rewards)
		{
			if (Reward.ItemId == TEXT("030101")) Logs += Reward.Quantity;
			else if (Reward.ItemId == TEXT("030201")) Seeds += Reward.Quantity;
			else AddError(TEXT("Cut returned an item from a different tree"));
		}
		if (Logs < 3 || Logs > 6 || Seeds < 0 || Seeds > 2) { AddError(TEXT("Cut did not independently choose log and seed quantities")); break; }
		LogCounts.Add(Logs); SeedCounts.Add(Seeds);
		if (!UPSGameDataLibrary::RollTreeShakeDrop(Items, Shake, TEXT("100201"), Seed, Rewards, Error)) { AddError(Error); break; }
		if (Rewards.IsEmpty()) bSawBlank = true;
		else
		{
			TestEqual(TEXT("Shake selects only one candidate"), Rewards.Num(), 1);
			const auto& Reward = Rewards[0];
			ShakeItems.Add(Reward.ItemId);
			if (Reward.ItemId == TEXT("030207")) TestTrue(TEXT("Apple quantity is 1..5"), Reward.Quantity >= 1 && Reward.Quantity <= 5);
			else TestEqual(TEXT("Seed and chestnut quantity is one"), Reward.Quantity, 1);
		}
	}
	TestEqual(TEXT("All four log amounts are reachable"), LogCounts.Num(), 4);
	TestEqual(TEXT("Zero, one and two seeds are reachable"), SeedCounts.Num(), 3);
	TestTrue(TEXT("Shake can legitimately return no item"), bSawBlank);
	TestEqual(TEXT("Seed, apple and chestnut can be selected"), ShakeItems.Num(), 3);
	for (int32 Species = 0; Species < 6; ++Species)
	{
		const auto Definition = PSGathering::GetTree(static_cast<EPSTreeSpecies>(Species));
		TestTrue(TEXT("All six species map to a cut group"), UPSGameDataLibrary::RollTreeCutDrop(Items, Cut, Definition.CutDropID, 123, Rewards, Error));
		TestTrue(TEXT("All six species map to a shake group"), UPSGameDataLibrary::RollTreeShakeDrop(Items, Shake, Definition.ShakeDropID, 123, Again, Error));
	}
	UPSGameDataLibrary::RollTreeCutDrop(Items, Cut, TEXT("100101"), 123, Rewards, Error);
	UPSGameDataLibrary::RollTreeCutDrop(Items, Cut, TEXT("100101"), 123, Again, Error);
	TestEqual(TEXT("Same seed keeps reward count stable"), Rewards.Num(), Again.Num());
	for (int32 Index = 0; Index < Rewards.Num(); ++Index) TestEqual(TEXT("Same seed keeps quantity stable"), Rewards[Index].Quantity, Again[Index].Quantity);
	TestFalse(TEXT("Missing drop ID fails explicitly"), UPSGameDataLibrary::RollTreeCutDrop(Items, Cut, TEXT("unknown"), 0, Rewards, Error));
	TestTrue(TEXT("Failed rolls clear output rather than reusing prior rewards"), Rewards.IsEmpty());
	TestFalse(TEXT("Wrong row structure is rejected"), UPSGameDataLibrary::RollTreeCutDrop(Items, Shake, TEXT("100101"), 0, Rewards, Error));
	auto* Candidate = Cut->FindRow<FPSTreeCutDropRow>(TEXT("100101_030101_3"), TEXT("test"));
	if (Candidate)
	{
		Candidate->DropRate = 0.5;
		TestFalse(TEXT("Wrong probability total is rejected"), UPSGameDataLibrary::ValidateTreeTables(Items, Cut, Shake, Error));
		Candidate->DropRate = 0.25;
		Candidate->ItemID = TEXT("039999");
		TestFalse(TEXT("Missing item references are rejected"), UPSGameDataLibrary::ValidateTreeTables(Items, Cut, Shake, Error));
		Candidate->ItemID = TEXT("030101");
	}

	const FString Slot = TEXT("TreeTablesTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot; Grid->FinishSpawning(FTransform::Identity);
	auto* Resources = Grid->Gathering.Get();
	Resources->ItemTable = Items; Resources->TreeCutDropTable = Cut; Resources->TreeShakeDropTable = Shake;
	FPSTreeRecord Tree; Tree.Cell = FIntPoint(6, 6); Tree.Health = 10;
	Resources->Restore({Tree}, {}, true, true);
	TestTrue(TEXT("Runtime felling uses the imported cut table"), Resources->Strike(Tree.Cell, PSGathering::GetAxe(PSGathering::AxeId(1))));
	const auto SavedDrops = Resources->GetDrops();
	TestTrue(TEXT("Felling produces at least a log stack"), !SavedDrops.IsEmpty());
	Grid->LoadWorld();
	TestEqual(TEXT("Saved rewards are not rerolled on reload"), Resources->GetDrops().Num(), SavedDrops.Num());
	for (int32 Index = 0; Index < SavedDrops.Num(); ++Index)
	{
		TestEqual(TEXT("Saved reward IDs remain stable"), Resources->GetDrops()[Index].Id, SavedDrops[Index].Id);
		TestEqual(TEXT("Saved reward amounts remain stable"), Resources->GetDrops()[Index].Item.Quantity, SavedDrops[Index].Item.Quantity);
	}
	auto* BlankTable = NewObject<UDataTable>(World); BlankTable->RowStruct = FPSTreeShakeDropRow::StaticStruct();
	FPSTreeShakeDropRow Blank; Blank.ID = TEXT("100201"); Blank.ItemID = TEXT("0"); Blank.DropRate = 1;
	BlankTable->AddRow(TEXT("Blank"), Blank); Resources->TreeShakeDropTable = BlankTable;
	Tree.Health = 60; Resources->Restore({Tree}, {}, true, true);
	bool bHadReward = true;
	TestTrue(TEXT("Blank shake is a completed valid action"), Resources->Shake(Tree.Cell, &bHadReward));
	TestFalse(TEXT("Blank shake creates no item"), bHadReward);
	TestTrue(TEXT("Blank shake leaves no pending drop"), Resources->GetDrops().IsEmpty());
	const int64 Deadline = Resources->GetTree(Tree.Cell).NextShakeHalfHour;
	TestEqual(TEXT("Blank shake still starts a seven-day cooldown"), Deadline, Resources->Now() + PSGathering::ShakeCooldown);
	TestFalse(TEXT("Blank shake cannot be rerolled immediately"), Resources->Shake(Tree.Cell));
	Grid->LoadWorld();
	TestEqual(TEXT("Blank cooldown survives reload"), Resources->GetTree(Tree.Cell).NextShakeHalfHour, Deadline);
	Resources->ItemTable = nullptr;
	TestFalse(TEXT("Partially configured tables do not fall back to free prototype rewards"), Resources->ValidateRewardData(Tree.Cell, false, Error));
	auto* Inventory = NewObject<UPSInventoryComponent>(World); Inventory->bAutoSave = false; Inventory->ResetToDefaults();
	Inventory->AddItemVariant(EPSItemType::Wood, TEXT("Log.Oak"), 1);
	Inventory->AddItemVariant(EPSItemType::Wood, TEXT("030101"), 1);
	TestEqual(TEXT("Old and new item IDs merge into the same stack"), Inventory->GetBagSlot(0).Quantity, 2);
	TestEqual(TEXT("Stacks store the canonical leading-zero ID"), Inventory->GetBagSlot(0).ItemId, FName(TEXT("030101")));
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	return true;
}
#endif
