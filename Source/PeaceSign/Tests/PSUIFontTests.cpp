#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../UI/PSUIWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "WidgetBlueprint.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/IToolTip.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSUIFontTest, "PeaceSign.UI.ProjectFont",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSUIFontTest::RunTest(const FString&)
{
	UFont* Font = UPSUIWidget::GetProjectFont();
	if (!TestNotNull(TEXT("Requested NanumSquareRoundR Font asset loads"), Font)) return false;
	TestEqual(TEXT("Font is the imported regular face requested by the user"), Font->GetPathName(),
		FString(TEXT("/Game/UI/Font/NanumFontSetup_TTF_SQUARE_ROUND/NanumSquareRoundR_Font.NanumSquareRoundR_Font")));
	TestTrue(TEXT("Font supports Runtime UMG caching"), Font->FontCacheType == EFontCacheType::Runtime);
	const FCompositeFont* Composite = Font->GetCompositeFont();
	TestTrue(TEXT("Font contains an actual typeface"), Composite && !Composite->DefaultTypeface.Fonts.IsEmpty());

	FSlateFontInfo Original = FCoreStyle::GetDefaultFontStyle("Bold", 17);
	Original.LetterSpacing = 25;
	Original.OutlineSettings.OutlineSize = 2;
	const FSlateFontInfo Applied = UPSUIWidget::WithProjectFont(Original);
	TestEqual(TEXT("Slate painting uses the same project asset"), Applied.FontObject.Get(), static_cast<const UObject*>(Font));
	TestFalse(TEXT("Inline default composite cannot silently retain Roboto"), Applied.CompositeFont.IsValid());
	TestEqual(TEXT("Changing family preserves font size"), Applied.Size, Original.Size);
	TestEqual(TEXT("Changing family preserves letter spacing"), Applied.LetterSpacing, Original.LetterSpacing);
	TestEqual(TEXT("Changing family preserves outline"), Applied.OutlineSettings.OutlineSize, Original.OutlineSettings.OutlineSize);
	TestEqual(TEXT("Hotbar key text keeps its original size"), UPSUIWidget::MakeProjectFont(9).Size, 9.f);
	TestEqual(TEXT("Stack count text keeps its original size"), UPSUIWidget::MakeProjectFont(11).Size, 11.f);

	// PreConstruct must reach nested WBP trees as well as top-level native text.
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
	auto* Root = CreateWidget<UPSUIWidget>(World);
	auto* Nested = CreateWidget<UPSUIWidget>(World);
	if (TestNotNull(TEXT("Common UI widget creates"), Root) && TestNotNull(TEXT("Nested UI widget creates"), Nested))
	{
		auto* Canvas = Root->WidgetTree->ConstructWidget<UCanvasPanel>(); Root->WidgetTree->RootWidget = Canvas;
		auto* Text = Root->WidgetTree->ConstructWidget<UTextBlock>(); Text->SetFont(Original); Canvas->AddChildToCanvas(Text);
		auto* NestedText = Nested->WidgetTree->ConstructWidget<UTextBlock>(); NestedText->SetFont(Original);
		Nested->WidgetTree->RootWidget = NestedText; Canvas->AddChildToCanvas(Nested);
		auto* Image = Root->WidgetTree->ConstructWidget<UImage>();
		FSlateBrush OriginalArt;
		OriginalArt.SetResourceObject(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Art/Inventory/Inven/Inven_Background.Inven_Background")));
		OriginalArt.ImageSize = FVector2D(64, 32);
		OriginalArt.TintColor = FLinearColor(.7f, .8f, .9f);
		Image->SetBrush(OriginalArt); Canvas->AddChildToCanvas(Image);
		Root->SetProjectToolTipText(FText::FromString(TEXT("가방 확장이 필요합니다")));
		// UWidget caches Slate weakly; retain the returned widget for tooltip inspection.
		const TSharedRef<SWidget> RootSlate = Root->TakeWidget();
		TestEqual(TEXT("Lifecycle applies project font to top-level text"), Text->GetFont().FontObject.Get(), static_cast<const UObject*>(Font));
		TestEqual(TEXT("Lifecycle applies project font to nested text"), NestedText->GetFont().FontObject.Get(), static_cast<const UObject*>(Font));
		TestEqual(TEXT("Lifecycle retains authored size"), Text->GetFont().Size, Original.Size);
		TestTrue(TEXT("Applying fonts preserves inventory image brush and resource"), Image->GetBrush() == OriginalArt);
		auto Tooltip = RootSlate->GetToolTip();
		if (TestTrue(TEXT("Text tooltip exists after Slate construction"), Tooltip.IsValid()))
		{
			const auto Content = Tooltip->GetContentWidget();
			if (TestEqual(TEXT("Tooltip keeps the engine's text-only structure"), Content->GetType(), FName(TEXT("STextBlock"))))
			{
				const auto SlateText = StaticCastSharedRef<STextBlock>(Content);
				TestEqual(TEXT("Tooltip uses project font rather than engine default"), SlateText->GetFont().FontObject.Get(), static_cast<const UObject*>(Font));
				TestEqual(TEXT("Tooltip retains original engine font size"), SlateText->GetFont().Size, FCoreStyle::Get().GetFontStyle("ToolTip.Font").Size);
			}
		}
		Root->SetProjectToolTipText(FText::FromString(TEXT("물고기")));
		Root->SynchronizeProperties();
		Tooltip = RootSlate->GetToolTip();
		if (TestTrue(TEXT("Tooltip survives property synchronization"), Tooltip.IsValid()))
		{
			const auto Content = Tooltip->GetContentWidget();
			if (TestEqual(TEXT("Synchronized tooltip keeps text-only content"), Content->GetType(), FName(TEXT("STextBlock"))))
			{
				const auto SlateText = StaticCastSharedRef<STextBlock>(Content);
				TestEqual(TEXT("Updated tooltip retains its item name"), SlateText->GetText().ToString(), FString(TEXT("물고기")));
				TestEqual(TEXT("Synchronized tooltip retains the project font"), SlateText->GetFont().FontObject.Get(), static_cast<const UObject*>(Font));
			}
		}
		// An artist's tooltip widget can contain baked lettering; do not replace it.
		auto* CustomTip = NewObject<UImage>(Root); CustomTip->SetBrush(OriginalArt);
		Root->SetToolTip(CustomTip);
		const auto CustomSlateTip = RootSlate->GetToolTip();
		Root->SetProjectToolTipText(FText::FromString(TEXT("빈 슬롯")));
		TestEqual(TEXT("Custom tooltip widget remains assigned"), Root->GetToolTip(), static_cast<UWidget*>(CustomTip));
		TestTrue(TEXT("Custom image tooltip is not replaced by text"), RootSlate->GetToolTip() == CustomSlateTip);
		TestTrue(TEXT("Custom tooltip artwork is unmodified"), CustomTip->GetBrush() == OriginalArt);
	}
	GEngine->DestroyWorldContext(World); World->DestroyWorld(false);

	const TCHAR* Assets[] = {
		TEXT("/Game/UI/WBP_PlayerStatus"), TEXT("/Game/UI/Fishing/WBP_Fishing"), TEXT("/Game/UI/Encyclopedia/WBP_Encyclopedia"),
		TEXT("/Game/UI/Inventory/WBP_Inventory"), TEXT("/Game/UI/Inventory/WBP_BagPanel"),
		TEXT("/Game/UI/Inventory/WBP_EquipmentPanel"), TEXT("/Game/UI/Inventory/WBP_EquipmentSlot"), TEXT("/Game/UI/Inventory/WBP_Information")};
	int32 TextCount = 0;
	for (const TCHAR* Path : Assets)
	{
		auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, Path);
		if (!TestNotNull(FString::Printf(TEXT("Existing UI loads: %s"), Path), Blueprint)) continue;
		Blueprint->WidgetTree->ForEachWidgetAndDescendants([this, Font, &TextCount, Path](UWidget* Widget)
		{
			if (auto* Text = Cast<UTextBlock>(Widget))
			{
				++TextCount;
				TestEqual(FString::Printf(TEXT("Designer font persisted: %s / %s"), Path, *Text->GetName()),
					Text->GetFont().FontObject.Get(), static_cast<const UObject*>(Font));
			}
		});
	}
	TestTrue(TEXT("Designer validation actually visits text widgets"), TextCount > 10);
	return true;
}
#endif
