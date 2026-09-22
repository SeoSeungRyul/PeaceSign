#include "PSFishingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
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
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	const FLinearColor WoodFace(0.91f, 0.63f, 0.35f, 0.98f);
	const FLinearColor WoodEdge(0.38f, 0.20f, 0.12f, 1.0f);
	const FLinearColor CreamFace(1.0f, 0.91f, 0.72f, 1.0f);
	const FLinearColor CreamEdge(0.69f, 0.43f, 0.25f, 1.0f);
	const FLinearColor CurrentFace(1.0f, 0.79f, 0.24f, 1.0f);
	const FLinearColor CurrentEdge(0.91f, 0.31f, 0.16f, 1.0f);
	const FLinearColor CompleteFace(0.35f, 0.88f, 0.73f, 1.0f);
	const FLinearColor CompleteEdge(0.05f, 0.43f, 0.35f, 1.0f);
	const FLinearColor ArrowColor(0.31f, 0.20f, 0.15f, 1.0f);

	FSlateBrush RoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline, const float Width)
	{
		return FSlateRoundedBoxBrush(Fill, 14.0f, Outline, Width);
	}

	FBox2f DirectionUV(const EPSFishingDirection Direction)
	{
		// Square UV regions around the four brown arrows in T_FishingUIAtlas (1254x1254).
		FVector2f Min;
		switch (Direction)
		{
		case EPSFishingDirection::Up: Min = FVector2f(925, 405); break;
		case EPSFishingDirection::Left: Min = FVector2f(500, 420); break;
		case EPSFishingDirection::Down: Min = FVector2f(510, 700); break;
		default: Min = FVector2f(100, 715); break;
		}
		constexpr float AtlasSize = 1254.0f;
		return FBox2f(Min / AtlasSize, (Min + FVector2f(220, 220)) / AtlasSize);
	}

	FSlateBrush DirectionBrush(UTexture2D* Atlas, const EPSFishingDirection Direction)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Atlas);
		Brush.SetImageSize(FVector2D(62, 62));
		Brush.SetUVRegion(DirectionUV(Direction));
		return Brush;
	}
}

void UPSFishingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UPSFishingWidget::BuildWidgetTree()
{
	if (Panel || !WidgetTree) return;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetPadding(FMargin(24, 16, 24, 18));
	Panel->SetBrush(RoundedBrush(WoodFace, WoodEdge, 4.0f));
	Panel->SetBrushColor(FLinearColor::White);
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.18f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetSize(FVector2D(590, 174));

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Rows);
	StateLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = StateLabel->GetFont();
	Font.Size = 21;
	Font.TypefaceFontName = TEXT("Bold");
	StateLabel->SetFont(Font);
	StateLabel->SetColorAndOpacity(FSlateColor(ArrowColor));
	StateLabel->SetShadowOffset(FVector2D(0, 1));
	StateLabel->SetShadowColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.58f, 0.75f));
	StateLabel->SetJustification(ETextJustify::Center);
	Rows->AddChildToVerticalBox(StateLabel);
	ResultIcon = WidgetTree->ConstructWidget<UImage>();
	ResultIcon->SetDesiredSizeOverride(FVector2D(48, 48));
	ResultIcon->SetVisibility(ESlateVisibility::Collapsed);
	Rows->AddChildToVerticalBox(ResultIcon)->SetHorizontalAlignment(HAlign_Center);
	TimeBar = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle BarStyle;
	BarStyle.SetBackgroundImage(RoundedBrush(FLinearColor(0.52f, 0.37f, 0.28f, 1.0f), FLinearColor(1.0f, 0.91f, 0.72f, 1.0f), 3.0f));
	BarStyle.SetFillImage(RoundedBrush(CompleteFace, CompleteEdge, 2.0f));
	TimeBar->SetWidgetStyle(BarStyle);
	TimeBar->SetFillColorAndOpacity(FLinearColor::White);
	Rows->AddChildToVerticalBox(TimeBar)->SetPadding(FMargin(12, 10, 12, 12));
	DirectionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Rows->AddChildToVerticalBox(DirectionRow);
	DirectionAtlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Fishing/T_FishingUIAtlas.T_FishingUIAtlas"));
	for (int32 Index = 0; Index < 6; ++Index)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(78);
		Size->SetHeightOverride(62);
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
		Box->SetPadding(FMargin(6));
		Box->SetBrush(RoundedBrush(CreamFace, CreamEdge, 4.0f));
		Box->SetBrushColor(FLinearColor::White);
		UOverlay* Content = WidgetTree->ConstructWidget<UOverlay>();
		UImage* Arrow = WidgetTree->ConstructWidget<UImage>();
		UOverlaySlot* ArrowSlot = Content->AddChildToOverlay(Arrow);
		ArrowSlot->SetHorizontalAlignment(HAlign_Center);
		ArrowSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Font.Size = 36;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(1.0f, 0.90f, 0.70f, 0.9f);
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(ArrowColor));
		Label->SetShadowOffset(FVector2D(0, 1));
		Label->SetShadowColorAndOpacity(FLinearColor(1, 1, 1, 0.55f));
		Label->SetJustification(ETextJustify::Center);
		UOverlaySlot* CheckSlot = Content->AddChildToOverlay(Label);
		CheckSlot->SetHorizontalAlignment(HAlign_Center);
		CheckSlot->SetVerticalAlignment(VAlign_Center);
		Box->SetContent(Content);
		Size->SetContent(Box);
		DirectionRow->AddChildToHorizontalBox(Size)->SetPadding(FMargin(5, 0));
		DirectionBoxes.Add(Box);
		DirectionImages.Add(Arrow);
		DirectionLabels.Add(Label);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UPSFishingWidget::Refresh(const EPSFishingState State, const float TimeRemaining, const float StateDuration,
	const TArray<EPSFishingDirection>& Sequence, const int32 SequenceIndex,
	const FText& FishName, const int32 FishSizeCm, UTexture2D* FishIcon, const float ResultOpacity)
{
	BuildWidgetTree();
	if (!Panel || !StateLabel || !TimeBar) return;
	if (State == EPSFishingState::Idle)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Panel->SetRenderOpacity(FMath::Clamp(ResultOpacity, 0.0f, 1.0f));
	Panel->SetBrush(State == EPSFishingState::Success
		? RoundedBrush(CompleteFace, CompleteEdge, 4.0f)
		: State == EPSFishingState::Failure || State == EPSFishingState::BiteWindow
			? RoundedBrush(CurrentFace, CurrentEdge, 4.0f)
			: RoundedBrush(WoodFace, WoodEdge, 4.0f));
	TimeBar->SetPercent(StateDuration > 0 ? FMath::Clamp(TimeRemaining / StateDuration, 0.0f, 1.0f) : 0.0f);
	TimeBar->SetVisibility(State == EPSFishingState::Success || State == EPSFishingState::Failure
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	DirectionRow->SetVisibility(State == EPSFishingState::Minigame ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ResultIcon->SetVisibility(State == EPSFishingState::Success && FishIcon
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (State == EPSFishingState::Success && FishIcon) ResultIcon->SetBrushFromTexture(FishIcon, true);
	switch (State)
	{
	case EPSFishingState::WaitingForBite: StateLabel->SetText(NSLOCTEXT("Fishing", "Waiting", "찌를 바라보는 중...  F / 우클릭 취소")); break;
	case EPSFishingState::BiteWindow: StateLabel->SetText(NSLOCTEXT("Fishing", "Bite", "! 입질 !  F 또는 우클릭")); break;
	case EPSFishingState::Minigame: StateLabel->SetText(NSLOCTEXT("Fishing", "Minigame", "노란 칸의 화살표 방향을 입력하세요")); break;
	case EPSFishingState::Success:
		StateLabel->SetText(FText::Format(NSLOCTEXT("Fishing", "Success", "{0}  {1}cm 획득!"), FishName, FText::AsNumber(FishSizeCm)));
		break;
	case EPSFishingState::Failure: StateLabel->SetText(NSLOCTEXT("Fishing", "Failure", "물고기를 놓쳤습니다")); break;
	default: break;
	}
	for (int32 DisplayIndex = 0; DisplayIndex < DirectionLabels.Num(); ++DisplayIndex)
	{
		// The current answer stays in slot three. Two prior slots remain empty at the start.
		const int32 SequenceSlot = SequenceIndex + DisplayIndex - 2;
		const bool bHasDirection = Sequence.IsValidIndex(SequenceSlot);
		const bool bCompleted = bHasDirection && SequenceSlot < SequenceIndex;
		const bool bCurrent = bHasDirection && DisplayIndex == 2;
		DirectionImages[DisplayIndex]->SetVisibility(bHasDirection && !bCompleted && DirectionAtlas
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasDirection && !bCompleted && DirectionAtlas)
			DirectionImages[DisplayIndex]->SetBrush(DirectionBrush(DirectionAtlas, Sequence[SequenceSlot]));
		DirectionLabels[DisplayIndex]->SetVisibility(bCompleted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DirectionLabels[DisplayIndex]->SetText(bCompleted ? FText::FromString(TEXT("✓")) : FText::GetEmpty());
		DirectionLabels[DisplayIndex]->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		DirectionBoxes[DisplayIndex]->SetBrush(bCompleted
			? RoundedBrush(CompleteFace, CompleteEdge, 4.0f)
			: bCurrent ? RoundedBrush(CurrentFace, CurrentEdge, 4.0f)
				: RoundedBrush(CreamFace, CreamEdge, 4.0f));
	}
}
