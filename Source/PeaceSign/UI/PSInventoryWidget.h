#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "PSInventoryWidget.generated.h"

class UBorder;
class UTextBlock;
class UTexture2D;
class UPSInventoryWidget;

/** Presentation-only sample. Replace with inventory item data when gameplay inventory is ready. */
USTRUCT(BlueprintType)
struct FPSInventoryPreviewItem
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UTexture2D> Icon = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlaceholderIcon = 0;
};

UCLASS()
class PEACESIGN_API UPSInventoryDragOperation : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UPSInventoryWidget> Inventory;
	int32 SourceIndex = INDEX_NONE;
};

/** Shared procedural slot; final art can replace its brushes and item textures. */
UCLASS()
class PEACESIGN_API UPSInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Setup(UPSInventoryWidget* InInventory, int32 InIndex);
protected:
	virtual int32 NativePaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual void NativeOnDragDetected(const FGeometry&, const FPointerEvent&, UDragDropOperation*&) override;
	virtual bool NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
	virtual void NativeOnMouseEnter(const FGeometry&, const FPointerEvent&) override;
	virtual void NativeOnMouseLeave(const FPointerEvent&) override;
private:
	UPROPERTY() TObjectPtr<UPSInventoryWidget> Inventory;
	int32 Index = INDEX_NONE;
};

UCLASS()
class PEACESIGN_API UPSInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPSInventoryWidget(const FObjectInitializer& ObjectInitializer);
	const FPSInventoryPreviewItem* GetItem(int32 Index) const;
	bool IsUnlocked(int32 Index) const { return Index >= 0 && Index < 10; }
	bool MoveItem(int32 From, int32 To);
	void SelectItem(int32 Index);
	int32 GetSelectedIndex() const { return SelectedIndex; }
	UFUNCTION(BlueprintCallable, Category="Inventory") void CloseInventory();
	// These brushes accept artist textures in a Widget Blueprint child without changing interaction code.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Art") FSlateBrush SlotBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Art") FSlateBrush SelectedSlotBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Art") FSlateBrush HoverSlotBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Preview") TArray<FPSInventoryPreviewItem> PreviewItems;
protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
private:
	friend class FPSInventoryPreviewTest;
	UPROPERTY() TObjectPtr<UTextBlock> DetailName;
	UPROPERTY() TObjectPtr<UTextBlock> DetailDescription;
	UPROPERTY() TObjectPtr<UTextBlock> DetailCount;
	UPROPERTY() TObjectPtr<UTextBlock> CapacityLabel;
	int32 SelectedIndex = 0;
};
