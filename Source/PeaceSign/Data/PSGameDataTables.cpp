#include "PSGameDataTables.h"

namespace
{
	struct FDropEntry { FName Row; FName Item; double Weight; int32 Count; };
	using FDropGroups = TMap<FName, TArray<FDropEntry>>;

	bool Fail(FString& Error, const FString& Message) { Error = Message; return false; }
	bool IsBlankItem(const FName Item) { return Item.IsNone() || Item == TEXT("0"); }

	template<typename T>
	bool ReadGroups(const UDataTable* Items, const UDataTable* Table, const bool bCut, FDropGroups& Groups, FString& Error)
	{
		Groups.Reset();
		if (!Items || Items->GetRowStruct() != FPSItemTableRow::StaticStruct()) return Fail(Error, TEXT("Item table must use PSItemTableRow."));
		if (!Table || Table->GetRowStruct() != T::StaticStruct()) return Fail(Error, TEXT("Drop table has the wrong row structure or is missing."));
		auto Names = Table->GetRowNames();
		Names.Sort(FNameLexicalLess()); // Stable outcomes regardless of DataTable map iteration order.
		for (const FName RowName : Names)
		{
			const T& Row = *Table->FindRow<T>(RowName, TEXT("TreeDrop"), false);
			if (Row.ID.IsNone() || !FMath::IsFinite(Row.DropRate) || Row.DropRate < 0 || Row.DropRate > 1 || Row.Count < 0)
				return Fail(Error, FString::Printf(TEXT("Invalid ID, DropRate or Count on row %s."), *RowName.ToString()));
			const FName Item = UPSGameDataLibrary::CanonicalItemID(Row.ItemID);
			if (IsBlankItem(Item))
			{
				if (bCut || Row.Count != 0) return Fail(Error, FString::Printf(TEXT("Blank item requires a shake row with Count=0: %s."), *RowName.ToString()));
			}
			else
			{
				FPSItemTableRow Definition;
				if (!UPSGameDataLibrary::FindItem(Items, Item, Definition)) return Fail(Error, FString::Printf(TEXT("Unknown ItemID %s on row %s."), *Item.ToString(), *RowName.ToString()));
				if (UPSGameDataLibrary::GatheringItemType(Item) == EPSItemType::None)
					return Fail(Error, FString::Printf(TEXT("ItemID %s is not a supported gathering item."), *Item.ToString()));
			}
			Groups.FindOrAdd(Row.ID).Add({RowName, Item, Row.DropRate, Row.Count});
		}
		if (Groups.IsEmpty()) return Fail(Error, TEXT("Drop table contains no rows."));
		for (const auto& Group : Groups)
		{
			TMap<FName, double> Totals;
			for (const auto& Entry : Group.Value) Totals.FindOrAdd(bCut ? Entry.Item : NAME_None) += Entry.Weight;
			for (const auto& Total : Totals)
				if (!FMath::IsNearlyEqual(Total.Value, 1.0, 0.00001))
					return Fail(Error, FString::Printf(TEXT("Probability sum for drop ID %s / %s must be 1, got %.10f."), *Group.Key.ToString(), *Total.Key.ToString(), Total.Value));
		}
		return true;
	}

	const FDropEntry& Select(const TArray<FDropEntry>& Entries, FRandomStream& Random)
	{
		double Total = 0;
		for (const auto& Entry : Entries) Total += Entry.Weight;
		const double Draw = Random.GetFraction() * Total;
		double Cumulative = 0;
		for (const auto& Entry : Entries)
		{
			Cumulative += Entry.Weight;
			if (Draw < Cumulative) return Entry;
		}
		// Floating-point rounding can land on the upper edge. Never choose a zero-weight row.
		for (int32 Index = Entries.Num() - 1; Index >= 0; --Index) if (Entries[Index].Weight > 0) return Entries[Index];
		checkNoEntry();
		return Entries[0];
	}

	void Append(const FDropEntry& Entry, TArray<FPSItemStack>& Rewards)
	{
		if (Entry.Count <= 0 || IsBlankItem(Entry.Item)) return;
		FPSItemStack Stack;
		Stack.ItemType = UPSGameDataLibrary::GatheringItemType(Entry.Item);
		Stack.ItemId = Entry.Item;
		Stack.Quantity = Entry.Count;
		Rewards.Add(Stack);
	}
}

FName UPSGameDataLibrary::CanonicalItemID(const FName ItemID)
{
	if (ItemID == TEXT("Log.Oak")) return TEXT("030101");
	if (ItemID == TEXT("Log.Maple")) return TEXT("030102");
	if (ItemID == TEXT("TreeSeed.Oak")) return TEXT("030201");
	if (ItemID == TEXT("TreeSeed.Maple")) return TEXT("030202");
	return ItemID;
}

EPSItemType UPSGameDataLibrary::GatheringItemType(const FName ItemID)
{
	const FName Canonical = CanonicalItemID(ItemID);
	for (int32 Species = 1; Species <= 6; ++Species)
	{
		if (Canonical == FName(*FString::Printf(TEXT("03010%d"), Species))) return EPSItemType::Wood;
		if (Canonical == FName(*FString::Printf(TEXT("03020%d"), Species))) return EPSItemType::TreeSeed;
	}
	if (Canonical == TEXT("030207") || Canonical == TEXT("030208")) return EPSItemType::GatheredFruit;
	return EPSItemType::None;
}

bool UPSGameDataLibrary::FindItem(const UDataTable* ItemTable, const FName ItemID, FPSItemTableRow& Item)
{
	Item = FPSItemTableRow();
	if (!ItemTable || ItemTable->GetRowStruct() != FPSItemTableRow::StaticStruct()) return false;
	const auto* Row = ItemTable->FindRow<FPSItemTableRow>(CanonicalItemID(ItemID), TEXT("Item lookup"), false);
	if (!Row) return false;
	Item = *Row;
	return true;
}

bool UPSGameDataLibrary::ValidateTreeTables(const UDataTable* ItemTable, const UDataTable* TreeCutDropTable,
	const UDataTable* TreeShakeDropTable, FString& Error)
{
	Error.Reset();
	FDropGroups Groups;
	return ReadGroups<FPSTreeCutDropRow>(ItemTable, TreeCutDropTable, true, Groups, Error)
		&& ReadGroups<FPSTreeShakeDropRow>(ItemTable, TreeShakeDropTable, false, Groups, Error);
}

bool UPSGameDataLibrary::RollTreeCutDrop(const UDataTable* ItemTable, const UDataTable* DropTable,
	const FName DropID, const int32 RandomSeed, TArray<FPSItemStack>& Rewards, FString& Error)
{
	Rewards.Reset(); Error.Reset();
	FDropGroups Groups;
	if (!ReadGroups<FPSTreeCutDropRow>(ItemTable, DropTable, true, Groups, Error)) return false;
	const auto* Group = Groups.Find(DropID);
	if (!Group) return Fail(Error, FString::Printf(TEXT("Unknown cut drop ID: %s"), *DropID.ToString()));
	TMap<FName, TArray<FDropEntry>> ByItem;
	for (const auto& Entry : *Group) ByItem.FindOrAdd(Entry.Item).Add(Entry);
	TArray<FName> IDs; ByItem.GenerateKeyArray(IDs); IDs.Sort(FNameLexicalLess());
	FRandomStream Random(RandomSeed);
	for (const FName ID : IDs) Append(Select(ByItem.FindChecked(ID), Random), Rewards);
	return true;
}

bool UPSGameDataLibrary::RollTreeShakeDrop(const UDataTable* ItemTable, const UDataTable* DropTable,
	const FName DropID, const int32 RandomSeed, TArray<FPSItemStack>& Rewards, FString& Error)
{
	Rewards.Reset(); Error.Reset();
	FDropGroups Groups;
	if (!ReadGroups<FPSTreeShakeDropRow>(ItemTable, DropTable, false, Groups, Error)) return false;
	const auto* Group = Groups.Find(DropID);
	if (!Group) return Fail(Error, FString::Printf(TEXT("Unknown shake drop ID: %s"), *DropID.ToString()));
	FRandomStream Random(RandomSeed);
	Append(Select(*Group, Random), Rewards);
	return true;
}
