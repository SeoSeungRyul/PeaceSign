#include "PSBushPromptWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UPSBushPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	auto* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetPadding(FMargin(14, 10));
	Panel->SetBrushColor(FLinearColor(0.03f, 0.09f, 0.04f, 0.85f));
	WidgetTree->RootWidget = Panel;
	auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Rows);
	Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetFont(MakeProjectFont(16));
	Rows->AddChildToVerticalBox(Label);
	Gauge = WidgetTree->ConstructWidget<UProgressBar>();
	Gauge->SetFillColorAndOpacity(FLinearColor(0.4f, 0.85f, 0.25f));
	Rows->AddChildToVerticalBox(Gauge);
}

void UPSBushPromptWidget::ShowPrompt(const FString& Message, const float Progress)
{
	Label->SetText(FText::FromString(Message));
	Gauge->SetPercent(Progress);
	Gauge->SetVisibility(Progress > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
