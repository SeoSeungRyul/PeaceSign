#include "PSFishingWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"

// Used once for authoring the editable WBP and as a native-only fallback in tests.
void UPSFishingWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FishingRoot"));
	Tree->RootWidget = Root;
	const auto Text = [Tree](const FName Name, const TCHAR* Value, const int32 FontSize)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = WithProjectFont(Label->GetFont()); Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetText(FText::FromString(Value));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(.25f, .15f, .1f)));
		Label->SetJustification(ETextJustify::Center);
		return Label;
	};
	const auto Panel = [Tree, Root](const FName Name, const FVector2D Position, const FVector2D Size)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetPadding(FMargin(16));
		Border->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.91f, .63f, .35f), 14, FLinearColor(.38f, .2f, .12f), 4));
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Border);
		Slot->SetPosition(Position); Slot->SetSize(Size); Slot->SetAlignment(FVector2D(.5f, 1));
		return Border;
	};
	const UPSFishingWidget* Defaults = GetDefault<UPSFishingWidget>();
	const int32 SlotCount = FMath::Clamp(Defaults->VisibleDirectionSlots, 1, 15);
	const int32 CurrentSlot = FMath::Clamp(Defaults->CurrentDirectionSlot, 0, SlotCount - 1);
	UBorder* Game = Panel(TEXT("FishMinigamePanel"), FVector2D(960, 300), FVector2D(SlotCount * 98 + 32, 210));
	UVerticalBox* GameRows = Tree->ConstructWidget<UVerticalBox>(); Game->SetContent(GameRows);
	GameRows->AddChildToVerticalBox(Text(TEXT("StateLabel"), TEXT("노란 칸의 화살표 방향을 입력하세요"), 20));
	UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TimeBar"));
	Bar->SetFillColorAndOpacity(FLinearColor(.15f, .8f, .6f));
	USizeBox* BarSize = Tree->ConstructWidget<USizeBox>(); BarSize->SetHeightOverride(14); BarSize->SetContent(Bar);
	GameRows->AddChildToVerticalBox(BarSize)->SetPadding(FMargin(8, 10));
	UHorizontalBox* Directions = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FishDirectionRow"));
	GameRows->AddChildToVerticalBox(Directions)->SetPadding(FMargin(0, 20, 0, 0));
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(88); Size->SetHeightOverride(88);
		UBorder* Box = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("FishDirectionBox%d"), Index));
		Box->SetBrush(Index == CurrentSlot ? Defaults->CurrentSlotBrush : Defaults->PendingSlotBrush);
		Box->SetPadding(FMargin(5));
		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(); Box->SetContent(Overlay);
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("DirectionImage%d"), Index));
		Image->SetBrush(Defaults->UpArrowBrush);
		UOverlaySlot* ImageSlot = Overlay->AddChildToOverlay(Image);
		ImageSlot->SetHorizontalAlignment(HAlign_Center); ImageSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Check = Text(*FString::Printf(TEXT("DirectionLabel%d"), Index), TEXT("✓"), 30);
		UOverlaySlot* CheckSlot = Overlay->AddChildToOverlay(Check);
		CheckSlot->SetHorizontalAlignment(HAlign_Center); CheckSlot->SetVerticalAlignment(VAlign_Center);
		Check->SetVisibility(ESlateVisibility::Collapsed);
		UImage* Marker = Tree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("DirectionMarker%d"), Index));
		Marker->SetBrush(Defaults->CurrentDirectionMarkerBrush);
		Marker->SetDesiredSizeOverride(FVector2D(24, 18));
		UOverlaySlot* MarkerSlot = Overlay->AddChildToOverlay(Marker);
		MarkerSlot->SetHorizontalAlignment(HAlign_Center); MarkerSlot->SetVerticalAlignment(VAlign_Top);
		MarkerSlot->SetPadding(FMargin(0, -24, 0, 0));
		Size->SetContent(Box); Directions->AddChildToHorizontalBox(Size)->SetPadding(FMargin(5, 0));
	}
	UBorder* Casting = Panel(TEXT("FishCastPanel"), FVector2D(1060, 400), FVector2D(540, 125));
	UVerticalBox* CastRows = Tree->ConstructWidget<UVerticalBox>(); Casting->SetContent(CastRows);
	CastRows->AddChildToVerticalBox(Text(TEXT("CastLabel"), TEXT("우클릭으로 투척!"), 19));
	UCanvasPanel* Track = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FishCastTrack"));
	USizeBox* TrackSize = Tree->ConstructWidget<USizeBox>(); TrackSize->SetWidthOverride(480); TrackSize->SetHeightOverride(24); TrackSize->SetContent(Track);
	UVerticalBoxSlot* TrackSlot = CastRows->AddChildToVerticalBox(TrackSize); TrackSlot->SetHorizontalAlignment(HAlign_Center); TrackSlot->SetPadding(FMargin(0, 20, 0, 0));
	const FName Names[] = {TEXT("CastGreen"), TEXT("CastYellow"), TEXT("CastRed"), TEXT("CastYellowRight"), TEXT("CastGreenRight")};
	const FLinearColor Colors[] = {FLinearColor(.2f, .8f, .4f), FLinearColor(1, .8f, .2f), FLinearColor(.95f, .25f, .2f), FLinearColor(1, .8f, .2f), FLinearColor(.2f, .8f, .4f)};
	const FVector4f Edges = FPSFishingCastBands().GetEdges();
	const float Boundaries[] = {0, Edges.X, Edges.Y, Edges.Z, Edges.W, 1};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		UBorder* Zone = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Names[Index]); Zone->SetBrushColor(Colors[Index]);
		UCanvasPanelSlot* ZoneSlot = Track->AddChildToCanvas(Zone);
		ZoneSlot->SetAnchors(FAnchors(Boundaries[Index], 0, Boundaries[Index + 1], 1)); ZoneSlot->SetOffsets(FMargin(0));
	}
	UImage* Marker = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CastMarker"));
	Marker->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.35f, .75f, .85f), 6, FLinearColor(.15f, .4f, .5f), 3));
	UCanvasPanelSlot* MarkerSlot = Track->AddChildToCanvas(Marker); MarkerSlot->SetAnchors(FAnchors(0, .5f));
	MarkerSlot->SetAlignment(FVector2D(.5f)); MarkerSlot->SetPosition(FVector2D::ZeroVector); MarkerSlot->SetSize(FVector2D(20, 46));
	UBorder* Result = Panel(TEXT("FishResultPanel"), FVector2D(24, -24), FVector2D(420, 185));
	UCanvasPanelSlot* ResultSlot = Cast<UCanvasPanelSlot>(Result->Slot); ResultSlot->SetAnchors(FAnchors(0, 1)); ResultSlot->SetAlignment(FVector2D(0, 1));
	UHorizontalBox* ResultRow = Tree->ConstructWidget<UHorizontalBox>(); Result->SetContent(ResultRow);
	UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ResultIcon")); Icon->SetDesiredSizeOverride(FVector2D(128));
	ResultRow->AddChildToHorizontalBox(Icon)->SetVerticalAlignment(VAlign_Center);
	UVerticalBox* ResultInfo = Tree->ConstructWidget<UVerticalBox>();
	ResultRow->AddChildToHorizontalBox(ResultInfo)->SetPadding(FMargin(12, 0));
	ResultInfo->AddChildToVerticalBox(Text(TEXT("ResultNameLabel"), TEXT("물고기"), 20));
	ResultInfo->AddChildToVerticalBox(Text(TEXT("ResultSizeLabel"), TEXT("20 cm"), 20));
	ResultInfo->AddChildToVerticalBox(Text(TEXT("ResultLabel"), TEXT("물고기를 놓쳤습니다"), 20));
	UBorder* Notice = Panel(TEXT("FishNoticePanel"), FVector2D(24, -24), FVector2D(440, 80));
	auto* NoticeSlot = CastChecked<UCanvasPanelSlot>(Notice->Slot);
	NoticeSlot->SetAnchors(FAnchors(0, 1)); NoticeSlot->SetAlignment(FVector2D(0, 1));
	Notice->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1, .85f, .8f), 14, FLinearColor(.8f, .15f, .1f), 3));
	UVerticalBox* NoticeRows = Tree->ConstructWidget<UVerticalBox>(); Notice->SetContent(NoticeRows);
	UTextBlock* Cross = Text(TEXT("NoticeCross"), TEXT("X"), 22); Cross->SetColorAndOpacity(FSlateColor(FLinearColor(.8f, .1f, .1f)));
	UBorder* CrossPanel = Panel(TEXT("FishNoticeCrossPanel"), FVector2D(960, 400), FVector2D(64, 64));
	FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType; CrossPanel->SetBrush(Empty);
	CrossPanel->SetContent(Cross);
	NoticeRows->AddChildToVerticalBox(Text(TEXT("NoticeLabel"), TEXT("물에서 해야 해요"), 18));
	AddBitePanel(Tree);
}

void UPSFishingWidget::AddBitePanel(UWidgetTree* Tree)
{
	UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
	if (!Root || Tree->FindWidget(TEXT("FishBitePanel"))) return;
	UBorder* Bubble = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FishBitePanel"));
	Bubble->SetPadding(FMargin(10));
	Bubble->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1, .85f, .15f), 20, FLinearColor(.5f, .3f, .05f), 3));
	UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Bubble);
	Slot->SetPosition(FVector2D(960, 180)); Slot->SetSize(FVector2D(150, 110)); Slot->SetAlignment(FVector2D(.5f, 1));
	UVerticalBox* Rows = Tree->ConstructWidget<UVerticalBox>(); Bubble->SetContent(Rows);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Index == 0 ? TEXT("!") : TEXT("F / 우클릭")));
		FSlateFontInfo Font = WithProjectFont(Text->GetFont()); Font.Size = Index == 0 ? 42 : 16; Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(.25f, .15f, .05f)));
		Text->SetJustification(ETextJustify::Center);
		Rows->AddChildToVerticalBox(Text);
	}
}
