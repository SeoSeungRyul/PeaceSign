#include "PSPlayerStatusWidget.h"
#include "../PSPlayerStatsComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UPSPlayerStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	const TCHAR* BarNames[] = {TEXT("HealthBar"), TEXT("StaminaBar"), TEXT("HungerBar"), TEXT("MentalBar")};
	const TCHAR* LabelNames[] = {TEXT("HealthLabel"), TEXT("StaminaLabel"), TEXT("HungerLabel"), TEXT("MentalLabel")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Bars.Add(Cast<UProgressBar>(WidgetTree->FindWidget(FName(BarNames[Index]))));
		Labels.Add(Cast<UTextBlock>(WidgetTree->FindWidget(FName(LabelNames[Index]))));
	}
	if (!Bars.Contains(nullptr) && !Labels.Contains(nullptr))
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		RefreshStats();
		return;
	}
	Bars.Reset();
	Labels.Reset();
	// Native layout also supplies a usable default when no Blueprint has been assigned.
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetPadding(FMargin(12.0f));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.9f));
	WidgetTree->RootWidget = Panel;
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Rows);
	const FLinearColor Colors[] = {FLinearColor(0.85f, 0.12f, 0.16f), FLinearColor(0.15f, 0.7f, 0.3f), FLinearColor(0.95f, 0.6f, 0.12f), FLinearColor(0.4f, 0.5f, 0.95f)};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 14;
		Label->SetFont(Font);
		Rows->AddChildToVerticalBox(Label)->SetPadding(FMargin(0, 4, 0, 3));
		Labels.Add(Label);
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
		Bar->SetFillColorAndOpacity(Colors[Index]);
		Rows->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0, 0, 0, 5));
		Bars.Add(Bar);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshStats();
}

void UPSPlayerStatusWidget::SetStatsComponent(UPSPlayerStatsComponent* InStats)
{
	if (Stats) Stats->OnStatsChanged.RemoveDynamic(this, &ThisClass::RefreshStats);
	Stats = InStats;
	if (Stats) Stats->OnStatsChanged.AddUniqueDynamic(this, &ThisClass::RefreshStats);
	RefreshStats();
}

void UPSPlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Stats) Stats->OnStatsChanged.AddUniqueDynamic(this, &ThisClass::RefreshStats);
	RefreshStats();
}

void UPSPlayerStatusWidget::NativeDestruct()
{
	if (Stats) Stats->OnStatsChanged.RemoveDynamic(this, &ThisClass::RefreshStats);
	Super::NativeDestruct();
}

void UPSPlayerStatusWidget::RefreshStats()
{
	if (Bars.Num() != 4 || Labels.Num() != 4) return;
	const float Values[] = {Stats ? Stats->Health : 0.0f, Stats ? Stats->Stamina : 0.0f, Stats ? Stats->Hunger : 0.0f, Stats ? Stats->MentalHealth : 0.0f};
	const float Maxima[] = {Stats ? Stats->MaxHealth : 100.0f, Stats ? Stats->MaxStamina : 100.0f, 100.0f, 100.0f};
	const TCHAR* Names[] = {TEXT("HP"), TEXT("SP"), TEXT("Hunger"), TEXT("Mental")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Bars[Index]->SetPercent(Maxima[Index] > 0.0f ? FMath::Clamp(Values[Index] / Maxima[Index], 0.0f, 1.0f) : 0.0f);
		Labels[Index]->SetText(FText::FromString(Stats ? FString::Printf(TEXT("%s  %.0f / %.0f"), Names[Index], Values[Index], Maxima[Index]) : FString::Printf(TEXT("%s  --"), Names[Index])));
	}
}
