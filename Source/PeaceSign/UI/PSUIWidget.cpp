#include "PSUIWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SToolTip.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

UPSUIWidget::UPSUIWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/UI/Font/NanumFontSetup_TTF_SQUARE_ROUND/NanumSquareRoundR_Font.NanumSquareRoundR_Font"));
	ProjectFont = Font.Object;
}

UFont* UPSUIWidget::GetProjectFont()
{
	return GetDefault<UPSUIWidget>()->ProjectFont;
}

FSlateFontInfo UPSUIWidget::WithProjectFont(FSlateFontInfo Font)
{
	if (UFont* SharedFont = GetProjectFont())
	{
		Font.FontObject = SharedFont;
		// CoreStyle fonts carry an inline composite, which must not override our asset.
		Font.CompositeFont.Reset();
		const FCompositeFont* Composite = SharedFont->GetCompositeFont();
		Font.TypefaceFontName = Composite && !Composite->DefaultTypeface.Fonts.IsEmpty()
			? Composite->DefaultTypeface.Fonts[0].Name : NAME_None;
	}
	return Font;
}

FSlateFontInfo UPSUIWidget::MakeProjectFont(const float Size)
{
	FSlateFontInfo Font;
	Font.Size = Size;
	return WithProjectFont(Font);
}

void UPSUIWidget::SetProjectToolTipText(const FText& Text)
{
	// A custom tooltip may contain supplied artwork. Never replace that widget.
	if (GetToolTip() || ToolTipWidgetDelegate.IsBound()) return;
	SetToolTipText(Text);
	ApplyProjectToolTip();
}

void UPSUIWidget::ApplyProjectToolTip()
{
	const TSharedPtr<SWidget> SlateWidget = GetCachedWidget();
	if (!SlateWidget || GetToolTip() || ToolTipWidgetDelegate.IsBound() || GetToolTipText().IsEmpty()) return;
	SlateWidget->SetToolTip(SNew(SToolTip)
		.Text(GetToolTipText())
		.Font(WithProjectFont(FCoreStyle::Get().GetFontStyle("ToolTip.Font"))));
}

void UPSUIWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	// UWidget creates text tooltips with the engine font when Slate is rebuilt.
	ApplyProjectToolTip();
}

int32 UPSUIWidget::ApplyFontToTree(UWidgetTree* Tree, const bool bModify)
{
	if (!Tree) return 0;
	int32 Changed = 0;
	Tree->ForEachWidgetAndDescendants([bModify, &Changed](UWidget* Widget)
	{
		if (auto* Text = Cast<UTextBlock>(Widget))
		{
			const FSlateFontInfo Font = WithProjectFont(Text->GetFont());
			if (Font != Text->GetFont())
			{
				if (bModify) Text->Modify();
				Text->SetFont(Font);
				++Changed;
			}
		}
	});
	return Changed;
}

void UPSUIWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	// All native layouts exist by this point; also runs for Designer previews.
	ApplyFontToTree(WidgetTree, false);
}

int32 UPSUIWidget::ApplyDesignerFont(UObject* WidgetBlueprintAsset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprintAsset);
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(UUserWidget::StaticClass()) || !Blueprint->WidgetTree) return -1;
	Blueprint->Modify();
	const int32 Changed = ApplyFontToTree(Blueprint->WidgetTree, true);
	if (Changed > 0) FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return Changed;
#else
	return -1;
#endif
}
