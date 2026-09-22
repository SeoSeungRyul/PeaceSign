#include "PSHotbarWidget.h"
#include "PSInventoryWidget.h"
#include "../PSPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	FLinearColor HotbarHex(const TCHAR* Value) { return FLinearColor(FColor::FromHex(Value)); }
}

void UPSHotbarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetPadding(FMargin(7.0f));
	Frame->SetBrushColor(HotbarHex(TEXT("211A14E8")));
	UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame);
	FrameSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	FrameSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	FrameSlot->SetPosition(FVector2D::ZeroVector);
	FrameSlot->SetSize(FVector2D(700.0f, 78.0f));
	UHorizontalBox* Slots = WidgetTree->ConstructWidget<UHorizontalBox>();
	Frame->SetContent(Slots);
	for (int32 Index = 0; Index < UPSInventoryComponent::HotbarSlotCount; ++Index)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(64.0f);
		Size->SetHeightOverride(64.0f);
		UPSHotbarSlotWidget* HotbarCell = CreateWidget<UPSHotbarSlotWidget>(this);
		HotbarCell->Setup(this, Index);
		Size->SetContent(HotbarCell);
		Slots->AddChildToHorizontalBox(Size)->SetPadding(FMargin(2.0f));
	}
}

void UPSHotbarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshHotbar);
	RefreshHotbar();
}

void UPSHotbarWidget::NativeDestruct()
{
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshHotbar);
	Super::NativeDestruct();
}

void UPSHotbarWidget::SetInventoryComponent(UPSInventoryComponent* InInventory)
{
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshHotbar);
	InventoryComponent = InInventory;
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshHotbar);
	RefreshHotbar();
}

const FPSItemStack* UPSHotbarWidget::GetItem(const int32 Index) const
{
	const FPSItemStack* Item = InventoryComponent ? InventoryComponent->FindHotbarSlot(Index) : nullptr;
	return Item && !Item->IsEmpty() ? Item : nullptr;
}

bool UPSHotbarWidget::IsSelected(const int32 Index) const
{
	const APSPlayerController* Controller = Cast<APSPlayerController>(GetOwningPlayer());
	return Controller && Controller->GetSelectedHotbarSlot() == Index;
}

bool UPSHotbarWidget::MoveItem(const EPSInventoryArea FromArea, const int32 FromIndex, const int32 ToIndex)
{
	return InventoryComponent && InventoryComponent->MoveItem(
		FromArea, FromIndex, EPSInventoryArea::Hotbar, ToIndex);
}

void UPSHotbarWidget::RefreshHotbar()
{
	InvalidateLayoutAndVolatility();
}

void UPSHotbarSlotWidget::Setup(UPSHotbarWidget* InHotbar, const int32 InIndex)
{
	Hotbar = InHotbar;
	Index = InIndex;
	SetVisibility(ESlateVisibility::Visible);
	ForceVolatile(true);
}

int32 UPSHotbarSlotWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& Culling, FSlateWindowElementList& Elements, int32 Layer,
	const FWidgetStyle& Style, const bool Enabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, Enabled);
	if (!Hotbar) return Layer;
	const FVector2D Scale = Geometry.GetLocalSize() / 64.0f;
	const auto Rect = [&](const float X, const float Y, const float W, const float H, const FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Elements, ++Layer,
			Geometry.ToPaintGeometry(FVector2D(W, H) * Scale, FSlateLayoutTransform(FVector2D(X, Y) * Scale)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
	};
	Rect(0, 0, 64, 64, HotbarHex(TEXT("17120E")));
	Rect(3, 3, 58, 58, IsHovered() ? HotbarHex(TEXT("C79559")) : HotbarHex(TEXT("96683E")));
	if (Hotbar->IsSelected(Index))
	{
		Rect(1, 1, 62, 4, HotbarHex(TEXT("8FE36A")));
		Rect(1, 59, 62, 4, HotbarHex(TEXT("8FE36A")));
	}
	const FString Key = Index == 9 ? TEXT("0") : FString::FromInt(Index + 1);
	FSlateDrawElement::MakeText(Elements, ++Layer,
		Geometry.ToPaintGeometry(FVector2D(14, 14) * Scale, FSlateLayoutTransform(FVector2D(7, 5) * Scale)),
		Key, FCoreStyle::GetDefaultFontStyle("Bold", 9), ESlateDrawEffect::None, HotbarHex(TEXT("FFF0D1")));
	const FPSItemStack* Item = Hotbar->GetItem(Index);
	if (!Item) return Layer;
	const FPSItemDefinition& Definition = PSItems::GetDefinition(*Item);
	const FLinearColor Color = Definition.Color;
	switch (Definition.PlaceholderIcon)
	{
	case 0: Rect(29, 16, 5, 34, HotbarHex(TEXT("735037"))); Rect(17, 14, 29, 8, Color); break;
	case 1: Rect(20, 24, 24, 23, HotbarHex(TEXT("E5CE95"))); Rect(24, 20, 16, 5, HotbarHex(TEXT("765A36"))); Rect(30, 28, 4, 13, Color); break;
	case 2: Rect(24, 25, 19, 17, Color); Rect(28, 42, 11, 6, Color); Rect(34, 16, 8, 5, HotbarHex(TEXT("86A95E"))); break;
	case 3: Rect(18, 23, 28, 9, Color); Rect(21, 34, 27, 10, Color); break;
	case 4: Rect(21, 23, 22, 22, Color); Rect(17, 30, 30, 11, Color); break;
	default: Rect(19, 25, 22, 15, Color); Rect(23, 21, 13, 23, Color); Rect(41, 21, 7, 23, Color); break;
	}
	if (Definition.bInfiniteDurability)
	{
		Rect(8, 53, 48, 4, HotbarHex(TEXT("B99BFF")));
	}
	else if (Definition.MaxDurability > 0)
	{
		Rect(8, 53, 48, 4, HotbarHex(TEXT("33251C")));
		const float Ratio = FMath::Clamp(static_cast<float>(Item->CurrentDurability) / Definition.MaxDurability, 0.0f, 1.0f);
		Rect(8, 53, 48 * Ratio, 4, Ratio > 0.3f ? HotbarHex(TEXT("78C679")) : HotbarHex(TEXT("E05A47")));
	}
	if (Definition.MaxStack > 1 || Item->Quantity > 1)
	{
		const FString Count = FString::FromInt(Item->Quantity);
		const float X = 57.0f - Count.Len() * 8.0f;
		Rect(X - 3, 44, Count.Len() * 8 + 5, 16, FLinearColor(0.045f, 0.032f, 0.02f, 0.85f));
		FSlateDrawElement::MakeText(Elements, ++Layer,
			Geometry.ToPaintGeometry(FVector2D(50, 18) * Scale, FSlateLayoutTransform(FVector2D(X, 44) * Scale)),
			Count, FCoreStyle::GetDefaultFontStyle("Bold", 11), ESlateDrawEffect::None, HotbarHex(TEXT("FFF0D1")));
	}
	return Layer;
}

FReply UPSHotbarSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!Hotbar || Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	if (APSPlayerController* Controller = Cast<APSPlayerController>(GetOwningPlayer())) Controller->SelectHotbarSlot(Index);
	return Hotbar->GetItem(Index)
		? FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton)
		: FReply::Handled();
}

void UPSHotbarSlotWidget::NativeOnDragDetected(const FGeometry&, const FPointerEvent&, UDragDropOperation*& Operation)
{
	if (!Hotbar || !Hotbar->GetItem(Index)) return;
	UPSInventoryDragOperation* Drag = NewObject<UPSInventoryDragOperation>(this);
	Drag->InventoryComponent = Hotbar->GetInventoryComponent();
	Drag->SourceArea = EPSInventoryArea::Hotbar;
	Drag->SourceIndex = Index;
	USizeBox* Visual = NewObject<USizeBox>(Drag);
	Visual->SetWidthOverride(64.0f);
	Visual->SetHeightOverride(64.0f);
	UPSHotbarSlotWidget* HotbarCell = CreateWidget<UPSHotbarSlotWidget>(this);
	HotbarCell->Setup(Hotbar, Index);
	Visual->SetContent(HotbarCell);
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
	Drag->DefaultDragVisual = Visual;
	Drag->Pivot = EDragPivot::CenterCenter;
	Operation = Drag;
}

bool UPSHotbarSlotWidget::NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation* Operation)
{
	UPSInventoryDragOperation* Drag = Cast<UPSInventoryDragOperation>(Operation);
	return Drag && Hotbar && Drag->InventoryComponent == Hotbar->GetInventoryComponent()
		&& Hotbar->MoveItem(Drag->SourceArea, Drag->SourceIndex, Index);
}

void UPSHotbarSlotWidget::NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Super::NativeOnMouseEnter(Geometry, Event);
	if (!Hotbar) return;
	const FPSItemStack* Item = Hotbar->GetItem(Index);
	SetToolTipText(Item ? PSItems::GetDefinition(*Item).Name : FText::FromString(TEXT("빈 단축 슬롯")));
}
