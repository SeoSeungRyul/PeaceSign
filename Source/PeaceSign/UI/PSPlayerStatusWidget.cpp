#include "PSPlayerStatusWidget.h"
#include "../PSPlayerStatsComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"

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
		BuildEquipmentPanel();
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
	BuildEquipmentPanel();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshStats();
}

void UPSPlayerStatusWidget::BuildEquipmentPanel()
{
	UWidget* StatusRoot = WidgetTree->RootWidget;
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Layout;
	USizeBox* StatusSize = WidgetTree->ConstructWidget<USizeBox>();
	StatusSize->SetHeightOverride(220.0f);
	StatusSize->SetContent(StatusRoot);
	Layout->AddChildToVerticalBox(StatusSize);
	EquipmentPanel = WidgetTree->ConstructWidget<UBorder>();
	EquipmentPanel->SetPadding(FMargin(14.0f, 10.0f));
	Layout->AddChildToVerticalBox(EquipmentPanel)->SetPadding(FMargin(0, 10, 0, 0));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	EquipmentPanel->SetContent(Rows);
	EquipmentLabel = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = EquipmentLabel->GetFont();
	Font.Size = 22;
	Font.TypefaceFontName = FName(TEXT("Bold"));
	EquipmentLabel->SetFont(Font);
	Rows->AddChildToVerticalBox(EquipmentLabel);
	EquipmentHint = WidgetTree->ConstructWidget<UTextBlock>();
	Font.Size = 12;
	EquipmentHint->SetFont(Font);
	EquipmentHint->SetAutoWrapText(true);
	Rows->AddChildToVerticalBox(EquipmentHint)->SetPadding(FMargin(0, 6, 0, 0));
	SetEquipment(EPSEquipment::BareHands);
}

void UPSPlayerStatusWidget::SetEquipment(const EPSEquipment InEquipment)
{
	if (!EquipmentLabel || !EquipmentHint || !EquipmentPanel) return;
	const bool bHoe = InEquipment == EPSEquipment::Hoe;
	const bool bSeed = InEquipment == EPSEquipment::Seed;
	EquipmentLabel->SetText(bHoe
		? NSLOCTEXT("Equipment", "Hoe", "현재 장비  ·  괭이")
		: bSeed
			? NSLOCTEXT("Equipment", "Seed", "현재 장비  ·  씨앗")
			: NSLOCTEXT("Equipment", "BareHands", "현재 장비  ·  맨손"));
	EquipmentHint->SetText(bHoe
		? NSLOCTEXT("Equipment", "HoeHint", "우클릭 · 밭 갈기\n[1] 괭이  [2] 씨앗  [0] 맨손")
		: bSeed
			? NSLOCTEXT("Equipment", "SeedHint", "우클릭 · 씨앗 심기\n[1] 괭이  [2] 씨앗  [0] 맨손")
			: NSLOCTEXT("Equipment", "BareHandsHint", "[1] 괭이  [2] 씨앗  [0] 맨손"));
	EquipmentLabel->SetColorAndOpacity(FSlateColor(bHoe
		? FLinearColor(1.0f, 0.8f, 0.25f)
		: bSeed ? FLinearColor(0.5f, 1.0f, 0.45f) : FLinearColor::White));
	EquipmentPanel->SetBrushColor(bHoe
		? FLinearColor(0.12f, 0.19f, 0.07f, 0.96f)
		: bSeed ? FLinearColor(0.06f, 0.18f, 0.08f, 0.96f) : FLinearColor(0.04f, 0.065f, 0.10f, 0.96f));
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
