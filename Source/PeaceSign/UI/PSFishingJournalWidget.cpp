#include "PSFishingJournalWidget.h"
#include "PSInventoryWidget.h"
#include "../PSPlayerController.h"
#include "../Fishing/PSFishingJournalComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/SWidget.h"
#include "Layout/ArrangedChildren.h"
#include "Framework/Application/SlateApplication.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace
{
	constexpr int32 EntriesPerPage = 16;
	constexpr float ActiveCategoryExtension = 64.f;
	const TCHAR* Categories[] = {TEXT("Fish"), TEXT("Gathering"), TEXT("Food"), TEXT("Artifact")};
	const TCHAR* CategoryNames[] = {TEXT("물고기 도감"), TEXT("채집 도감"), TEXT("음식 도감"), TEXT("유물 도감")};
	UTexture2D* Texture(const FString& Path) { return LoadObject<UTexture2D>(nullptr, *Path); }
	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Child, FVector2D Position, FVector2D Size, int32 Z = 0)
	{
		auto* Slot = Canvas->AddChildToCanvas(Child); Slot->SetPosition(Position); Slot->SetSize(Size); Slot->SetZOrder(Z); return Slot;
	}
	UImage* Picture(UWidgetTree* Tree, UCanvasPanel* Canvas, FName Name, const FString& Path, FVector2D Position, FVector2D Size, int32 Z = 0)
	{
		auto* Result = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Result->SetBrushFromTexture(Texture(Path)); Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Canvas, Result, Position, Size, Z); return Result;
	}
	void CenterText(UWidgetTree* Tree, UCanvasPanel* Canvas, UTextBlock* Label, FVector2D Position, FVector2D Size, bool Center = true)
	{
		// Align the measured text block inside its actual artwork region. Long/multiline text
		// can shrink to fit, while short values keep the authored font size and center vertically.
		auto* Frame = Cast<UScaleBox>(Label->GetParent());
		if (!Frame || Frame->GetParent() != Canvas)
		{
			Label->RemoveFromParent();
			Frame = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), *FString::Printf(TEXT("%sFrame"), *Label->GetName()));
			Frame->SetContent(Label);
			Place(Canvas, Frame, Position, Size, 5);
		}
		else if (auto* Slot = Cast<UCanvasPanelSlot>(Frame->Slot)) { Slot->SetPosition(Position); Slot->SetSize(Size); }
		Frame->SetStretch(EStretch::ScaleToFit); Frame->SetStretchDirection(EStretchDirection::DownOnly);
		Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* Content = CastChecked<UScaleBoxSlot>(Label->Slot);
		Content->SetHorizontalAlignment(HAlign_Center); Content->SetVerticalAlignment(VAlign_Center);
		Label->SetRenderTranslation(FVector2D::ZeroVector);
		Label->SetJustification(Center ? ETextJustify::Center : ETextJustify::Left);
		Label->SetAutoWrapText(false); Label->SetWrapTextAt(Size.X);
	}
	UTextBlock* Text(UWidgetTree* Tree, UCanvasPanel* Canvas, FName Name, const FString& Value, int32 FontSize, FVector2D Position, FVector2D Size, bool Center = true)
	{
		auto* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(FText::FromString(Value)); Result->SetFont(UPSUIWidget::MakeProjectFont(FontSize));
		Result->SetColorAndOpacity(FSlateColor(FLinearColor(.24f, .12f, .035f)));
		Result->SetJustification(Center ? ETextJustify::Center : ETextJustify::Left);
		Result->SetAutoWrapText(false); Result->SetWrapTextAt(Size.X); Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		CenterText(Tree, Canvas, Result, Position, Size, Center); return Result;
	}
	UButton* Button(UWidgetTree* Tree, UCanvasPanel* Canvas, FName Name, FVector2D Position, FVector2D Size, const FString& Path = FString(), int32 Z = 6)
	{
		auto* Result = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style; Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Hovered = Style.Normal; Style.Pressed = Style.Normal; Style.Disabled = Style.Normal;
		Style.NormalPadding = FMargin(0); Style.PressedPadding = FMargin(0);
		Result->SetStyle(Style);
		if (!Path.IsEmpty())
		{
			auto* Art = Tree->ConstructWidget<UImage>(); Art->SetBrushFromTexture(Texture(Path), true);
			Art->SetVisibility(ESlateVisibility::HitTestInvisible); Result->SetContent(Art);
			auto* ContentSlot = CastChecked<UButtonSlot>(Art->Slot);
			ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill); ContentSlot->SetPadding(FMargin(0));
		}
		Place(Canvas, Result, Position, Size, Z); return Result;
	}
	void ExtendTopTab(UWidgetTree* Tree, UButton* Tab, FVector2D FaceSize = FVector2D::ZeroVector)
	{
		auto* Slot = Tab ? Cast<UCanvasPanelSlot>(Tab->Slot) : nullptr;
		if (!Slot) return;
		if (FaceSize.IsNearlyZero()) FaceSize = FVector2D(Slot->GetSize().X, Slot->GetSize().X);
		auto* Art = Cast<UCanvasPanel>(Tab->GetContent());
		auto* Face = Art && Art->GetChildrenCount() ? Cast<UImage>(Art->GetChildAt(0)) : Cast<UImage>(Tab->GetContent());
		if (!Face) return;
		if (!Art)
		{
			Face->RemoveFromParent();
			Art = Tree->ConstructWidget<UCanvasPanel>(); Tab->SetContent(Art);
			auto* Content = CastChecked<UButtonSlot>(Art->Slot);
			Content->SetHorizontalAlignment(HAlign_Fill); Content->SetVerticalAlignment(VAlign_Fill); Content->SetPadding(FMargin(0));
			Place(Art, Face, FVector2D::ZeroVector, FaceSize);
			Place(Art, Tree->ConstructWidget<UImage>(), FVector2D::ZeroVector, FaceSize);
		}
		auto* FaceSlot = CastChecked<UCanvasPanelSlot>(Face->Slot); FaceSlot->SetSize(FaceSize);
		auto* Tail = CastChecked<UImage>(Art->GetChildAt(1)); FSlateBrush Brush = Face->GetBrush();
		Brush.SetUVRegion(FBox2f(FVector2f(0, .8f), FVector2f(1, 1)));
		Tail->SetBrush(Brush); Tail->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* TailSlot = CastChecked<UCanvasPanelSlot>(Tail->Slot);
		TailSlot->SetPosition(FVector2D(0, FaceSize.Y * .76)); TailSlot->SetSize(FVector2D(FaceSize.X, FaceSize.Y * .64));
		Slot->SetSize(FVector2D(FaceSize.X, FaceSize.Y * 1.4));
	}
	void Bind(UWidgetTree* Tree, FName Name, UObject* Owner, FName Function)
	{
		if (auto* Control = Cast<UButton>(Tree->FindWidget(Name)))
		{
			FScriptDelegate Callback; Callback.BindUFunction(Owner, Function); Control->OnClicked.AddUnique(Callback);
		}
	}
	void SetText(UWidgetTree* Tree, FName Name, const FString& Value)
	{
		if (auto* Label = Cast<UTextBlock>(Tree->FindWidget(Name))) Label->SetText(FText::FromString(Value));
	}
	bool FitNotebook(UWidgetTree* Tree, FVector2D Position, FVector2D Size, FVector2D Reference)
	{
		auto* Book = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("JournalBook")));
		UWidget* Cover = Tree->FindWidget(TEXT("BookBackground"));
		UWidget* Binder = Tree->FindWidget(TEXT("BookBinder"));
		auto* CoverSlot = Cover ? Cast<UCanvasPanelSlot>(Cover->Slot) : nullptr;
		auto* BinderSlot = Binder ? Cast<UCanvasPanelSlot>(Binder->Slot) : nullptr;
		auto* ReferenceBox = Book ? Cast<USizeBox>(Book->GetParent()) : nullptr;
		auto* Fit = ReferenceBox ? Cast<UScaleBox>(ReferenceBox->GetParent()) : nullptr;
		auto* FitSlot = Fit ? Cast<UCanvasPanelSlot>(Fit->Slot) : nullptr;
		if (!Book || !CoverSlot || !BinderSlot || !ReferenceBox || !FitSlot) return false;
		const FVector2D OldPosition(FMath::Min(CoverSlot->GetPosition().X, BinderSlot->GetPosition().X), FMath::Min(CoverSlot->GetPosition().Y, BinderSlot->GetPosition().Y));
		const FVector2D OldEnd(FMath::Max((CoverSlot->GetPosition() + CoverSlot->GetSize()).X, (BinderSlot->GetPosition() + BinderSlot->GetSize()).X),
			FMath::Max((CoverSlot->GetPosition() + CoverSlot->GetSize()).Y, (BinderSlot->GetPosition() + BinderSlot->GetSize()).Y));
		const FVector2D OldSize = OldEnd - OldPosition;
		if (OldSize.X <= 0 || OldSize.Y <= 0 || Size.X <= 0 || Size.Y <= 0) return false;
		const FVector2D Scale = Size / OldSize;
		for (UWidget* Child : Book->GetAllChildren())
		{
			auto* Slot = Cast<UCanvasPanelSlot>(Child->Slot);
			if (!Slot) continue;
			Child->Modify(); Slot->Modify();
			FVector2D NewPosition = Position + (Slot->GetPosition() - OldPosition) * Scale;
			FVector2D NewSize = Slot->GetSize() * Scale;
			if (Child->GetFName() == TEXT("FishStar") || Child->GetFName() == TEXT("FishLock"))
			{
				const FVector2D IconSize = Slot->GetSize() * FMath::Min(Scale.X, Scale.Y);
				NewPosition += (NewSize - IconSize) * .5; NewSize = IconSize;
			}
			Slot->SetPosition(NewPosition); Slot->SetSize(NewSize);
		}
		if (auto* Grid = Cast<UUniformGridPanel>(Tree->FindWidget(TEXT("FishList"))))
		{
			Grid->Modify(); Grid->SetMinDesiredSlotWidth(Grid->GetMinDesiredSlotWidth() * Scale.X);
			Grid->SetMinDesiredSlotHeight(Grid->GetMinDesiredSlotHeight() * Scale.Y);
		}
		ReferenceBox->Modify(); ReferenceBox->SetWidthOverride(Reference.X); ReferenceBox->SetHeightOverride(Reference.Y);
		FitSlot->Modify(); FitSlot->SetOffsets(FMargin(16)); Fit->SetStretchDirection(EStretchDirection::Both);
		return true;
	}
#if WITH_EDITOR
	bool FindLayoutGeometry(const TSharedRef<SWidget>& Widget, const FGeometry& Geometry, const TSharedRef<SWidget>& Target, FGeometry& Result)
	{
		if (Widget == Target) { Result = Geometry; return true; }
		FArrangedChildren Children(EVisibility::Visible); Widget->ArrangeChildren(Geometry, Children, true);
		for (int32 Index = 0; Index < Children.Num(); ++Index)
			if (FindLayoutGeometry(Children[Index].Widget, Children[Index].Geometry, Target, Result)) return true;
		return false;
	}
#endif
}

void UPSFishingJournalEntry::Setup(UPSFishingJournalWidget* Owner, FName Id)
{
	Journal = Owner; FishId = Id;
	OnClicked.AddUniqueDynamic(this, &ThisClass::SelectEntry);
}
void UPSFishingJournalEntry::SelectEntry() { if (Journal) Journal->SelectFish(FishId); }

UPSFishingJournalWidget::UPSFishingJournalWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
	// Hard references keep alternate-category artwork and both star states in cooked builds.
	for (const TCHAR* Kind : Categories)
	{
		const FString Path = FString::Printf(TEXT("/Game/UI/Encyclopedia/Types_Page/Type_Title/Type_Title_%s"), Kind);
		ConstructorHelpers::FObjectFinder<UTexture2D> Art(*Path); CategoryTitleTextures.Add(Art.Object);
	}
	for (const TCHAR* Color : {TEXT("Gray"), TEXT("Yellow")})
	{
		const FString Path = FString::Printf(TEXT("/Game/UI/Encyclopedia/Information_Page/Information_Star/Information_Star_%s"), Color);
		ConstructorHelpers::FObjectFinder<UTexture2D> Art(*Path); StarTextures.Add(Art.Object);
	}
	ConstructorHelpers::FObjectFinder<UTexture2D> EntryName(TEXT("/Game/UI/Encyclopedia/Types_Page/Types_NameTag")); EntryNameTexture = EntryName.Object;
}

void UPSFishingJournalWidget::BuildLayout(UWidgetTree* Tree)
{
	auto* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("JournalViewport")); Tree->RootWidget = Root;
	auto* Shade = Tree->ConstructWidget<UBorder>(); Shade->SetBrushColor(FLinearColor(0, 0, 0, .6f));
	Place(Root, Shade, FVector2D::ZeroVector, FVector2D::ZeroVector)->SetAnchors(FAnchors(0, 0, 1, 1));
	auto* Fit = Tree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit);
	auto* FitSlot = Place(Root, Fit, FVector2D::ZeroVector, FVector2D::ZeroVector, 1);
	FitSlot->SetAnchors(FAnchors(0, 0, 1, 1)); FitSlot->SetOffsets(FMargin(12));
	auto* Size = Tree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(1300); Size->SetHeightOverride(960); Fit->SetContent(Size);
	auto* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("JournalBook")); Size->SetContent(Canvas);
	// Side tabs lie above the cover and below the first page. The page masks their roots.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FString Name = FString::Printf(TEXT("Category%s"), Categories[Index]);
		const FString Path = FString::Printf(TEXT("/Game/UI/Book/Encyclopedia_Index/Encyclopedia_Index_%s"), Categories[Index]);
		auto* Tab = Button(Tree, Canvas, *Name, FVector2D(923, 160 + Index * 112), FVector2D(298, 86), Path, 2);
		Tab->SetToolTipText(FText::FromString(CategoryNames[Index]));
		if (Index == 0) Tab->SetRenderTranslation(FVector2D(ActiveCategoryExtension, 0));
	}
	Picture(Tree, Canvas, TEXT("BookBackground"), TEXT("/Game/UI/Book/Book_Background"), FVector2D(42, 78), FVector2D(1139, 867), 1)->SetVisibility(ESlateVisibility::Visible);
	Picture(Tree, Canvas, TEXT("BookFirstPage"), TEXT("/Game/UI/Book/Book_1stPage"), FVector2D(54, 82), FVector2D(1092, 832), 3)->SetVisibility(ESlateVisibility::Visible);
	Picture(Tree, Canvas, TEXT("BookBinder"), TEXT("/Game/UI/Book/Book_Binder"), FVector2D(3, 104), FVector2D(82, 775), 4);
	Picture(Tree, Canvas, TEXT("TypesBackground"), TEXT("/Game/UI/Encyclopedia/Types_Page/Type_Background"), FVector2D(94, 110), FVector2D(509, 772), 3);
	Picture(Tree, Canvas, TEXT("InformationBackground"), TEXT("/Game/UI/Encyclopedia/Information_Page/Information_Background"), FVector2D(605, 110), FVector2D(509, 772), 3);
	Picture(Tree, Canvas, TEXT("CategoryTitleArt"), TEXT("/Game/UI/Encyclopedia/Types_Page/Type_Title/Type_Title_Fish"), FVector2D(124, 131), FVector2D(454, 105), 4);
	auto* Title = Text(Tree, Canvas, TEXT("CategoryTitle"), TEXT("물고기 도감"), 32, FVector2D(132, 139), FVector2D(438, 89), true);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	auto* Grid = Tree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("FishList"));
	Grid->SetMinDesiredSlotWidth(101); Grid->SetMinDesiredSlotHeight(129);
	Place(Canvas, Grid, FVector2D(146, 253), FVector2D(404, 516), 5);
	Text(Tree, Canvas, TEXT("EmptyCategory"), TEXT("등록된 항목이 없습니다."), 21, FVector2D(160, 460), FVector2D(380, 80), true)->SetVisibility(ESlateVisibility::Collapsed);
	Picture(Tree, Canvas, TEXT("PageArrows"), TEXT("/Game/UI/Encyclopedia/Types_Page/Type_PageArrow"), FVector2D(250, 812), FVector2D(198, 52), 4);
	Button(Tree, Canvas, TEXT("PreviousPage"), FVector2D(250, 812), FVector2D(52, 52));
	Button(Tree, Canvas, TEXT("NextPage"), FVector2D(396, 812), FVector2D(52, 52));
	Text(Tree, Canvas, TEXT("PageNumber"), TEXT("1 / 1"), 30, FVector2D(302, 812), FVector2D(94, 52), true);
	Picture(Tree, Canvas, TEXT("NameArt"), TEXT("/Game/UI/Encyclopedia/Information_Page/Information_Name"), FVector2D(706, 134), FVector2D(319, 57), 4);
	Text(Tree, Canvas, TEXT("FishName"), TEXT("항목을 선택해 주세요"), 27, FVector2D(712, 138), FVector2D(307, 49), true);
	Picture(Tree, Canvas, TEXT("PictureArt"), TEXT("/Game/UI/Encyclopedia/Information_Page/Information_Picture"), FVector2D(675, 193), FVector2D(379, 299), 4);
	auto* ImageFit = Tree->ConstructWidget<UScaleBox>(); ImageFit->SetStretch(EStretch::ScaleToFit);
	Place(Canvas, ImageFit, FVector2D(697, 216), FVector2D(330, 250), 5);
	auto* Fish = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FishImage")); Fish->SetVisibility(ESlateVisibility::HitTestInvisible); ImageFit->SetContent(Fish);
	Picture(Tree, Canvas, TEXT("FishStar"), TEXT("/Game/UI/Encyclopedia/Information_Page/Information_Star/Information_Star_Gray"), FVector2D(1014, 180), FVector2D(70, 69), 6);
	Picture(Tree, Canvas, TEXT("SpecialArt"), TEXT("/Game/UI/Encyclopedia/Information_Page/Information_SpecialInfor/Information_SpecialInfor_Background"), FVector2D(672, 503), FVector2D(386, 52), 4);
	Picture(Tree, Canvas, TEXT("FishLock"), TEXT("/Game/UI/Encyclopedia/Information_Page/Information_SpecialInfor/Information_SpecialInfor_LockIcon"), FVector2D(850, 512), FVector2D(31, 34), 6);
	Text(Tree, Canvas, TEXT("FishSpecial"), TEXT(""), 14, FVector2D(682, 506), FVector2D(366, 46), true);
	const TCHAR* Labels[] = {TEXT("설명"), TEXT("사이즈"), TEXT("최대 기록"), TEXT("계절"), TEXT("위치")};
	const TCHAR* Values[] = {TEXT("FishDetails"), TEXT("FishSize"), TEXT("FishRecord"), TEXT("FishSeason"), TEXT("FishLocation")};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Text(Tree, Canvas, *FString::Printf(TEXT("DetailLabel%d"), Index), Labels[Index], 20, FVector2D(638, 557 + Index * 55.5), FVector2D(121, 55.5), true);
		Text(Tree, Canvas, Values[Index], TEXT(""), Index == 0 ? 16 : 21, FVector2D(778, 560 + Index * 55.5), FVector2D(298, 49.5), true);
	}
	// Even the raised tab overlaps the cover; drawing the cover above it hides the bottom edge.
	auto* Inventory = Button(Tree, Canvas, TEXT("InventoryTab"), FVector2D(85, 20), FVector2D(72, 72), TEXT("/Game/UI/Book/Book_Index/Book_Index_Inven"), 0);
	ExtendTopTab(Tree, Inventory);
	auto* Journal = Button(Tree, Canvas, TEXT("JournalTab"), FVector2D(177, 20), FVector2D(72, 72), TEXT("/Game/UI/Book/Book_Index/Book_Index_Encyclopedia"), 0);
	ExtendTopTab(Tree, Journal); Journal->SetRenderTranslation(FVector2D(0, -16));
	auto* Skill = Button(Tree, Canvas, TEXT("SkillTab"), FVector2D(269, 20), FVector2D(72, 72), TEXT("/Game/UI/Book/Book_Index/Book_Index_Skill"), 0);
	ExtendTopTab(Tree, Skill);
	Skill->SetToolTipText(FText::FromString(TEXT("스킬 (준비 중)")));
	// The inventory's authored notebook is 1200x800, centered in its 1920x1080 canvas.
	FitNotebook(Tree, FVector2D(360, 140), FVector2D(1200, 800), FVector2D(1920, 1080));
}

bool UPSFishingJournalWidget::ApplyBookDesignerLayout(UObject* Asset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(StaticClass())) return false;
	if (!Texture(TEXT("/Game/UI/Book/Book_Background"))) return false;
	Blueprint->Modify(); Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, NAME_None, RF_Transactional);
	BuildLayout(Blueprint->WidgetTree); Blueprint->WidgetVariableNameToGuidMap.Empty();
	Blueprint->WidgetTree->ForEachWidget([Blueprint](UWidget* Widget) { Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid()); });
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); return true;
#else
	return false;
#endif
}
bool UPSFishingJournalWidget::InitializeDesignerLayout(UObject* Asset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
	if (!Blueprint || !Blueprint->WidgetTree) return false;
	if (!Blueprint->WidgetTree->RootWidget) return ApplyBookDesignerLayout(Asset);
	return true; // Setup scripts and normal initialization preserve Designer edits.
#else
	return false;
#endif
}

bool UPSFishingJournalWidget::AdjustBookTextAlignment(UObject* Asset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(StaticClass()) || !Blueprint->WidgetTree) return false;
	Blueprint->Modify();
	TArray<UTextBlock*> Labels;
	Blueprint->WidgetTree->ForEachWidget([&Labels](UWidget* Widget) { if (auto* Label = Cast<UTextBlock>(Widget)) Labels.Add(Label); });
	for (UTextBlock* Label : Labels)
	{
		Label->Modify();
		UCanvasPanel* Canvas = Cast<UCanvasPanel>(Label->GetParent());
		UPanelWidget* Parent = Label->GetParent();
		if (!Canvas && Parent) Canvas = Cast<UCanvasPanel>(Parent->GetParent());
		if (!Canvas) continue;
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Canvas == Parent ? Label->Slot : Parent->Slot);
		if (!Slot) continue;
		FVector2D Position = Slot->GetPosition(), Size = Slot->GetSize();
		const auto ArtRegion = [&](const TCHAR* Name, FVector2D Inset)
		{
			if (UWidget* Art = Blueprint->WidgetTree->FindWidget(Name))
				if (auto* ArtSlot = Cast<UCanvasPanelSlot>(Art->Slot)) { Position = ArtSlot->GetPosition() + Inset; Size = ArtSlot->GetSize() - Inset * 2; }
		};
		const FName Name = Label->GetFName();
		if (Name == TEXT("CategoryTitle")) ArtRegion(TEXT("CategoryTitleArt"), FVector2D(8, 8));
		else if (Name == TEXT("FishName")) ArtRegion(TEXT("NameArt"), FVector2D(6, 4));
		else if (Name == TEXT("FishSpecial")) ArtRegion(TEXT("SpecialArt"), FVector2D(10, 3));
		else if (Name == TEXT("PageNumber")) ArtRegion(TEXT("PageArrows"), FVector2D(52, 0));
		const TCHAR* Values[] = {TEXT("FishDetails"), TEXT("FishSize"), TEXT("FishRecord"), TEXT("FishSeason"), TEXT("FishLocation")};
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const bool Heading = Name == *FString::Printf(TEXT("DetailLabel%d"), Index);
			if (!Heading && Name != Values[Index]) continue;
			if (UWidget* Art = Blueprint->WidgetTree->FindWidget(TEXT("InformationBackground")))
				if (auto* ArtSlot = Cast<UCanvasPanelSlot>(Art->Slot))
				{
					const FVector2D Scale = ArtSlot->GetSize() / FVector2D(509, 772);
					Position = ArtSlot->GetPosition() + FVector2D(Heading ? 33 : 173, (Heading ? 447 : 450) + Index * 55.5) * Scale;
					Size = FVector2D(Heading ? 121 : 298, Heading ? 55.5 : 49.5) * Scale;
				}
		}
		CenterText(Blueprint->WidgetTree, Canvas, Label, Position, Size);
	}
	Blueprint->WidgetTree->ForEachWidget([Blueprint](UWidget* Widget)
	{
		if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())) Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
	});
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); return true;
#else
	return false;
#endif
}

bool UPSFishingJournalWidget::AdjustBookTabs(UObject* Asset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
	if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(StaticClass())) return false;
	UWidget* Cover = Blueprint->WidgetTree->FindWidget(TEXT("BookBackground"));
	UWidget* Paper = Blueprint->WidgetTree->FindWidget(TEXT("BookFirstPage"));
	auto* PaperSlot = Paper ? Cast<UCanvasPanelSlot>(Paper->Slot) : nullptr;
	if (!Cover || !PaperSlot) return false;
	Blueprint->Modify(); Cover->Modify(); Paper->Modify(); PaperSlot->Modify();
	Cover->SetVisibility(ESlateVisibility::Visible); Paper->SetVisibility(ESlateVisibility::Visible);
	if (auto* CoverSlot = Cast<UCanvasPanelSlot>(Cover->Slot)) { CoverSlot->Modify(); CoverSlot->SetZOrder(1); }
	PaperSlot->SetZOrder(3);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UWidget* Tab = Blueprint->WidgetTree->FindWidget(*FString::Printf(TEXT("Category%s"), Categories[Index]));
		auto* Slot = Tab ? Cast<UCanvasPanelSlot>(Tab->Slot) : nullptr;
		if (!Slot) return false;
		Tab->Modify(); Slot->Modify();
		FVector2D Position = Slot->GetPosition();
		Position.X = PaperSlot->GetPosition().X + PaperSlot->GetSize().X - Slot->GetSize().X + 75;
		Slot->SetPosition(Position); Slot->SetZOrder(2);
		Tab->SetRenderTranslation(FVector2D(Index == 0 ? ActiveCategoryExtension : 0, 0));
	}
	for (const TCHAR* Name : {TEXT("InventoryTab"), TEXT("JournalTab"), TEXT("SkillTab")})
	{
		UWidget* Tab = Blueprint->WidgetTree->FindWidget(Name);
		auto* Slot = Tab ? Cast<UCanvasPanelSlot>(Tab->Slot) : nullptr;
		if (!Slot) return false;
		Tab->Modify(); Slot->Modify();
		FVector2D Position = Slot->GetPosition(); Position.Y = 20;
		Slot->SetPosition(Position); Slot->SetZOrder(0);
		Tab->SetRenderTranslation(FVector2D(0, Tab->GetFName() == TEXT("JournalTab") ? -16 : 0));
		ExtendTopTab(Blueprint->WidgetTree, CastChecked<UButton>(Tab));
	}
	Blueprint->WidgetTree->ForEachWidget([Blueprint](UWidget* Widget)
	{
		if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())) Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
	});
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); return true;
#else
	return false;
#endif
}

bool UPSFishingJournalWidget::MatchInventoryNotebook(UObject* JournalAsset, UObject* InventoryAsset)
{
#if WITH_EDITOR
	auto* Journal = Cast<UWidgetBlueprint>(JournalAsset);
	auto* Inventory = Cast<UWidgetBlueprint>(InventoryAsset);
	if (!Journal || !Inventory || !Journal->WidgetTree || !Inventory->WidgetTree || !Inventory->GeneratedClass) return false;
	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogTemp, Error, TEXT("Matching inventory bookmark geometry requires editor mode (-ExecutePythonScript), not a commandlet."));
		return false;
	}
	const auto* Defaults = Cast<UPSInventoryWidget>(Inventory->GeneratedClass->GetDefaultObject());
	if (!Defaults) return false;
	UImage* Background = nullptr;
	Inventory->WidgetTree->ForEachWidget([&Background](UWidget* Widget)
	{
		if (auto* Image = Cast<UImage>(Widget))
			if (const UObject* Resource = Image->GetBrush().GetResourceObject(); Resource && Resource->GetFName() == TEXT("Inven_Background")) Background = Image;
	});
	auto* Slot = Background ? Cast<UCanvasPanelSlot>(Background->Slot) : nullptr;
	if (!Slot) return false;
	const FVector2D Reference = Defaults->DesignerReferenceSize;
	const FAnchors Anchors = Slot->GetAnchors(); const FMargin Offsets = Slot->GetOffsets();
	FVector2D Size(Offsets.Right, Offsets.Bottom);
	if (Anchors.Minimum.X != Anchors.Maximum.X) Size.X = Reference.X * (Anchors.Maximum.X - Anchors.Minimum.X) - Offsets.Left - Offsets.Right;
	if (Anchors.Minimum.Y != Anchors.Maximum.Y) Size.Y = Reference.Y * (Anchors.Maximum.Y - Anchors.Minimum.Y) - Offsets.Top - Offsets.Bottom;
	const FVector2D Position = Reference * Anchors.Minimum + FVector2D(Offsets.Left, Offsets.Top) - Size * Slot->GetAlignment();
	Journal->Modify();
	if (!FitNotebook(Journal->WidgetTree, Position, Size, Reference)) return false;
	// Ask Slate to arrange the authored inventory tree, so HorizontalBox padding,
	// anchors and any nested ScaleBoxes are included in the shared bookmark positions.
	const TSharedRef<SWidget> InventorySlate = Inventory->WidgetTree->RootWidget->TakeWidget();
	InventorySlate->SlatePrepass(1.f);
	const FGeometry RootGeometry = FGeometry::MakeRoot(Reference, FSlateLayoutTransform(1.f));
	const TCHAR* SourceNames[] = {TEXT("Button_Information"), TEXT("Button_Encyclopedia"), TEXT("Button_Setting")};
	const TCHAR* TargetNames[] = {TEXT("InventoryTab"), TEXT("JournalTab"), TEXT("SkillTab")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UWidget* Source = Inventory->WidgetTree->FindWidget(SourceNames[Index]);
		auto* Target = Cast<UButton>(Journal->WidgetTree->FindWidget(TargetNames[Index]));
		if (!Source || !Target) return false;
		FGeometry Geometry;
		if (!FindLayoutGeometry(InventorySlate, RootGeometry, Source->TakeWidget(), Geometry)) return false;
		const FVector2D FaceSize = Geometry.GetLocalSize();
		if (FaceSize.X <= 0 || FaceSize.Y <= 0) return false;
		auto* TargetSlot = CastChecked<UCanvasPanelSlot>(Target->Slot); TargetSlot->Modify(); Target->Modify();
		TargetSlot->SetPosition(RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
		ExtendTopTab(Journal->WidgetTree, Target, FaceSize);
		Target->SetRenderTranslation(FVector2D(0, Index == 1 ? -24 : 0));
		UE_LOG(LogTemp, Display, TEXT("NOTEBOOK_TAB_MATCH %s position=%s size=%s"), TargetNames[Index], *TargetSlot->GetPosition().ToString(), *FaceSize.ToString());
	}
	UE_LOG(LogTemp, Display, TEXT("NOTEBOOK_MATCH reference=%s position=%s size=%s background=%s"), *Reference.ToString(), *Position.ToString(), *Size.ToString(), *Background->GetName());
	FBlueprintEditorUtils::MarkBlueprintAsModified(Journal); return true;
#else
	return false;
#endif
}

void UPSFishingJournalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget) BuildLayout(WidgetTree);
	FishList = Cast<UUniformGridPanel>(WidgetTree->FindWidget(TEXT("FishList")));
	FishName = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("FishName")));
	FishStar = Cast<UImage>(WidgetTree->FindWidget(TEXT("FishStar")));
	FishDetails = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("FishDetails")));
	FishImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("FishImage")));
	FishSpecial = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("FishSpecial")));
	FishLock = Cast<UImage>(WidgetTree->FindWidget(TEXT("FishLock")));
	Bind(WidgetTree, TEXT("PreviousPage"), this, GET_FUNCTION_NAME_CHECKED(ThisClass, PreviousPage));
	Bind(WidgetTree, TEXT("NextPage"), this, GET_FUNCTION_NAME_CHECKED(ThisClass, NextPage));
	Bind(WidgetTree, TEXT("InventoryTab"), this, GET_FUNCTION_NAME_CHECKED(ThisClass, InventoryTab));
	Bind(WidgetTree, TEXT("CategoryFish"), this, GET_FUNCTION_NAME_CHECKED(ThisClass, FishTab));
	Bind(WidgetTree, TEXT("CategoryGathering"), this, GET_FUNCTION_NAME_CHECKED(ThisClass, GatheringTab));
	Bind(WidgetTree, TEXT("CategoryFood"), this, GET_FUNCTION_NAME_CHECKED(ThisClass, FoodTab));
	Bind(WidgetTree, TEXT("CategoryArtifact"), this, GET_FUNCTION_NAME_CHECKED(ThisClass, ArtifactTab));
}
void UPSFishingJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (const auto* Controller = Cast<APSPlayerController>(GetOwningPlayer()))
		if (auto* Journal = Controller->GetFishingJournalComponent()) Journal->OnJournalChanged.AddUniqueDynamic(this, &ThisClass::OnJournalChanged);
	RefreshJournal();
}
void UPSFishingJournalWidget::NativeDestruct()
{
	if (const auto* Controller = Cast<APSPlayerController>(GetOwningPlayer()))
		if (auto* Journal = Controller->GetFishingJournalComponent()) Journal->OnJournalChanged.RemoveDynamic(this, &ThisClass::OnJournalChanged);
	Super::NativeDestruct();
}
void UPSFishingJournalWidget::OnJournalChanged(FName) { RefreshJournal(); }
void UPSFishingJournalWidget::FishTab() { SelectCategory(0); }
void UPSFishingJournalWidget::GatheringTab() { SelectCategory(1); }
void UPSFishingJournalWidget::FoodTab() { SelectCategory(2); }
void UPSFishingJournalWidget::ArtifactTab() { SelectCategory(3); }
void UPSFishingJournalWidget::SelectCategory(int32 Index)
{
	if (Index < 0 || Index > 3 || Category == Index) return;
	Category = Index; Page = 0; SelectedFish = NAME_None; RefreshJournal();
}
void UPSFishingJournalWidget::PreviousPage() { if (Page > 0) { --Page; SelectedFish = NAME_None; RefreshJournal(); } }
void UPSFishingJournalWidget::NextPage()
{
	const auto* Controller = Cast<APSPlayerController>(GetOwningPlayer());
	if (Category == 0 && Controller && (Page + 1) * EntriesPerPage < Controller->GetFishIds().Num()) { ++Page; SelectedFish = NAME_None; RefreshJournal(); }
}
void UPSFishingJournalWidget::UpdateCategoryTabs()
{
	for (int32 Index = 0; Index < 4; ++Index)
		if (UWidget* Tab = WidgetTree->FindWidget(*FString::Printf(TEXT("Category%s"), Categories[Index]))) Tab->SetRenderTranslation(FVector2D(Index == Category ? ActiveCategoryExtension : 0, 0));
	if (auto* Title = Cast<UImage>(WidgetTree->FindWidget(TEXT("CategoryTitleArt"))); Title && CategoryTitleTextures.IsValidIndex(Category)) Title->SetBrushFromTexture(CategoryTitleTextures[Category]);
	SetText(WidgetTree, TEXT("CategoryTitle"), CategoryNames[Category]);
}
void UPSFishingJournalWidget::RefreshJournal()
{
	const auto* Controller = Cast<APSPlayerController>(GetOwningPlayer());
	if (!Controller || !FishList) return;
	UpdateCategoryTabs();
	const auto* Journal = Controller->GetFishingJournalComponent();
	const TArray<FName> Ids = Category == 0 ? Controller->GetFishIds() : TArray<FName>();
	const int32 PageCount = FMath::Max(1, FMath::DivideAndRoundUp(Ids.Num(), EntriesPerPage)); Page = FMath::Clamp(Page, 0, PageCount - 1);
	SetText(WidgetTree, TEXT("PageNumber"), FString::Printf(TEXT("%d / %d"), Page + 1, PageCount));
	if (UWidget* Empty = WidgetTree->FindWidget(TEXT("EmptyCategory"))) Empty->SetVisibility(Ids.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UWidget* Previous = WidgetTree->FindWidget(TEXT("PreviousPage"))) Previous->SetIsEnabled(Page > 0);
	if (UWidget* Next = WidgetTree->FindWidget(TEXT("NextPage"))) Next->SetIsEnabled(Page + 1 < PageCount);
	if (Ids.IsEmpty()) SelectedFish = NAME_None;
	else if (!Ids.Contains(SelectedFish) || Ids.IndexOfByKey(SelectedFish) / EntriesPerPage != Page) SelectedFish = Ids[Page * EntriesPerPage];
	FishList->ClearChildren();
	for (int32 Index = Page * EntriesPerPage; !Ids.IsEmpty() && Index < (Page + 1) * EntriesPerPage; ++Index)
	{
		// Empty cells preserve all four row/column tracks on a partly filled last page.
		if (!Ids.IsValidIndex(Index))
		{
			FishList->AddChildToUniformGrid(WidgetTree->ConstructWidget<USizeBox>(), (Index % EntriesPerPage) / 4, Index % 4);
			continue;
		}
		FName Id = Ids[Index]; const FPSFishDefinition* Definition = Controller->FindFishDefinition(Id);
		const bool Known = Journal && Journal->HasDiscovered(Id);
		auto* Entry = WidgetTree->ConstructWidget<UPSFishingJournalEntry>(); Entry->Setup(this, Id);
		FButtonStyle Style; Style.Normal = *FCoreStyle::Get().GetBrush("WhiteBrush");
		Style.Normal.TintColor = FSlateColor(FLinearColor(.55f, .32f, .1f, Id == SelectedFish ? .22f : 0));
		Style.Hovered = Style.Normal; Style.Hovered.TintColor = FSlateColor(FLinearColor(.55f, .32f, .1f, .15f));
		Style.Pressed = Style.Hovered; Style.NormalPadding = FMargin(0); Style.PressedPadding = FMargin(0); Entry->SetStyle(Style);
		auto* CellFit = WidgetTree->ConstructWidget<UScaleBox>(); CellFit->SetStretch(EStretch::ScaleToFit); Entry->SetContent(CellFit);
		auto* CellSize = WidgetTree->ConstructWidget<USizeBox>(); CellSize->SetWidthOverride(101); CellSize->SetHeightOverride(129); CellFit->SetContent(CellSize);
		auto* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(); CellSize->SetContent(Cell);
		auto* ContentSlot = CastChecked<UButtonSlot>(CellFit->Slot);
		ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill); ContentSlot->SetPadding(FMargin(0));
		Picture(WidgetTree, Cell, NAME_None, TEXT("/Game/UI/Encyclopedia/Types_Page/Types_NameTag"), FVector2D(6, 6), FVector2D(89, 30));
		Text(WidgetTree, Cell, NAME_None, FString::Printf(TEXT("%02d %s"), Index + 1, Known ? *Definition->Name.ToString() : TEXT("???")), 13, FVector2D(9, 8), FVector2D(83, 24), true);
		if (UTexture2D* Icon = Known ? Controller->GetFishIcon(Id) : nullptr)
		{
			auto* Fit = WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit);
			Place(Cell, Fit, FVector2D(9, 43), FVector2D(83, 70));
			auto* Art = WidgetTree->ConstructWidget<UImage>(); Art->SetBrushFromTexture(Icon, true); Art->SetVisibility(ESlateVisibility::HitTestInvisible); Fit->SetContent(Art);
		}
		else Text(WidgetTree, Cell, NAME_None, TEXT("?"), 36, FVector2D(10, 48), FVector2D(81, 65), true);
		FishList->AddChildToUniformGrid(Entry, (Index % EntriesPerPage) / 4, Index % 4);
	}
	if (SelectedFish.IsNone()) ClearDetails(); else SelectFish(SelectedFish);
}
void UPSFishingJournalWidget::ClearDetails()
{
	if (FishName) FishName->SetText(FText::FromString(TEXT("항목을 선택해 주세요")));
	if (FishImage) FishImage->SetVisibility(ESlateVisibility::Collapsed);
	if (FishStar) FishStar->SetVisibility(ESlateVisibility::Collapsed);
	if (FishLock) FishLock->SetVisibility(ESlateVisibility::Collapsed);
	for (const TCHAR* Name : {TEXT("FishDetails"), TEXT("FishSize"), TEXT("FishRecord"), TEXT("FishSeason"), TEXT("FishLocation"), TEXT("FishSpecial")}) SetText(WidgetTree, Name, TEXT(""));
}
void UPSFishingJournalWidget::SelectFish(FName Id)
{
	const auto* Controller = Cast<APSPlayerController>(GetOwningPlayer());
	if (!Controller || Category != 0) return;
	const bool Changed = SelectedFish != Id; SelectedFish = Id;
	const auto* Journal = Controller->GetFishingJournalComponent();
	const FPSFishDefinition* Fish = Controller->FindFishDefinition(Id);
	const bool Known = Fish && Journal && Journal->HasDiscovered(Id);
	const bool Mastered = Known && Controller->HasMasteredFish(Id);
	const int32 Number = Controller->GetFishIds().IndexOfByKey(Id) + 1;
	if (FishName) FishName->SetText(FText::FromString(FString::Printf(TEXT("%02d %s"), Number, Known ? *Fish->Name.ToString() : TEXT("???"))));
	if (FishStar)
	{
		FishStar->SetVisibility(Known ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (StarTextures.IsValidIndex(Mastered ? 1 : 0)) FishStar->SetBrushFromTexture(StarTextures[Mastered ? 1 : 0]);
	}
	UTexture2D* Icon = Known ? Controller->GetFishIcon(Id) : nullptr;
	if (FishImage) { FishImage->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); if (Icon) FishImage->SetBrushFromTexture(Icon, true); }
	if (FishLock) FishLock->SetVisibility(Mastered ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (FishSpecial) FishSpecial->SetText(Mastered ? Fish->DescriptionPlus : FText::GetEmpty());
	SetText(WidgetTree, TEXT("FishDetails"), Known ? Fish->Description.ToString() : TEXT("낚시에 성공하면 정보가 등록돼요."));
	SetText(WidgetTree, TEXT("FishSize"), Known ? FString::Printf(TEXT("%d ~ %d cm"), Fish->MinSize, Fish->MaxSize) : TEXT("???"));
	SetText(WidgetTree, TEXT("FishRecord"), Known ? FString::Printf(TEXT("%d cm"), Journal->GetRecord(Id).LargestSizeCm) : TEXT("???"));
	const TCHAR* Seasons[] = {TEXT("봄"), TEXT("여름"), TEXT("가을"), TEXT("겨울")};
	SetText(WidgetTree, TEXT("FishSeason"), Known ? Seasons[FMath::Clamp(Fish->Season, 0, 3)] : TEXT("???"));
	TArray<FString> Places; const TCHAR* Locations[] = {TEXT("바다"), TEXT("강"), TEXT("호수")};
	if (Known) for (int32 Location : Fish->Location) if (Location >= 0 && Location < 3) Places.AddUnique(Locations[Location]);
	SetText(WidgetTree, TEXT("FishLocation"), Known ? FString::Join(Places, TEXT(", ")) : TEXT("???"));
	if (Changed && FishList && Controller->GetFishIds().Contains(Id))
	{
		Page = Controller->GetFishIds().IndexOfByKey(Id) / EntriesPerPage;
		RefreshJournal();
	}
}
void UPSFishingJournalWidget::InventoryTab()
{
	if (auto* Controller = Cast<APSPlayerController>(GetOwningPlayer())) Controller->ShowInventoryTab();
}
void UPSFishingJournalWidget::CloseJournal()
{
	if (auto* Controller = Cast<APSPlayerController>(GetOwningPlayer())) Controller->CloseInventory();
}
FReply UPSFishingJournalWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::I) { CloseJournal(); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
