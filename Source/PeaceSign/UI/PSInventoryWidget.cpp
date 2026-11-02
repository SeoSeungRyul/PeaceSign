#include "PSInventoryWidget.h"
#include "PSHotbarWidget.h"
#include "../PSPlayerController.h"
#include "../Inventory/PSInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace
{
	FLinearColor Hex(const TCHAR* Value) { return FLinearColor(FColor::FromHex(Value)); }
	UTextBlock* Label(UWidgetTree* Tree, const FString& Text, int32 Size, FLinearColor Color)
	{
		auto* Result = Tree->ConstructWidget<UTextBlock>();
		Result->SetText(FText::FromString(Text));
		FSlateFontInfo Font = UPSUIWidget::WithProjectFont(Result->GetFont());
		Font.Size = Size;
		Result->SetFont(Font);
		Result->SetColorAndOpacity(Color);
		return Result;
	}

	// Nested UserWidgets own separate trees. Is Variable is not required for name lookup.
	void CollectDesignerWidgets(UUserWidget* Owner, TArray<UWidget*>& Out)
	{
		if (!Owner || !Owner->WidgetTree) return;
		Owner->WidgetTree->ForEachWidget([&Out](UWidget* Widget)
		{
			Out.Add(Widget);
			if (UUserWidget* Child = Cast<UUserWidget>(Widget)) CollectDesignerWidgets(Child, Out);
		});
	}

	UOverlay* WrapDesignerImage(UWidgetTree* Tree, UImage* Image)
	{
		UPanelWidget* Parent = Image ? Image->GetParent() : nullptr;
		if (!Parent) return nullptr;
		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>();
		if (!Parent->ReplaceChildAt(Parent->GetChildIndex(Image), Overlay)) return nullptr;
		Image->Slot = nullptr;
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* Art = Overlay->AddChildToOverlay(Image);
		Art->SetHorizontalAlignment(HAlign_Fill);
		Art->SetVerticalAlignment(VAlign_Fill);
		return Overlay;
	}

	bool IsBookCover(const UImage* Image)
	{
		const UObject* Resource = Image ? Image->GetBrush().GetResourceObject() : nullptr;
		return Resource && Resource->GetFName() == TEXT("Inven_Background");
	}

#if WITH_EDITOR
	bool AttachDesignerBookTabs(UWidgetTree* Tree)
	{
		UWidget* Navigation = Tree->FindWidget(TEXT("Button_Information"));
		UImage* Cover = nullptr;
		Tree->ForEachWidget([&Cover](UWidget* Widget) { if (auto* Image = Cast<UImage>(Widget); IsBookCover(Image)) Cover = Image; });
		if (!Navigation || !Cover) return false;
		const auto CanvasBranch = [](UWidget* Widget)
		{
			while (Widget && !Cast<UCanvasPanelSlot>(Widget->Slot)) Widget = Widget->GetParent();
			return Widget;
		};
		Navigation = CanvasBranch(Navigation);
		UWidget* CoverBranch = CanvasBranch(Cover);
		if (!Navigation || !CoverBranch || Navigation == CoverBranch) return false;
		auto* NavigationSlot = CastChecked<UCanvasPanelSlot>(Navigation->Slot);
		auto* CoverSlot = CastChecked<UCanvasPanelSlot>(CoverBranch->Slot);
		Navigation->Modify(); NavigationSlot->Modify(); Cover->Modify();
		NavigationSlot->SetZOrder(CoverSlot->GetZOrder() - 1);
		FVector2D Translation = Navigation->GetRenderTransform().Translation; Translation.Y = 0;
		Navigation->SetRenderTranslation(Translation);
		Cover->SetVisibility(ESlateVisibility::Visible);
		return true;
	}
#endif
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
	if (WidgetTree->RootWidget)
	{
		// Never overwrite an artist-authored tree, even when a binding is missing.
		bDesignerLayout = true;
		if (!HotbarHost) ConnectDesignerLayout();
		return;
	}
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
	Size->SetHeightOverride(800);
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
	Rows->AddChildToVerticalBox(Label(WidgetTree, TEXT("가방과 위 Hotbar 사이로 아이템을 옮길 수 있습니다."), 14, Hex(TEXT("C6B59B"))))->SetPadding(FMargin(0, 12, 0, 22));
	auto* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	Rows->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto* Bag = WidgetTree->ConstructWidget<UVerticalBox>();
	Body->AddChildToHorizontalBox(Bag)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Bag->AddChildToVerticalBox(Label(WidgetTree, TEXT("HOTBAR   /   1~0 빠른 선택"), 13, Hex(TEXT("BDA687"))))->SetPadding(FMargin(0, 0, 0, 8));
	HotbarHost = WidgetTree->ConstructWidget<USizeBox>();
	HotbarHost->SetWidthOverride(700.0f);
	HotbarHost->SetHeightOverride(78.0f);
	Bag->AddChildToVerticalBox(HotbarHost)->SetPadding(FMargin(0, 0, 0, 18));
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
	if (bDesignerLayout && !HotbarHost) ConnectDesignerLayout();
	if (InventoryComponent) InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshInventory);
	RefreshInventory();
}

bool UPSInventoryWidget::ConnectDesignerLayout()
{
	TArray<UWidget*> Widgets;
	CollectDesignerWidgets(this, Widgets);
	UUniformGridPanel* Grid = nullptr;
	UImage* HotbarArt = nullptr;
	for (UWidget* Widget : Widgets)
	{
		if (Widget->GetFName() == TEXT("BagGrid")) Grid = Cast<UUniformGridPanel>(Widget);
		if (Widget->GetFName() == TEXT("MiniLine1")) HotbarArt = Cast<UImage>(Widget);
		if (UImage* Image = Cast<UImage>(Widget)) Image->SetVisibility(IsBookCover(Image) ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	}
	if (!Grid || !HotbarArt)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory designer requires BagGrid and MiniLine1 in its nested widget trees."));
		return false;
	}
	TArray<UImage*> Rows;
	for (UWidget* Child : Grid->GetAllChildren())
		if (UImage* Image = Cast<UImage>(Child)) Rows.Add(Image);
	Rows.Sort([](const UImage& A, const UImage& B)
	{
		return CastChecked<UUniformGridSlot>(A.Slot)->GetRow() < CastChecked<UUniformGridSlot>(B.Slot)->GetRow();
	});
	if (Rows.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory BagGrid requires at least one row artwork image."));
		return false;
	}
	// Preserve both the authored row height/padding and all existing save slots.
	const int32 RowCount = FMath::DivideAndRoundUp(UPSInventoryComponent::MaxBagSlotCount, 10);
	UImage* Template = Rows.Last();
	while (Rows.Num() < RowCount)
	{
		UImage* Art = WidgetTree->ConstructWidget<UImage>();
		Art->SetBrush(Template->GetBrush());
		Grid->AddChildToUniformGrid(Art, Rows.Num(), 0);
		Rows.Add(Art);
	}
	if (UScrollBoxSlot* GridSlot = Cast<UScrollBoxSlot>(Grid->Slot)) GridSlot->SetVerticalAlignment(VAlign_Top);
	for (int32 Row = 0; Row < RowCount; ++Row)
	{
		UOverlay* Overlay = WrapDesignerImage(WidgetTree, Rows[Row]);
		if (!Overlay) continue;
		UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>();
		UOverlaySlot* CellsSlot = Overlay->AddChildToOverlay(Cells);
		CellsSlot->SetHorizontalAlignment(HAlign_Fill);
		CellsSlot->SetVerticalAlignment(VAlign_Fill);
		for (int32 Column = 0; Column < 10; ++Column)
		{
			UPSInventorySlotWidget* Cell = CreateWidget<UPSInventorySlotWidget>(this);
			Cell->Setup(this, Row * 10 + Column, false);
			Cells->AddChildToHorizontalBox(Cell)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	UOverlay* HotbarOverlay = WrapDesignerImage(WidgetTree, HotbarArt);
	if (!HotbarOverlay) return false;
	HotbarHost = WidgetTree->ConstructWidget<USizeBox>();
	UOverlaySlot* HostSlot = HotbarOverlay->AddChildToOverlay(HotbarHost);
	HostSlot->SetHorizontalAlignment(HAlign_Fill);
	HostSlot->SetVerticalAlignment(VAlign_Fill);
	// Preserve the artist's existing navigation layout, adding interaction over each image.
	for (UWidget* Widget : Widgets)
	{
		UImage* Art = Cast<UImage>(Widget);
		if (!Art || (Art->GetFName() != TEXT("Button_Information") && Art->GetFName() != TEXT("Button_Encyclopedia") && Art->GetFName() != TEXT("Button_Setting"))) continue;
		UOverlay* Tab = WrapDesignerImage(WidgetTree, Art);
		if (!Tab) continue;
		// Continue the empty lower edge beneath the book without lowering or stretching its icon.
		auto* Tail = WidgetTree->ConstructWidget<UImage>(); FSlateBrush TailBrush = Art->GetBrush();
		TailBrush.SetUVRegion(FBox2f(FVector2f(0, .8f), FVector2f(1, 1)));
		TailBrush.ImageSize.Y = 64; Tail->SetBrush(TailBrush); Tail->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* TailSlot = Tab->AddChildToOverlay(Tail);
		TailSlot->SetHorizontalAlignment(HAlign_Fill); TailSlot->SetVerticalAlignment(VAlign_Bottom);
		TailSlot->SetPadding(FMargin(0, 0, 0, -40));
		auto* Button = WidgetTree->ConstructWidget<UButton>();
		FButtonStyle Style; Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Hovered = Style.Normal; Style.Pressed = Style.Normal; Style.Disabled = Style.Normal;
		Style.NormalPadding = FMargin(0); Style.PressedPadding = FMargin(0); Button->SetStyle(Style);
		UOverlaySlot* Hit = Tab->AddChildToOverlay(Button);
		Hit->SetHorizontalAlignment(HAlign_Fill); Hit->SetVerticalAlignment(VAlign_Fill);
		Hit->SetPadding(FMargin(0, 0, 0, -40));
		if (Art->GetFName() == TEXT("Button_Information")) Tab->SetRenderTranslation(FVector2D(0, -24));
		else if (Art->GetFName() == TEXT("Button_Encyclopedia")) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenJournal);
		else Button->SetToolTipText(FText::FromString(TEXT("스킬 (준비 중)")));
	}
	// Evaluate all authored anchors against the same reference canvas, then scale the
	// complete book. Otherwise fixed child coordinates escape a smaller viewport.
	UWidget* AuthoredRoot = WidgetTree->RootWidget;
	UCanvasPanel* Viewport = WidgetTree->ConstructWidget<UCanvasPanel>();
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
	Fit->SetStretch(EStretch::ScaleToFit);
	Fit->SetStretchDirection(EStretchDirection::Both);
	UCanvasPanelSlot* FitSlot = Viewport->AddChildToCanvas(Fit);
	FitSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	FitSlot->SetOffsets(FMargin(16));
	USizeBox* Reference = WidgetTree->ConstructWidget<USizeBox>();
	Reference->SetWidthOverride(FMath::Max(1.0, DesignerReferenceSize.X));
	Reference->SetHeightOverride(FMath::Max(1.0, DesignerReferenceSize.Y));
	Reference->SetContent(AuthoredRoot);
	Fit->SetContent(Reference);
	WidgetTree->RootWidget = Viewport;
	return true;
}

void UPSInventoryWidget::OpenJournal()
{
	if (auto* Controller = Cast<APSPlayerController>(GetOwningPlayer())) Controller->ToggleFishingJournal();
}

bool UPSInventoryWidget::ApplyBookTabArtwork(UObject* Asset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
	if (!Blueprint || !Blueprint->WidgetTree) return false;
	const TCHAR* Names[] = {TEXT("Button_Information"), TEXT("Button_Encyclopedia"), TEXT("Button_Setting")};
	const TCHAR* Images[] = {TEXT("Inven"), TEXT("Encyclopedia"), TEXT("Skill")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* Art = Cast<UImage>(Blueprint->WidgetTree->FindWidget(Names[Index]));
		const FString Path = FString::Printf(TEXT("/Game/UI/Book/Book_Index/Book_Index_%s"), Images[Index]);
		auto* Texture = LoadObject<UTexture2D>(nullptr, *Path);
		if (!Art || !Texture) return false;
		Art->Modify(); Art->SetBrushFromTexture(Texture);
	}
	if (!AttachDesignerBookTabs(Blueprint->WidgetTree)) return false;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); return true;
#else
	return false;
#endif
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
	if (!DetailName || !DetailCount || !DetailDescription || !CapacityLabel) return;
	const auto* Item = GetItem(Index);
	const FPSItemDefinition& Definition = Item ? PSItems::GetDefinition(*Item) : PSItems::GetDefinition(EPSItemType::None);
	DetailName->SetText(Item ? GetItemDisplayName(*Item) : Definition.Name);
	if (Item && Definition.bInfiniteDurability) DetailCount->SetText(FText::FromString(TEXT("내구도   무한")));
	else if (Item && Definition.MaxDurability > 0) DetailCount->SetText(FText::FromString(
		FString::Printf(TEXT("내구도   %d / %d"), Item->CurrentDurability, Definition.MaxDurability)));
	else DetailCount->SetText(Item ? FText::FromString(FString::Printf(TEXT("보유 수량   %d"), Item->Quantity)) : FText::GetEmpty());
	DetailDescription->SetText(Item ? GetItemDescription(*Item) : Definition.Description);
	int32 Occupied = 0;
	const int32 Unlocked = InventoryComponent ? InventoryComponent->GetUnlockedBagSlotCount() : 0;
	for (int32 ItemIndex = 0; ItemIndex < Unlocked; ++ItemIndex) Occupied += GetItem(ItemIndex) ? 1 : 0;
	CapacityLabel->SetText(FText::FromString(FString::Printf(TEXT("가방   /   1단계                                      %d / %d칸 사용"), Occupied, Unlocked)));
}

FText UPSInventoryWidget::GetItemDisplayName(const FPSItemStack& Item) const
{
	const APSPlayerController* Controller = Cast<APSPlayerController>(GetOwningPlayer());
	return Controller ? Controller->GetItemDisplayName(Item) : PSItems::GetDefinition(Item).Name;
}

FText UPSInventoryWidget::GetItemDescription(const FPSItemStack& Item) const
{
	const APSPlayerController* Controller = Cast<APSPlayerController>(GetOwningPlayer());
	return Controller ? Controller->GetItemDescription(Item) : PSItems::GetDefinition(Item).Description;
}

UTexture2D* UPSInventoryWidget::GetItemIcon(const FPSItemStack& Item) const
{
	const APSPlayerController* Controller = Cast<APSPlayerController>(GetOwningPlayer());
	if (Controller)
		if (UTexture2D* ItemIcon = Controller->GetItemIcon(Item)) return ItemIcon;
	const TObjectPtr<UTexture2D>* IconTexture = ItemIcons.Find(Item.ItemType);
	return IconTexture ? IconTexture->Get() : nullptr;
}

void UPSInventoryWidget::AttachHotbar(UPSHotbarWidget* InHotbar)
{
	if (bDesignerLayout && !HotbarHost) ConnectDesignerLayout();
	if (!HotbarHost || !InHotbar || HostedHotbar == InHotbar) return;
	if (HostedHotbar) DetachHotbar();
	InHotbar->RemoveFromParent();
	if (bDesignerLayout)
	{
		// Use the same physical hotbar and interaction code over the artist's row image.
		UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>();
		HotbarHost->SetContent(Cells);
		for (int32 Index = 0; Index < UPSInventoryComponent::HotbarSlotCount; ++Index)
		{
			UPSHotbarSlotWidget* Cell = CreateWidget<UPSHotbarSlotWidget>(this);
			Cell->Setup(InHotbar, Index, false);
			Cells->AddChildToHorizontalBox(Cell)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	else HotbarHost->SetContent(InHotbar);
	HostedHotbar = InHotbar;
}

UPSHotbarWidget* UPSInventoryWidget::DetachHotbar()
{
	UPSHotbarWidget* Result = HostedHotbar;
	if (HotbarHost && Result) HotbarHost->ClearChildren();
	HostedHotbar = nullptr;
	return Result;
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

void UPSInventorySlotWidget::Setup(UPSInventoryWidget* InInventory, int32 InIndex, bool bInDrawBackground)
{
	Inventory = InInventory;
	Index = InIndex;
	bDrawBackground = bInDrawBackground;
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
	if (bDrawBackground) Rect(0, 0, 64, 64, Hex(TEXT("1C1713")));
	const FSlateBrush& Brush = Selected ? Inventory->SelectedSlotBrush : IsHovered() && Unlocked ? Inventory->HoverSlotBrush : Inventory->SlotBrush;
	if (bDrawBackground)
	{
		FSlateDrawElement::MakeBox(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2D(60, 60) * Scale, FSlateLayoutTransform(FVector2D(2, 2) * Scale)),
			&Brush, ESlateDrawEffect::None, Unlocked ? Brush.GetTint(Style) : Hex(TEXT("494035")));
		if (!Brush.GetResourceObject() || !Unlocked)
		{
			Rect(5, 5, 54, 54, Unlocked ? Hex(IsHovered() ? TEXT("C79559") : TEXT("B88951")) : Hex(TEXT("363129")));
			Rect(5, 5, 54, 3, Unlocked ? Hex(TEXT("8C613B")) : Hex(TEXT("2C2822")));
		}
	}
	else if (!Unlocked) Rect(2, 2, 60, 60, FLinearColor(0.12f, 0.09f, 0.06f, 0.45f));
	else if (Selected || IsHovered())
	{
		const FLinearColor Highlight(1, 0.8f, 0.3f, 0.8f);
		Rect(2, 2, 60, 2, Highlight);
		Rect(2, 60, 60, 2, Highlight);
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
	const FPSItemDefinition& Definition = PSItems::GetDefinition(*Item);
	UTexture2D* IconTexture = Inventory->GetItemIcon(*Item);
	if (IconTexture)
	{
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(IconTexture);
		FSlateDrawElement::MakeBox(Elements, ++Layer, Geometry.ToPaintGeometry(FVector2D(40, 40) * Scale, FSlateLayoutTransform(FVector2D(12, 10) * Scale)), &IconBrush);
	}
	else
	{
		const FLinearColor C = Definition.Color;
		switch (Definition.PlaceholderIcon)
		{
		case 9: Rect(29, 18, 6, 36, C); Rect(16, 16, 27, 14, C); break;
		case 8: Rect(18, 27, 25, 20, C); Rect(23, 19, 16, 5, C); Rect(39, 28, 12, 5, C); Rect(49, 32, 4, 8, C); break;
		case 0: Rect(29, 16, 5, 34, Hex(TEXT("735037"))); Rect(17, 14, 29, 8, C); Rect(16, 20, 9, 8, C); break;
		case 1: Rect(20, 24, 24, 23, Hex(TEXT("E5CE95"))); Rect(24, 20, 16, 5, Hex(TEXT("765A36"))); Rect(30, 28, 4, 13, C); Rect(23, 27, 8, 5, C); Rect(33, 24, 7, 6, C); break;
		case 2: Rect(24, 25, 19, 17, C); Rect(28, 42, 11, 6, C); Rect(29, 17, 4, 10, Hex(TEXT("63884D"))); Rect(34, 16, 8, 5, Hex(TEXT("86A95E"))); break;
		case 3: Rect(18, 23, 28, 9, C); Rect(21, 34, 27, 10, C); Rect(21, 26, 21, 2, Hex(TEXT("775136"))); Rect(24, 38, 20, 2, Hex(TEXT("775136"))); break;
		case 4: Rect(21, 23, 22, 22, C); Rect(17, 30, 30, 11, C); Rect(25, 20, 13, 4, C); Rect(24, 26, 12, 4, Hex(TEXT("D3D6D8"))); break;
		case 6: Rect(29, 18, 5, 34, Hex(TEXT("735037"))); Rect(15, 15, 34, 7, C); Rect(12, 18, 10, 5, C); break;
		case 7: Rect(25, 12, 14, 4, C); Rect(25, 16, 4, 8, C); Rect(35, 16, 4, 8, C); Rect(30, 22, 4, 20, Hex(TEXT("735037"))); Rect(23, 40, 18, 12, C); Rect(27, 52, 10, 4, C); break;
		default: Rect(19, 25, 22, 15, C); Rect(23, 21, 13, 23, C); Rect(41, 21, 7, 23, C); Rect(21, 28, 3, 3, Hex(TEXT("243C3A"))); break;
		}
	}
	if (Definition.bInfiniteDurability)
	{
		Rect(8, 53, 48, 4, Hex(TEXT("B99BFF")));
	}
	else if (Definition.MaxDurability > 0)
	{
		Rect(8, 53, 48, 4, Hex(TEXT("33251C")));
		const float Ratio = FMath::Clamp(static_cast<float>(Item->CurrentDurability) / Definition.MaxDurability, 0.0f, 1.0f);
		Rect(8, 53, 48 * Ratio, 4, Ratio > 0.3f ? Hex(TEXT("78C679")) : Hex(TEXT("E05A47")));
	}
	if (Definition.MaxStack > 1 || Item->Quantity > 1)
	{
		const FString Count = FString::FromInt(Item->Quantity);
		const float X = 57.0f - Count.Len() * 8.0f;
		Rect(X - 3, 44, Count.Len() * 8 + 5, 16, FLinearColor(0.045f, 0.032f, 0.02f, 0.85f));
		FSlateDrawElement::MakeText(Elements, ++Layer,
			Geometry.ToPaintGeometry(FVector2D(50, 18) * Scale, FSlateLayoutTransform(FVector2D(X, 44) * Scale)),
			Count, MakeProjectFont(11), ESlateDrawEffect::None, Hex(TEXT("FFF0D1")));
	}
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
		SetProjectToolTipText(Item ? Inventory->GetItemDisplayName(*Item) : FText::FromString(Inventory->IsUnlocked(Index) ? TEXT("빈 슬롯") : TEXT("가방 확장이 필요합니다")));
	}
}

void UPSInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{
	Super::NativeOnMouseLeave(Event);
}
