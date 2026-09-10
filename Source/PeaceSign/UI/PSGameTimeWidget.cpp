#include "PSGameTimeWidget.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"

void UPSGameTimeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetPadding(FMargin(16.0f, 12.0f));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.9f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	PanelSlot->SetPosition(FVector2D(-24.0f, 24.0f));
	PanelSlot->SetSize(FVector2D(220.0f, 94.0f));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Rows);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(NSLOCTEXT("GameTime", "Title", "게임 시간"));
	FSlateFontInfo Font = Title->GetFont();
	Font.Size = 14;
	Title->SetFont(Font);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.75f, 0.8f)));
	Rows->AddChildToVerticalBox(Title);
	TimeLabel = WidgetTree->ConstructWidget<UTextBlock>();
	Font.Size = 24;
	Font.TypefaceFontName = TEXT("Bold");
	TimeLabel->SetFont(Font);
	TimeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.5f)));
	Rows->AddChildToVerticalBox(TimeLabel)->SetPadding(FMargin(0, 6, 0, 0));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPSGameTimeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GameTime = GetWorld()->GetSubsystem<UPSGameTimeSubsystem>();
	if (GameTime) GameTime->OnClockChanged.AddUniqueDynamic(this, &ThisClass::RefreshClock);
	RefreshClock();
}

void UPSGameTimeWidget::NativeDestruct()
{
	if (GameTime) GameTime->OnClockChanged.RemoveDynamic(this, &ThisClass::RefreshClock);
	GameTime = nullptr;
	Super::NativeDestruct();
}

void UPSGameTimeWidget::RefreshClock()
{
	if (!TimeLabel || !GameTime) return;
	const int32 Minute = GameTime->GetMinuteOfDay();
	const int32 Hour = Minute / 60;
	TimeLabel->SetText(FText::Format(NSLOCTEXT("GameTime", "Clock", "{0} {1}"),
		Hour < 12 ? NSLOCTEXT("GameTime", "AM", "오전") : NSLOCTEXT("GameTime", "PM", "오후"),
		FText::FromString(FString::Printf(TEXT("%02d:%02d"), Hour % 12 == 0 ? 12 : Hour % 12, Minute % 60))));
}
