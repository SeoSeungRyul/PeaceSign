#include "PSInventoryComponent.h"
#include "PSInventorySaveGame.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr int32 CurrentInventorySaveVersion = 3;

	bool UsesCropId(const EPSItemType ItemType)
	{
		return ItemType == EPSItemType::TestSeed || ItemType == EPSItemType::TestCrop;
	}

	int32 NormalizeCropId(const EPSItemType ItemType, const int32 CropId)
	{
		return UsesCropId(ItemType) ? FMath::Max(0, CropId) : INDEX_NONE;
	}

	bool MatchesItem(const FPSItemStack& Slot, const EPSItemType ItemType, const int32 CropId)
	{
		return !Slot.IsEmpty() && Slot.ItemType == ItemType && (CropId < 0 || Slot.CropId == CropId);
	}

	bool MatchesStack(const FPSItemStack& Slot, const EPSItemType ItemType, const FName ItemId,
		const int32 CropId, const int32 CurrentDurability)
	{
		return !Slot.IsEmpty() && Slot.ItemType == ItemType && Slot.ItemId == ItemId
			&& Slot.CropId == CropId && Slot.CurrentDurability == CurrentDurability;
	}

	void SanitizeSlots(TArray<FPSItemStack>& Slots, const bool bLegacyDurability)
	{
		for (FPSItemStack& Slot : Slots)
		{
			if (!PSItems::IsValid(Slot.ItemType) || Slot.Quantity <= 0) Slot.Clear();
			else
			{
				Slot.CropId = NormalizeCropId(Slot.ItemType, Slot.CropId);
				if (Slot.ItemId.IsNone()) Slot.ItemId = PSItems::GetDefaultItemId(Slot.ItemType);
				const FPSItemDefinition& Definition = PSItems::GetDefinition(Slot);
				Slot.Quantity = FMath::Min(Slot.Quantity, Definition.MaxStack);
				if (Definition.bInfiniteDurability) Slot.CurrentDurability = INDEX_NONE;
				else if (Definition.MaxDurability > 0)
				{
					if (bLegacyDurability && Slot.CurrentDurability <= 0) Slot.CurrentDurability = Definition.MaxDurability;
					Slot.CurrentDurability = FMath::Clamp(Slot.CurrentDurability, 0, Definition.MaxDurability);
					if (Slot.CurrentDurability <= 0) Slot.Clear();
				}
				else Slot.CurrentDurability = INDEX_NONE;
			}
		}
	}
}

UPSInventoryComponent::UPSInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPSInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!LoadInventory())
	{
		InitializeDefaults();
		if (bAutoSave) SaveInventory();
	}
}

void UPSInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bAutoSave && HotbarSlots.Num() == HotbarSlotCount && BagSlots.Num() == MaxBagSlotCount) SaveInventory();
	Super::EndPlay(EndPlayReason);
}

FPSItemStack UPSInventoryComponent::GetHotbarSlot(const int32 Index) const
{
	return HotbarSlots.IsValidIndex(Index) ? HotbarSlots[Index] : FPSItemStack();
}

const FPSItemStack* UPSInventoryComponent::FindHotbarSlot(const int32 Index) const
{
	return HotbarSlots.IsValidIndex(Index) ? &HotbarSlots[Index] : nullptr;
}

bool UPSInventoryComponent::IsHotbarSlot(const int32 Index) const
{
	return Index >= 0 && Index < HotbarSlotCount && HotbarSlots.IsValidIndex(Index);
}

FPSItemStack UPSInventoryComponent::GetBagSlot(const int32 Index) const
{
	return BagSlots.IsValidIndex(Index) ? BagSlots[Index] : FPSItemStack();
}

const FPSItemStack* UPSInventoryComponent::FindBagSlot(const int32 Index) const
{
	return BagSlots.IsValidIndex(Index) ? &BagSlots[Index] : nullptr;
}

bool UPSInventoryComponent::IsBagSlotUnlocked(const int32 Index) const
{
	return Index >= 0 && Index < UnlockedBagSlotCount && BagSlots.IsValidIndex(Index);
}

TArray<FPSItemStack>& UPSInventoryComponent::GetAreaSlots(const EPSInventoryArea Area)
{
	return Area == EPSInventoryArea::Hotbar ? HotbarSlots : BagSlots;
}

const TArray<FPSItemStack>& UPSInventoryComponent::GetAreaSlots(const EPSInventoryArea Area) const
{
	return Area == EPSInventoryArea::Hotbar ? HotbarSlots : BagSlots;
}

bool UPSInventoryComponent::IsAreaSlotUnlocked(const EPSInventoryArea Area, const int32 Index) const
{
	return Area == EPSInventoryArea::Hotbar ? IsHotbarSlot(Index) : IsBagSlotUnlocked(Index);
}

int32 UPSInventoryComponent::CountItem(const EPSItemType ItemType, const int32 CropId) const
{
	if (!PSItems::IsValid(ItemType)) return 0;
	int32 Total = 0;
	for (const FPSItemStack& Slot : HotbarSlots)
		if (MatchesItem(Slot, ItemType, CropId)) Total += Slot.Quantity;
	for (int32 Index = 0; Index < UnlockedBagSlotCount && BagSlots.IsValidIndex(Index); ++Index)
		if (MatchesItem(BagSlots[Index], ItemType, CropId)) Total += BagSlots[Index].Quantity;
	return Total;
}

bool UPSInventoryComponent::HasItem(const EPSItemType ItemType, const int32 Quantity, const int32 CropId) const
{
	return Quantity > 0 && CountItem(ItemType, CropId) >= Quantity;
}

bool UPSInventoryComponent::CanAddItem(const EPSItemType ItemType, const int32 Quantity, const int32 CropId) const
{
	return CanAddItemVariant(ItemType, PSItems::GetDefaultItemId(ItemType), Quantity, CropId);
}

bool UPSInventoryComponent::CanAddItemVariant(const EPSItemType ItemType, FName ItemId, const int32 Quantity,
	const int32 CropId, int32 CurrentDurability) const
{
	if (!PSItems::IsValid(ItemType) || Quantity <= 0) return false;
	if (ItemId.IsNone()) ItemId = PSItems::GetDefaultItemId(ItemType);
	int64 Capacity = 0;
	const FPSItemDefinition& Definition = PSItems::GetDefinition(ItemType, ItemId);
	const int32 MaxStack = Definition.MaxStack;
	const int32 NormalizedCropId = NormalizeCropId(ItemType, CropId);
	CurrentDurability = Definition.bInfiniteDurability || Definition.MaxDurability <= 0
		? INDEX_NONE : FMath::Clamp(CurrentDurability < 0 ? Definition.MaxDurability : CurrentDurability, 1, Definition.MaxDurability);
	for (int32 Index = 0; Index < UnlockedBagSlotCount && BagSlots.IsValidIndex(Index); ++Index)
	{
		const FPSItemStack& Slot = BagSlots[Index];
		if (Slot.IsEmpty()) Capacity += MaxStack;
		else if (MatchesStack(Slot, ItemType, ItemId, NormalizedCropId, CurrentDurability))
			Capacity += FMath::Max(0, MaxStack - Slot.Quantity);
		if (Capacity >= Quantity) return true;
	}
	return false;
}

bool UPSInventoryComponent::AddItem(const EPSItemType ItemType, const int32 Quantity, const int32 CropId)
{
	return AddItemVariant(ItemType, PSItems::GetDefaultItemId(ItemType), Quantity, CropId);
}

bool UPSInventoryComponent::AddItemVariant(const EPSItemType ItemType, FName ItemId, const int32 Quantity,
	const int32 CropId, int32 CurrentDurability)
{
	if (ItemId.IsNone()) ItemId = PSItems::GetDefaultItemId(ItemType);
	const FPSItemDefinition& Definition = PSItems::GetDefinition(ItemType, ItemId);
	CurrentDurability = Definition.bInfiniteDurability || Definition.MaxDurability <= 0
		? INDEX_NONE : FMath::Clamp(CurrentDurability < 0 ? Definition.MaxDurability : CurrentDurability, 1, Definition.MaxDurability);
	if (!CanAddItemVariant(ItemType, ItemId, Quantity, CropId, CurrentDurability)) return false;
	int32 Remaining = Quantity;
	const int32 MaxStack = Definition.MaxStack;
	const int32 NormalizedCropId = NormalizeCropId(ItemType, CropId);
	for (int32 Index = 0; Index < UnlockedBagSlotCount && Remaining > 0; ++Index)
	{
		FPSItemStack& Slot = BagSlots[Index];
		if (!MatchesStack(Slot, ItemType, ItemId, NormalizedCropId, CurrentDurability)) continue;
		const int32 Added = FMath::Min(Remaining, MaxStack - Slot.Quantity);
		Slot.Quantity += Added;
		Remaining -= Added;
	}
	for (int32 Index = 0; Index < UnlockedBagSlotCount && Remaining > 0; ++Index)
	{
		FPSItemStack& Slot = BagSlots[Index];
		if (!Slot.IsEmpty()) continue;
		Slot.ItemType = ItemType;
		Slot.CropId = NormalizedCropId;
		Slot.ItemId = ItemId;
		Slot.CurrentDurability = CurrentDurability;
		Slot.Quantity = FMath::Min(Remaining, MaxStack);
		Remaining -= Slot.Quantity;
	}
	NotifyChanged();
	return true;
}

bool UPSInventoryComponent::ConsumeHotbarDurability(const int32 SlotIndex, const int32 Amount, bool& bDestroyed)
{
	bDestroyed = false;
	if (!IsHotbarSlot(SlotIndex) || Amount <= 0) return false;
	FPSItemStack& Stack = HotbarSlots[SlotIndex];
	if (Stack.IsEmpty()) return false;
	const FPSItemDefinition& Definition = PSItems::GetDefinition(Stack);
	if (Definition.bInfiniteDurability) return true;
	if (!PSItems::UsesDurability(Stack)) return false;
	Stack.CurrentDurability = FMath::Max(0, Stack.CurrentDurability - Amount);
	if (Stack.CurrentDurability == 0)
	{
		Stack.Clear();
		bDestroyed = true;
	}
	NotifyChanged();
	return true;
}

bool UPSInventoryComponent::ConsumeItemDurability(const EPSItemType ItemType, const int32 Amount, bool& bDestroyed)
{
	bDestroyed = false;
	if (!PSItems::IsValid(ItemType) || Amount <= 0) return false;
	const auto ConsumeFrom = [&](TArray<FPSItemStack>& Slots, const int32 Count) -> bool
	{
		for (int32 Index = 0; Index < FMath::Min(Count, Slots.Num()); ++Index)
		{
			FPSItemStack& Stack = Slots[Index];
			if (Stack.IsEmpty() || Stack.ItemType != ItemType) continue;
			const FPSItemDefinition& Definition = PSItems::GetDefinition(Stack);
			if (Definition.bInfiniteDurability) return true;
			if (!PSItems::UsesDurability(Stack)) continue;
			Stack.CurrentDurability = FMath::Max(0, Stack.CurrentDurability - Amount);
			if (Stack.CurrentDurability == 0)
			{
				Stack.Clear();
				bDestroyed = true;
			}
			return true;
		}
		return false;
	};
	const bool bConsumed = ConsumeFrom(BagSlots, UnlockedBagSlotCount)
		|| ConsumeFrom(HotbarSlots, HotbarSlotCount);
	if (bConsumed) NotifyChanged();
	return bConsumed;
}

bool UPSInventoryComponent::RestoreDurability(const EPSInventoryArea Area, const int32 SlotIndex, const int32 Amount)
{
	if (!IsAreaSlotUnlocked(Area, SlotIndex) || Amount <= 0) return false;
	FPSItemStack& Stack = GetAreaSlots(Area)[SlotIndex];
	if (!PSItems::UsesDurability(Stack)) return false;
	const int32 Before = Stack.CurrentDurability;
	Stack.CurrentDurability = FMath::Min(PSItems::GetDefinition(Stack).MaxDurability, Before + Amount);
	if (Before == Stack.CurrentDurability) return false;
	NotifyChanged();
	return true;
}

float UPSInventoryComponent::GetDurabilityRatio(const EPSInventoryArea Area, const int32 SlotIndex) const
{
	if (!IsAreaSlotUnlocked(Area, SlotIndex)) return 0.0f;
	const FPSItemStack& Stack = GetAreaSlots(Area)[SlotIndex];
	const FPSItemDefinition& Definition = PSItems::GetDefinition(Stack);
	if (Definition.bInfiniteDurability) return 1.0f;
	return Definition.MaxDurability > 0
		? FMath::Clamp(static_cast<float>(Stack.CurrentDurability) / Definition.MaxDurability, 0.0f, 1.0f) : 0.0f;
}

bool UPSInventoryComponent::RemoveItem(const EPSItemType ItemType, const int32 Quantity, const int32 CropId)
{
	if (Quantity <= 0 || !HasItem(ItemType, Quantity, CropId)) return false;
	int32 Remaining = Quantity;
	const auto RemoveFrom = [&](TArray<FPSItemStack>& Slots, const int32 Count, int32& Amount)
	{
		for (int32 Index = FMath::Min(Count, Slots.Num()) - 1; Index >= 0 && Amount > 0; --Index)
		{
			FPSItemStack& Slot = Slots[Index];
			if (!MatchesItem(Slot, ItemType, CropId)) continue;
			const int32 Removed = FMath::Min(Amount, Slot.Quantity);
			Slot.Quantity -= Removed;
			Amount -= Removed;
			if (Slot.Quantity == 0) Slot.Clear();
		}
	};
	RemoveFrom(BagSlots, UnlockedBagSlotCount, Remaining);
	RemoveFrom(HotbarSlots, HotbarSlotCount, Remaining);
	NotifyChanged();
	return true;
}

bool UPSInventoryComponent::RemoveFromHotbarSlot(const int32 SlotIndex, const int32 Quantity)
{
	if (!IsHotbarSlot(SlotIndex) || Quantity <= 0 || HotbarSlots[SlotIndex].IsEmpty()
		|| HotbarSlots[SlotIndex].Quantity < Quantity) return false;
	HotbarSlots[SlotIndex].Quantity -= Quantity;
	if (HotbarSlots[SlotIndex].Quantity == 0) HotbarSlots[SlotIndex].Clear();
	NotifyChanged();
	return true;
}

bool UPSInventoryComponent::MoveItem(const EPSInventoryArea FromArea, const int32 FromIndex,
	const EPSInventoryArea ToArea, const int32 ToIndex)
{
	if (!IsAreaSlotUnlocked(FromArea, FromIndex) || !IsAreaSlotUnlocked(ToArea, ToIndex)
		|| (FromArea == ToArea && FromIndex == ToIndex)) return false;
	TArray<FPSItemStack>& SourceSlots = GetAreaSlots(FromArea);
	TArray<FPSItemStack>& DestinationSlots = GetAreaSlots(ToArea);
	FPSItemStack& Source = SourceSlots[FromIndex];
	FPSItemStack& Destination = DestinationSlots[ToIndex];
	if (Source.IsEmpty()) return false;
	if (Destination.IsEmpty())
	{
		Destination = Source;
		Source.Clear();
	}
	else if (Source.ItemType == Destination.ItemType && Source.CropId == Destination.CropId
		&& Source.ItemId == Destination.ItemId && Source.CurrentDurability == Destination.CurrentDurability)
	{
		const int32 Capacity = PSItems::GetDefinition(Source).MaxStack - Destination.Quantity;
		if (Capacity <= 0) return false;
		const int32 Moved = FMath::Min(Capacity, Source.Quantity);
		Destination.Quantity += Moved;
		Source.Quantity -= Moved;
		if (Source.Quantity == 0) Source.Clear();
	}
	else
	{
		Swap(Source, Destination);
	}
	NotifyChanged();
	return true;
}

void UPSInventoryComponent::ResetToDefaults()
{
	InitializeDefaults();
	NotifyChanged();
}

void UPSInventoryComponent::InitializeDefaults()
{
	HotbarSlots.SetNum(HotbarSlotCount);
	BagSlots.SetNum(MaxBagSlotCount);
	for (FPSItemStack& Slot : HotbarSlots) Slot.Clear();
	for (FPSItemStack& Slot : BagSlots) Slot.Clear();
	UnlockedBagSlotCount = 10;
	HotbarSlots[0].ItemType = EPSItemType::Hoe;
	HotbarSlots[0].Quantity = 1;
	HotbarSlots[0].CurrentDurability = PSItems::GetDefinition(EPSItemType::Hoe).MaxDurability;
	HotbarSlots[1].ItemType = EPSItemType::TestSeed;
	HotbarSlots[1].CropId = 0;
	HotbarSlots[1].Quantity = 24;
	HotbarSlots[2].ItemType = EPSItemType::FishingRod;
	HotbarSlots[2].ItemId = PSItemIds::WoodenFishingRod;
	HotbarSlots[2].Quantity = 1;
	HotbarSlots[2].CurrentDurability = PSItems::GetDefinition(HotbarSlots[2]).MaxDurability;
}

void UPSInventoryComponent::NotifyChanged()
{
	if (bAutoSave) SaveInventory();
	OnInventoryChanged.Broadcast();
}

bool UPSInventoryComponent::SaveInventory() const
{
	if (SaveSlotName.IsEmpty() || HotbarSlots.Num() != HotbarSlotCount || BagSlots.Num() != MaxBagSlotCount)
		return false;
	UPSInventorySaveGame* Save = Cast<UPSInventorySaveGame>(
		UGameplayStatics::CreateSaveGameObject(UPSInventorySaveGame::StaticClass()));
	if (!Save) return false;
	Save->DataVersion = CurrentInventorySaveVersion;
	Save->HotbarSlots = HotbarSlots;
	Save->BagSlots = BagSlots;
	Save->UnlockedBagSlotCount = UnlockedBagSlotCount;
	return UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
}

bool UPSInventoryComponent::LoadInventory()
{
	if (SaveSlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0)) return false;
	const UPSInventorySaveGame* Save = Cast<UPSInventorySaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!Save) return false;
	bool bMigrated = false;
	if (Save->DataVersion >= 2 && Save->HotbarSlots.Num() == HotbarSlotCount
		&& Save->BagSlots.Num() == MaxBagSlotCount)
	{
		HotbarSlots = Save->HotbarSlots;
		BagSlots = Save->BagSlots;
		UnlockedBagSlotCount = FMath::Clamp(Save->UnlockedBagSlotCount, 10, MaxBagSlotCount);
	}
	else if (Save->Slots.Num() == MaxBagSlotCount)
	{
		HotbarSlots.SetNum(HotbarSlotCount);
		BagSlots.SetNum(MaxBagSlotCount);
		for (FPSItemStack& Slot : HotbarSlots) Slot.Clear();
		for (FPSItemStack& Slot : BagSlots) Slot.Clear();
		for (int32 Index = 0; Index < HotbarSlotCount; ++Index) HotbarSlots[Index] = Save->Slots[Index];
		for (int32 OldIndex = HotbarSlotCount; OldIndex < Save->Slots.Num(); ++OldIndex)
			BagSlots[OldIndex - HotbarSlotCount] = Save->Slots[OldIndex];
		UnlockedBagSlotCount = FMath::Clamp(FMath::Max(10, Save->UnlockedSlotCount - HotbarSlotCount), 10, MaxBagSlotCount);
		bMigrated = true;
	}
	else
	{
		return false;
	}
	const bool bLegacyDurability = Save->DataVersion < CurrentInventorySaveVersion;
	SanitizeSlots(HotbarSlots, bLegacyDurability);
	SanitizeSlots(BagSlots, bLegacyDurability);
	if (Save->DataVersion < 1 && CountItem(EPSItemType::FishingRod) == 0 && HotbarSlots[2].IsEmpty())
	{
		HotbarSlots[2].ItemType = EPSItemType::FishingRod;
		HotbarSlots[2].ItemId = PSItemIds::WoodenFishingRod;
		HotbarSlots[2].Quantity = 1;
		HotbarSlots[2].CurrentDurability = PSItems::GetDefinition(HotbarSlots[2]).MaxDurability;
		bMigrated = true;
	}
	if (bLegacyDurability) bMigrated = true;
	if (bMigrated && bAutoSave) SaveInventory();
	OnInventoryChanged.Broadcast();
	return true;
}
