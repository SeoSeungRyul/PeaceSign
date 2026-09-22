#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSItemTypes.h"
#include "PSInventoryComponent.generated.h"

UENUM(BlueprintType)
enum class EPSInventoryArea : uint8
{
	Hotbar,
	Bag
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPSInventoryChanged);

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PEACESIGN_API UPSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPSInventoryComponent();
	static constexpr int32 HotbarSlotCount = 10;
	static constexpr int32 MaxBagSlotCount = 50;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="Inventory|Hotbar")
	FPSItemStack GetHotbarSlot(int32 Index) const;
	const FPSItemStack* FindHotbarSlot(int32 Index) const;
	UFUNCTION(BlueprintPure, Category="Inventory|Hotbar")
	bool IsHotbarSlot(int32 Index) const;

	UFUNCTION(BlueprintPure, Category="Inventory|Bag")
	FPSItemStack GetBagSlot(int32 Index) const;
	const FPSItemStack* FindBagSlot(int32 Index) const;
	UFUNCTION(BlueprintPure, Category="Inventory|Bag")
	bool IsBagSlotUnlocked(int32 Index) const;
	UFUNCTION(BlueprintPure, Category="Inventory|Bag")
	int32 GetUnlockedBagSlotCount() const { return UnlockedBagSlotCount; }

	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 CountItem(EPSItemType ItemType, int32 CropId = -1) const;
	UFUNCTION(BlueprintPure, Category="Inventory")
	bool HasItem(EPSItemType ItemType, int32 Quantity = 1, int32 CropId = -1) const;
	/** Tests capacity in the bag. Harvest rewards enter the bag, never an empty hotbar slot. */
	UFUNCTION(BlueprintPure, Category="Inventory|Bag")
	bool CanAddItem(EPSItemType ItemType, int32 Quantity = 1, int32 CropId = -1) const;
	UFUNCTION(BlueprintCallable, Category="Inventory|Bag")
	bool AddItem(EPSItemType ItemType, int32 Quantity = 1, int32 CropId = -1);
	UFUNCTION(BlueprintPure, Category="Inventory|Bag")
	bool CanAddItemVariant(EPSItemType ItemType, FName ItemId, int32 Quantity = 1, int32 CropId = -1,
		int32 CurrentDurability = -1) const;
	UFUNCTION(BlueprintCallable, Category="Inventory|Bag")
	bool AddItemVariant(EPSItemType ItemType, FName ItemId, int32 Quantity = 1, int32 CropId = -1,
		int32 CurrentDurability = -1);
	/** Removes matching items from the bag first and then the hotbar. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RemoveItem(EPSItemType ItemType, int32 Quantity = 1, int32 CropId = -1);
	UFUNCTION(BlueprintCallable, Category="Inventory|Hotbar")
	bool RemoveFromHotbarSlot(int32 SlotIndex, int32 Quantity = 1);
	/** Applies durability loss to one hotbar tool. A tool is removed when it reaches zero. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Durability")
	bool ConsumeHotbarDurability(int32 SlotIndex, int32 Amount, bool& bDestroyed);
	/** Applies durability loss to the first matching owned item, preferring the bag. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Durability")
	bool ConsumeItemDurability(EPSItemType ItemType, int32 Amount, bool& bDestroyed);
	UFUNCTION(BlueprintCallable, Category="Inventory|Durability")
	bool RestoreDurability(EPSInventoryArea Area, int32 SlotIndex, int32 Amount);
	UFUNCTION(BlueprintPure, Category="Inventory|Durability")
	float GetDurabilityRatio(EPSInventoryArea Area, int32 SlotIndex) const;
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool MoveItem(EPSInventoryArea FromArea, int32 FromIndex, EPSInventoryArea ToArea, int32 ToIndex);
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void ResetToDefaults();
	UFUNCTION(BlueprintCallable, Category="Inventory|Save")
	bool SaveInventory() const;
	UFUNCTION(BlueprintCallable, Category="Inventory|Save")
	bool LoadInventory();

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FPSInventoryChanged OnInventoryChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Save")
	FString SaveSlotName = TEXT("PeaceSignInventory");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Save")
	bool bAutoSave = true;

private:
	friend class FPSInventoryPreviewTest;
	void InitializeDefaults();
	void NotifyChanged();
	bool IsAreaSlotUnlocked(EPSInventoryArea Area, int32 Index) const;
	TArray<FPSItemStack>& GetAreaSlots(EPSInventoryArea Area);
	const TArray<FPSItemStack>& GetAreaSlots(EPSInventoryArea Area) const;
	UPROPERTY(SaveGame)
	TArray<FPSItemStack> HotbarSlots;
	UPROPERTY(SaveGame)
	TArray<FPSItemStack> BagSlots;
	UPROPERTY(SaveGame)
	int32 UnlockedBagSlotCount = 10;
};
