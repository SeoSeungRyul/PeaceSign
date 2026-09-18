#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Inventory/PSInventoryComponent.h"
#include "PSHotbarWidget.generated.h"

class UPSHotbarWidget;

UCLASS()
class PEACESIGN_API UPSHotbarSlotWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Setup(UPSHotbarWidget* InHotbar, int32 InIndex);
protected:
	virtual int32 NativePaint(const FPaintArgs&, const FGeometry&, const FSlateRect&,
		FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual void NativeOnDragDetected(const FGeometry&, const FPointerEvent&, UDragDropOperation*&) override;
	virtual bool NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
	virtual void NativeOnMouseEnter(const FGeometry&, const FPointerEvent&) override;
private:
	UPROPERTY() TObjectPtr<UPSHotbarWidget> Hotbar;
	int32 Index = INDEX_NONE;
};

UCLASS()
class PEACESIGN_API UPSHotbarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetInventoryComponent(UPSInventoryComponent* InInventory);
	UPSInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	const FPSItemStack* GetItem(int32 Index) const;
	bool IsSelected(int32 Index) const;
	bool MoveItem(EPSInventoryArea FromArea, int32 FromIndex, int32 ToIndex);
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	friend class FPSInventoryPreviewTest;
	UFUNCTION() void RefreshHotbar();
	UPROPERTY(Transient) TObjectPtr<UPSInventoryComponent> InventoryComponent;
};
