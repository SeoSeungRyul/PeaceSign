#include "PSInventoryWidget.h"
#include "../PSPlayerController.h"
#include "../Inventory/PSInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	FLinearColor Hex(const TCHAR* Value) { return FLinearColor(FColor::FromHex(Value)); }
	UTextBlock* Label(UWidgetTree* Tree, const FString& Text, int32 Size, FLinearColor Color)
	{
		auto* Result = Tree->ConstructWidget<UTextBlock>();
		Result->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Result->GetFont();
		Font.Size = Size;
		Result->SetFont(Font);
		Result->SetColorAndOpacity(Color);
		return Result;
	}
}

UPSInventoryWidget::UPSInventoryWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SlotBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");
	SlotBrush.TintColor = Hex(TEXT("D7A15C"));
	SelectedSlotBrush = SlotBrush;
	SelectedSlotBrush.TintColor = Hex(TEXT("FF8F00"));
	HoverSlotBrush = SlotBrush;
	HoverSlotBrush.TintColor = Hex(TEXT("FFD54F"));
}

void UPSInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	auto* Shade = WidgetTree->ConstructWidget<UBorder>();
	Shade->SetBrushColor(FLinearColor(0.008f, 0.012f, 0.01f, 0.78f));
	auto* ShadeSlot = Canvas->AddChildToCanvas(Shade);
	ShadeSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ShadeSlot->SetOffsets(FMargin(0));
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	auto* ScaleSlot = Canvas->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ScaleSlot->SetOffsets(FMargin(32));
	auto* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(1100);
	Size->SetHeightOverride(680);
	Scale->SetContent(Size);
	auto* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrushColor(Hex(TEXT("3E2720")));
	Frame->SetPadding(FMargin(4));
	Size->SetContent(Frame);
	auto* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(Hex(TEXT("302A23")));
	Panel->SetPadding(FMargin(28));
	Frame->SetContent(Panel);
	auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Rows);
	auto* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Rows->AddChildToVerticalBox(Header);
	auto* Heading = WidgetTree->ConstructWidget<UVerticalBox>();
	Header->AddChildToHorizontalBox(Heading)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Heading->AddChildToVerticalBox(Label(WidgetTree, TEXT("PEACE SIGN  /  INVENTORY"), 12, Hex(TEXT("BFA885"))));
	Heading->AddChildToVerticalBox(Label(WidgetTree, TEXT("나의 가방"), 32, Hex(TEXT("FFF0D1"))))->SetPadding(FMargin(0, 5, 0, 0));
	auto* Close = WidgetTree->ConstructWidget<UButton>();
	Close->SetBackgroundColor(Hex(TEXT("73543B")));
	Close->SetContent(Label(WidgetTree, TEXT("  닫기  [I / Esc]  "), 15, Hex(TEXT("FFF0D1"))));
	Close->OnClicked.AddDynamic(this, &ThisClass::CloseInventory);
	Header->AddChildToHorizontalBox(Close)->SetVerticalAlignment(VAlign_Center);
	Rows->AddChildToVerticalBox(Label(WidgetTree, TEXT("가방과 화면 아래 단축 슬롯 사이로 아이템을 옮길 수 있습니다."), 14, Hex(TEXT("C6B59B"))))->SetPadding(FMargin(0, 12, 0, 22));
	auto* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	Rows->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto* Bag = WidgetTree->ConstructWidget<UVerticalBox>();
	Body->AddChildToHorizontalBox(Bag)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CapacityLabel = Label(WidgetTree, TEXT(""), 16, Hex(TEXT("EAD2AC")));
	Bag->AddChildToVerticalBox(CapacityLabel)->SetPadding(FMargin(0, 0, 0, 14));
	auto* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Grid->SetSlotPadding(FMargin(3));
	Bag->AddChildToVerticalBox(Grid);
	for (int32 Index = 0; Index < 50; ++Index)
	{
		auto* SlotSize = WidgetTree->ConstructWidget<USizeBox>();
		SlotSize->SetWidthOverride(64);
		SlotSize->SetHeightOverride(64);
		auto* Cell = CreateWidget<UPSInventorySlotWidget>(this);
		Cell->Setup(this, Index);
		SlotSize->SetContent(Cell);
		Grid->AddChildToUniformGrid(SlotSize, Index / 10, Index % 10);
	}
	Bag->AddChildToVerticalBox(Label(WidgetTree, TEXT("현재 가방 10칸 · 잠긴 줄은 가방 확장 시 열립니다"), 13, Hex(TEXT("BDA687"))))->SetPadding(FMargin(3, 16, 0, 0));
	Bag->AddChildToVerticalBox(Label(WidgetTree, TEXT("다음 확장  20칸 / 1,000골드   ·   구매 기능 준비 중"), 13, Hex(TEXT("BDA687"))))->SetPadding(FMargin(3, 8, 0, 0));
	auto* DetailSize = WidgetTree->ConstructWidget<USizeBox>();
	DetailSize->SetWidthOverride(280);
	Body->AddChildToHorizontalBox(DetailSize)->SetPadding(FMargin(20, 0, 0, 0));
	auto* Detail = WidgetTree->ConstructWidget<UBorder>();
	Detail->SetBrushColor(Hex(TEXT("25221D")));
	Detail->SetPadding(FMargin(22));
	DetailSize->SetContent(Detail);
	auto* DetailRows = WidgetTree->ConstructWidget<UVerticalBox>();
	Detail->SetContent(DetailRows);
	DetailRows->AddChildToVerticalBox(Label(WidgetTree, TEXT("선택한 아이템"), 13, Hex(TEXT("BDA687"))));
	DetailName = Label(WidgetTree, TEXT(""), 25, Hex(TEXT("FFF0D1")));
	DetailRows->AddChildToVerticalBox(DetailName)->SetPadding(FMargin(0, 18, 0, 12));
	DetailCount = Label(WidgetTree, TEXT(""), 15, Hex(TEXT("FFD18A")));
	DetailRows->AddChildToVerticalBox(DetailCount)->SetPadding(FMargin(0, 0, 0, 24));
	DetailDescription = Label(WidgetTree, TEXT(""), 16, Hex(TEXT("D1C4AF")));
	DetailDescription->SetAutoWrapText(true);
	DetailRows->AddChildToVerticalBox(DetailDescription)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto* Note = Label(WidgetTree, TEXT("아이템 이동과 수량은\n자동으로 저장됩니다."), 13, Hex(TEXT("AC977B")));
	DetailRows->AddChildToVerticalBox(Note);
	Rows->AddChildToVerticalBox(Label(WidgetTree, TEXT("클릭  선택     /     드래그  이동 · 교환     /     I 또는 Esc  닫기"), 14, Hex(TEXT("EAD2AC"))))->SetPadding(FMargin(0, 20, 0, 0));
	SelectItem(0);
}

void UPSInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshInventory);
	RefreshInventory();
}

void UPSInventoryWidget::NativeDestruct()
{
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshInventory);
	Super::NativeDestruct();
}

void UPSInventoryWidget::SetInventoryComponent(UPSInventoryComponent* InInventory)
{
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshInventory);
	InventoryComponent = InInventory;
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshInventory);
	RefreshInventory();
}

const FPSItemStack* UPSInventoryWidget::GetItem(const int32 Index) const
{
	if (!InventoryComponent || !InventoryComponent->IsBagSlotUnlocked(Index)) return nullptr;
	const FPSItemStack* Item = InventoryComponent->FindBagSlot(Index);
	return Item && !Item->IsEmpty() ? Item : nullptr;
}

bool UPSInventoryWidget::IsUnlocked(const int32 Index) const
{
	return InventoryComponent && InventoryComponent->IsBagSlotUnlocked(Index);
}

bool UPSInventoryWidget::MoveItem(int32 From, int32 To)
{
	if (!InventoryComponent || !InventoryComponent->MoveItem(EPSInventoryArea::Bag, From, EPSInventoryArea::Bag, To)) return false;
	SelectItem(To);
	return true;
}

void UPSInventoryWidget::SelectItem(int32 Index)
{
	if (!IsUnlocked(Index)) return;
	SelectedIndex = Index;
	if (!DetailName) return;
	const auto* Item = GetItem(Index);
	const FPSItemDefinition& Definition = PSItems::GetDefinition(Item ? Item->ItemType : EPSItemType::None);
	DetailName->SetText(Definition.Name);
	DetailCount->SetText(Item ? FText::FromString(FString::Printf(TEXT("보유 수량   %d"), Item->Quantity)) : FText::GetEmpty());
	DetailDescription->SetText(Definition.Description);
	int32 Occupied = 0;
	const int32 Unlocked = InventoryComponent ? InventoryComponent->GetUnlockedBagSlotCount() : 0;
	for (int32 ItemIndex = 0; ItemIndex < Unlocked; ++ItemIndex) Occupied += GetItem(ItemIndex) ? 1 : 0;
	CapacityLabel->SetText(FText::FromString(FString::Printf(TEXT("가방   /   1단계                                      %d / %d칸 사용"), Occupied, Unlocked)));
}

void UPSInventoryWidget::RefreshInventory()
{
	SelectItem(SelectedIndex);
	InvalidateLayoutAndVolatility();
}

void UPSInventoryWidget::CloseInventory()
{
	if (auto* Controller = Cast<APSPlayerController>(GetOwningPlayer())) Controller->CloseInventory();
}

FReply UPSInventoryWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::I || Event.GetKey() == EKeys::Escape)
	{
		CloseInventory();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UPSInventorySlotWidget::Setup(UPSInventoryWidget* InInventory, int32 InIndex)
{
	Inventory = InInventory;
	Index = InIndex;
	SetVisibility(ESlateVisibility::Visible);
	ForceVolatile(true);
}

int32 UPSInventorySlotWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool Enabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, Enabled);
	if (!Inventory) return Layer;
	const FVector2D Scale = Geometry.GetLocalSize() / 64.0;
	const auto Rect = [&](float X, float Y, float W, float H, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Elements, ++Layer,
			Geometry.ToPaintGeometry(FVector2D(W, H) * Scale, FSlateLayoutTransform(FVector2D(X, Y) * Scale)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
	};
	const bool Unlocked = Inventory->IsUnlocked(Index);
	const bool Selected = Index == Inventory->GetSelectedIndex();
	Rect(0, 0, 64, 64, Hex(TEXT("1C1713")));
	const FSlateBrush& Brush = Selected ? Inventory->SelectedSlotBrush : IsHovered() && Unlocked ? Inventory->HoverSlotBrush : Inventory->SlotBrush;
	FSlateDrawElement::MakeBox(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2D(60, 60) * Scale, FSlateLayoutTransform(FVector2D(2, 2) * Scale)),
		&Brush, ESlateDrawEffect::None, Unlocked ? Brush.GetTint(Style) : Hex(TEXT("494035")));
	if (!Brush.GetResourceObject() || !Unlocked)
	{
		Rect(5, 5, 54, 54, Unlocked ? Hex(IsHovered() ? TEXT("C79559") : TEXT("B88951")) : Hex(TEXT("363129")));
		Rect(5, 5, 54, 3, Unlocked ? Hex(TEXT("8C613B")) : Hex(TEXT("2C2822")));
	}
	if (!Unlocked)
	{
		Rect(26, 25, 12, 13, Hex(TEXT("655B4B")));
		Rect(28, 20, 8, 3, Hex(TEXT("655B4B")));
		Rect(26, 22, 3, 6, Hex(TEXT("655B4B")));
		Rect(35, 22, 3, 6, Hex(TEXT("655B4B")));
		return Layer;
	}
	const auto* Item = Inventory->GetItem(Index);
	if (!Item) return Layer;
	const FPSItemDefinition& Definition = PSItems::GetDefinition(Item->ItemType);
	const TObjectPtr<UTexture2D>* IconTexture = Inventory->ItemIcons.Find(Item->ItemType);
	if (IconTexture && IconTexture->Get())
	{
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(IconTexture->Get());
		FSlateDrawElement::MakeBox(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2D(40, 40) * Scale, FSlateLayoutTransform(FVector2D(12, 10) * Scale)), &IconBrush);
	}
	else
	{
		const FLinearColor C = Definition.Color;
		switch (Definition.PlaceholderIcon)
		{
		case 0: Rect(29, 16, 5, 34, Hex(TEXT("735037"))); Rect(17, 14, 29, 8, C); Rect(16, 20, 9, 8, C); break;
		case 1: Rect(20, 24, 24, 23, Hex(TEXT("E5CE95"))); Rect(24, 20, 16, 5, Hex(TEXT("765A36"))); Rect(30, 28, 4, 13, C); Rect(23, 27, 8, 5, C); Rect(33, 24, 7, 6, C); break;
		case 2: Rect(24, 25, 19, 17, C); Rect(28, 42, 11, 6, C); Rect(29, 17, 4, 10, Hex(TEXT("63884D"))); Rect(34, 16, 8, 5, Hex(TEXT("86A95E"))); break;
		case 3: Rect(18, 23, 28, 9, C); Rect(21, 34, 27, 10, C); Rect(21, 26, 21, 2, Hex(TEXT("775136"))); Rect(24, 38, 20, 2, Hex(TEXT("775136"))); break;
		case 4: Rect(21, 23, 22, 22, C); Rect(17, 30, 30, 11, C); Rect(25, 20, 13, 4, C); Rect(24, 26, 12, 4, Hex(TEXT("D3D6D8"))); break;
		default: Rect(19, 25, 22, 15, C); Rect(23, 21, 13, 23, C); Rect(41, 21, 7, 23, C); Rect(21, 28, 3, 3, Hex(TEXT("243C3A"))); break;
		}
	}
	const FString Count = FString::FromInt(Item->Quantity);
	const float X = 57.0f - Count.Len() * 8.0f;
	Rect(X - 3, 44, Count.Len() * 8 + 5, 16, FLinearColor(0.045f, 0.032f, 0.02f, 0.85f));
	FSlateDrawElement::MakeText(Elements, ++Layer,
		Geometry.ToPaintGeometry(FVector2D(50, 18) * Scale, FSlateLayoutTransform(FVector2D(X, 44) * Scale)),
		Count, FCoreStyle::GetDefaultFontStyle("Bold", 11), ESlateDrawEffect::None, Hex(TEXT("FFF0D1")));
	return Layer;
}

FReply UPSInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Inventory && Inventory->IsUnlocked(Index) && Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Inventory->SelectItem(Index);
		if (Inventory->GetItem(Index)) return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		return FReply::Handled();
	}
	return FReply::Handled();
}

void UPSInventorySlotWidget::NativeOnDragDetected(const FGeometry&, const FPointerEvent&, UDragDropOperation*& Operation)
{
	if (!Inventory || !Inventory->GetItem(Index)) return;
	auto* Drag = NewObject<UPSInventoryDragOperation>(this);
	Drag->InventoryComponent = Inventory->GetInventoryComponent();
	Drag->SourceArea = EPSInventoryArea::Bag;
	Drag->SourceIndex = Index;
	auto* Visual = NewObject<USizeBox>(Drag);
	Visual->SetWidthOverride(64);
	Visual->SetHeightOverride(64);
	auto* Cell = CreateWidget<UPSInventorySlotWidget>(this);
	Cell->Setup(Inventory, Index);
	Visual->SetContent(Cell);
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
	Drag->DefaultDragVisual = Visual;
	Drag->Pivot = EDragPivot::CenterCenter;
	Operation = Drag;
}

bool UPSInventorySlotWidget::NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation* Operation)
{
	auto* Drag = Cast<UPSInventoryDragOperation>(Operation);
	if (!Drag || !Inventory || Drag->InventoryComponent != Inventory->GetInventoryComponent()) return false;
	const bool bMoved = Drag->InventoryComponent->MoveItem(
		Drag->SourceArea, Drag->SourceIndex, EPSInventoryArea::Bag, Index);
	if (bMoved) Inventory->SelectItem(Index);
	return bMoved;
}

void UPSInventorySlotWidget::NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Super::NativeOnMouseEnter(Geometry, Event);
	if (Inventory)
	{
		const auto* Item = Inventory->GetItem(Index);
		SetToolTipText(Item ? PSItems::GetDefinition(Item->ItemType).Name : FText::FromString(Inventory->IsUnlocked(Index) ? TEXT("빈 슬롯") : TEXT("가방 확장이 필요합니다")));
	}
}

void UPSInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{
	Super::NativeOnMouseLeave(Event);
}
