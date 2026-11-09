#include "PSFishingWidget.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#endif

bool UPSFishingWidget::UpgradeOctober26Layout(UObject* Asset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
	auto* Defaults = Blueprint && Blueprint->GeneratedClass ? Cast<UPSFishingWidget>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!Defaults || !Blueprint->WidgetTree) return false;
	UWidgetTree* Tree = Blueprint->WidgetTree;
	auto* Row = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("FishDirectionRow")));
	if (!Row || (Row->GetChildrenCount() != 6 && Row->GetChildrenCount() != 7)) return false;
	if (Row->GetChildrenCount() == 6)
	{
		auto* SourceFrame = Cast<USizeBox>(Row->GetChildAt(5));
		auto* SourceBox = Cast<UBorder>(Tree->FindWidget(TEXT("FishDirectionBox5")));
		auto* SourceArrow = Cast<UImage>(Tree->FindWidget(TEXT("DirectionImage5")));
		auto* SourceMarker = Cast<UImage>(Tree->FindWidget(TEXT("DirectionMarker5")));
		auto* SourceScale = SourceArrow ? Cast<UScaleBox>(SourceArrow->GetParent()) : nullptr;
		if (!SourceFrame || !SourceBox || !SourceArrow || !SourceMarker || !SourceScale
			|| !Cast<UCanvasPanelSlot>(SourceBox->Slot) || !Cast<UCanvasPanelSlot>(SourceMarker->Slot)
			|| !Cast<UCanvasPanelSlot>(SourceScale->Slot)) return false;
		const TCHAR* NewNames[] = {TEXT("FishDirectionSlot6"), TEXT("FishDirectionCell6"), TEXT("FishDirectionBox6"),
			TEXT("FishDirectionContent6"), TEXT("FishDirectionScale6"), TEXT("DirectionImage6"), TEXT("DirectionMarker6")};
		for (const TCHAR* Name : NewNames) if (Tree->FindWidget(Name)) return false;
		Blueprint->Modify(); Tree->Modify(); Row->Modify();
		const auto PlaceLike = [](UCanvasPanel* Parent, UWidget* Child, const UWidget* Source)
		{
			auto* From = CastChecked<UCanvasPanelSlot>(Source->Slot);
			auto* To = Parent->AddChildToCanvas(Child);
			To->SetAnchors(From->GetAnchors()); To->SetOffsets(From->GetOffsets());
			To->SetAlignment(From->GetAlignment()); To->SetAutoSize(From->GetAutoSize());
			To->SetZOrder(From->GetZOrder());
		};
		auto* Frame = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FishDirectionSlot6"));
		Frame->SetWidthOverride(SourceFrame->GetWidthOverride()); Frame->SetHeightOverride(SourceFrame->GetHeightOverride());
		auto* Cell = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FishDirectionCell6")); Frame->SetContent(Cell);
		auto* Box = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FishDirectionBox6"));
		Box->SetBrush(SourceBox->Background); Box->SetPadding(SourceBox->GetPadding()); PlaceLike(Cell, Box, SourceBox);
		auto* Content = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FishDirectionContent6")); Box->SetContent(Content);
		auto* Scale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("FishDirectionScale6"));
		Scale->SetStretch(SourceScale->GetStretch()); Scale->SetStretchDirection(SourceScale->GetStretchDirection()); PlaceLike(Content, Scale, SourceScale);
		auto* Arrow = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DirectionImage6"));
		Arrow->SetBrush(SourceArrow->GetBrush()); Scale->SetContent(Arrow);
		auto* Marker = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DirectionMarker6"));
		Marker->SetBrush(SourceMarker->GetBrush()); PlaceLike(Cell, Marker, SourceMarker);
		auto* PreviousSlot = CastChecked<UHorizontalBoxSlot>(SourceFrame->Slot);
		const FMargin LastPadding = PreviousSlot->GetPadding();
		PreviousSlot->SetPadding(CastChecked<UHorizontalBoxSlot>(Row->GetChildAt(4)->Slot)->GetPadding());
		Row->AddChildToHorizontalBox(Frame)->SetPadding(LastPadding);
		if (auto* Slot = Cast<UCanvasPanelSlot>(Row->Slot))
		{
			const FVector2D OldSize = Slot->GetSize();
			const double AddedWidth = OldSize.X / 6;
			Slot->SetPosition(Slot->GetPosition() - FVector2D(AddedWidth / 2, 0));
			Slot->SetSize(OldSize + FVector2D(AddedWidth, 0));
		}
	}
	Blueprint->Modify(); Defaults->Modify();
	Defaults->VisibleDirectionSlots = 7; Defaults->CurrentDirectionSlot = 3;
	Tree->ForEachWidget([Blueprint](UWidget* Widget)
	{
		if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
			Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
	});
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
#else
	return false;
#endif
}

bool UPSFishingWidget::ApplyArtworkDesignerLayout(UObject* WidgetBlueprintAsset)
{
#if WITH_EDITOR
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprintAsset);
	UPSFishingWidget* Defaults = Blueprint && Blueprint->GeneratedClass
		? Cast<UPSFishingWidget>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!Defaults || !Blueprint->ParentClass->IsChildOf(StaticClass())) return false;
	// Validate every required image before replacing the prototype tree.
	TMap<FString, UTexture2D*> Textures;
	const auto Load = [&Textures](const FString& Relative) -> UTexture2D*
	{
		if (UTexture2D** Existing = Textures.Find(Relative)) return *Existing;
		const FString ObjectPath = TEXT("/Game/UI/Fishing/") + Relative + TEXT(".") + FPaths::GetCleanFilename(Relative);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
		Textures.Add(Relative, Texture);
		return Texture;
	};
	const TCHAR* Parts[] = {
		TEXT("Fish_Ballon/Fish_Ballon_Cant"), TEXT("Fish_Ballon/Fish_Ballon_Catch"),
		TEXT("Fish_Casting/Fish_Casting_Background"), TEXT("Fish_Casting/Fish_Casting_Main"), TEXT("Fish_Casting/Fish_Casting_Player"),
		TEXT("Fish_Catch/Fish_Catch_Background"), TEXT("Fish_Catch/Fish_Catch_Clip"), TEXT("Fish_Catch/Fish_Catch_Icon"),
		TEXT("Fish_Catch/Fish_Catch_Information"), TEXT("Fish_Catch/Fish_Catch_Name"),
		TEXT("Fish_Game/Fish_Game_Background1"), TEXT("Fish_Game/Fish_Game_Background2"), TEXT("Fish_Game/Fish_Game_Clip"),
		TEXT("Fish_Game/ArrowBox/ArrowBox_Now"), TEXT("Fish_Game/ArrowBox/ArrowBox_O"), TEXT("Fish_Game/ArrowBox/ArrowBox_X"), TEXT("Fish_Game/ArrowBox/NowArrow"),
		TEXT("Fish_Game/Timer/Timer_Bar"), TEXT("Fish_Game/Timer/Timer_Bar_BackGround"), TEXT("Fish_Game/Timer/Timer_icon"),
		TEXT("Fish_Tbg/Fish_Tbg_Cant1"), TEXT("Fish_Tbg/Fish_Tbg_Cant2")};
	for (const TCHAR* Part : Parts) if (!Load(Part)) return false;
	for (const TCHAR* State : {TEXT("Normal"), TEXT("Now"), TEXT("O")})
		for (const TCHAR* Direction : {TEXT("T"), TEXT("L"), TEXT("B"), TEXT("R")})
			if (!Load(FString::Printf(TEXT("Fish_Game/Arrow/Arrow_%s/Arrow_%s"), State, Direction))) return false;
	const auto Brush = [&Load](const FString& Relative)
	{
		UTexture2D* Texture = Load(Relative);
		FSlateBrush Result;
		Result.DrawAs = ESlateBrushDrawType::Image;
		Result.SetResourceObject(Texture);
		// Author from source dimensions: platform data can still be compiling in a commandlet.
		Result.SetImageSize(FVector2D(Texture->Source.GetSizeX(), Texture->Source.GetSizeY()));
		return Result;
	};
	const auto Colour = [](const uint8 R, const uint8 G, const uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	};
	Blueprint->Modify(); Defaults->Modify();
	Defaults->VisibleDirectionSlots = 7; Defaults->CurrentDirectionSlot = 3;
	Defaults->PendingSlotBrush = FSlateRoundedBoxBrush(Colour(196, 164, 118), 9, Colour(120, 79, 38), 4);
	Defaults->CurrentSlotBrush = Brush(TEXT("Fish_Game/ArrowBox/ArrowBox_Now"));
	Defaults->CompleteSlotBrush = Brush(TEXT("Fish_Game/ArrowBox/ArrowBox_O"));
	Defaults->WrongSlotBrush = Brush(TEXT("Fish_Game/ArrowBox/ArrowBox_X"));
	Defaults->UpArrowBrush = Brush(TEXT("Fish_Game/Arrow/Arrow_Normal/Arrow_T"));
	Defaults->LeftArrowBrush = Brush(TEXT("Fish_Game/Arrow/Arrow_Normal/Arrow_L"));
	Defaults->DownArrowBrush = Brush(TEXT("Fish_Game/Arrow/Arrow_Normal/Arrow_B"));
	Defaults->RightArrowBrush = Brush(TEXT("Fish_Game/Arrow/Arrow_Normal/Arrow_R"));
	const auto Arrows = [&Brush](const TCHAR* State, FPSFishingArrowBrushes& Set)
	{
		const FString Base = FString::Printf(TEXT("Fish_Game/Arrow/Arrow_%s/Arrow_"), State);
		Set.Up = Brush(Base + TEXT("T")); Set.Left = Brush(Base + TEXT("L"));
		Set.Down = Brush(Base + TEXT("B")); Set.Right = Brush(Base + TEXT("R"));
	};
	Arrows(TEXT("Now"), Defaults->CurrentArrowBrushes);
	Arrows(TEXT("O"), Defaults->CompleteArrowBrushes);
	Defaults->CurrentDirectionMarkerBrush = Brush(TEXT("Fish_Game/ArrowBox/NowArrow"));
	Defaults->CastMarkerBrush = Brush(TEXT("Fish_Casting/Fish_Casting_Player"));
	Defaults->GeneralNoticeBrush = Brush(TEXT("Fish_Game/Fish_Game_Background1"));
	Defaults->RodNoticeBrush = Brush(TEXT("Fish_Tbg/Fish_Tbg_Cant1"));
	Defaults->WaterNoticeBrush = Brush(TEXT("Fish_Tbg/Fish_Tbg_Cant2"));
	Defaults->MinigameOffset = FVector2D(0, -100);
	Defaults->CastingOffset = FVector2D(0, -100);
	Defaults->NoticeOffset = FVector2D(0, -120);
	Defaults->BiteOffset = FVector2D(0, -24);
	UWidgetTree* Tree = NewObject<UWidgetTree>(Blueprint, NAME_None, RF_Transactional);
	Blueprint->WidgetTree = Tree;
	Blueprint->WidgetVariableNameToGuidMap.Reset();
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FishingRoot"));
	Tree->RootWidget = Root;
	const auto Place = [](UCanvasPanel* Parent, UWidget* Child, const FVector2D Position, const FVector2D Size)
	{
		UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child);
		Slot->SetPosition(Position); Slot->SetSize(Size);
		return Slot;
	};
	const auto Image = [Tree, &Brush, &Place](UCanvasPanel* Parent, const FName Name, const FString& Relative,
		const FVector2D Position, const FVector2D Size)
	{
		UImage* Widget = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Widget->SetBrush(Brush(Relative)); Place(Parent, Widget, Position, Size);
		return Widget;
	};
	const auto Text = [Tree, &Place, &Colour](UCanvasPanel* Parent, const FName Name, const TCHAR* Value,
		const FVector2D Position, const FVector2D Size, const int32 Points)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = WithProjectFont(Label->GetFont()); Font.Size = Points;
		Label->SetFont(Font); Label->SetText(FText::FromString(Value));
		Label->SetColorAndOpacity(Colour(91, 53, 26)); Label->SetJustification(ETextJustify::Center);
		Place(Parent, Label, Position, Size);
		return Label;
	};
	const auto Panel = [Tree, Root, &Place](const FName Name, const FVector2D Position, const FVector2D Size)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		Border->SetBrush(Empty); Border->SetPadding(FMargin(0));
		UCanvasPanelSlot* Slot = Place(Root, Border, Position, Size); Slot->SetAlignment(FVector2D(.5f, 1));
		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(); Border->SetContent(Canvas);
		return Canvas;
	};
	// Keep every layer as a normal Designer widget, rather than a runtime-painted atlas.
	UCanvasPanel* Game = Panel(TEXT("FishMinigamePanel"), FVector2D(960, 400), FVector2D(885, 245));
	Image(Game, TEXT("GameWoodFrame"), TEXT("Fish_Game/Fish_Game_Background2"), FVector2D::ZeroVector, FVector2D(885, 245));
	Image(Game, TEXT("GamePaper"), TEXT("Fish_Game/Fish_Game_Background1"), FVector2D(7, 6), FVector2D(871, 232));
	Image(Game, TEXT("GameFishClips"), TEXT("Fish_Game/Fish_Game_Clip"), FVector2D(18, 8), FVector2D(848, 27));
	Text(Game, TEXT("StateLabel"), TEXT("노란 칸의 화살표 방향을 입력하세요"), FVector2D(170, 8), FVector2D(545, 22), 16);
	UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FishDirectionRow"));
	Place(Game, Row, FVector2D(65, 30), FVector2D(755, 130));
	for (int32 Index = 0; Index < 7; ++Index)
	{
		USizeBox* Frame = Tree->ConstructWidget<USizeBox>(); Frame->SetWidthOverride(95); Frame->SetHeightOverride(130);
		UCanvasPanel* Cell = Tree->ConstructWidget<UCanvasPanel>(); Frame->SetContent(Cell);
		Row->AddChildToHorizontalBox(Frame)->SetPadding(FMargin(0, 0, Index < 6 ? 15 : 0, 0));
		UBorder* Box = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("FishDirectionBox%d"), Index));
		Box->SetPadding(FMargin(0)); Box->SetBrush(Index == 3 ? Defaults->CurrentSlotBrush : Defaults->PendingSlotBrush);
		Place(Cell, Box, FVector2D(0, 32), FVector2D(95));
		UCanvasPanel* Content = Tree->ConstructWidget<UCanvasPanel>(); Box->SetContent(Content);
		UScaleBox* ArrowFit = Tree->ConstructWidget<UScaleBox>(); ArrowFit->SetStretch(EStretch::ScaleToFit);
		Place(Content, ArrowFit, FVector2D(19.5), FVector2D(56));
		UImage* Arrow = Tree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("DirectionImage%d"), Index));
		Arrow->SetBrush(Defaults->UpArrowBrush); ArrowFit->SetContent(Arrow);
		Image(Cell, *FString::Printf(TEXT("DirectionMarker%d"), Index), TEXT("Fish_Game/ArrowBox/NowArrow"), FVector2D(30, 0), FVector2D(35, 30));
	}
	Image(Game, TEXT("TimerIcon"), TEXT("Fish_Game/Timer/Timer_icon"), FVector2D(86, 179), FVector2D(28, 46));
	UProgressBar* Time = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TimeBar"));
	FProgressBarStyle TimeStyle = Time->GetWidgetStyle();
	TimeStyle.BackgroundImage = Brush(TEXT("Fish_Game/Timer/Timer_Bar_BackGround"));
	TimeStyle.FillImage = Brush(TEXT("Fish_Game/Timer/Timer_Bar"));
	Time->SetWidgetStyle(TimeStyle); Time->SetBorderPadding(FVector2D(5));
	Time->SetFillColorAndOpacity(FLinearColor::White);
	Place(Game, Time, FVector2D(132, 190), FVector2D(660, 29));
	UCanvasPanel* Casting = Panel(TEXT("FishCastPanel"), FVector2D(960, 540), FVector2D(805, 116));
	UTextBlock* CastLabel = Text(Casting, TEXT("CastLabel"), TEXT("우클릭으로 투척!"), FVector2D(160, 3), FVector2D(485, 26), 18);
	CastLabel->SetShadowColorAndOpacity(FLinearColor(1, .94f, .8f, 1)); CastLabel->SetShadowOffset(FVector2D(1, 1));
	Image(Casting, TEXT("CastFrame"), TEXT("Fish_Casting/Fish_Casting_Background"), FVector2D(0, 42), FVector2D(805, 55));
	UCanvasPanel* Track = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FishCastTrack"));
	Place(Casting, Track, FVector2D(10.5, 52.5), FVector2D(784, 34));
	const FName ZoneNames[] = {TEXT("CastGreen"), TEXT("CastYellow"), TEXT("CastRed"), TEXT("CastYellowRight"), TEXT("CastGreenRight")};
	const float Crop[] = {0, 150.f / 784, 325.f / 784, 459.f / 784, 634.f / 784, 1};
	const FVector4f Edges = FPSFishingCastBands().GetEdges();
	const float Boundaries[] = {0, Edges.X, Edges.Y, Edges.Z, Edges.W, 1};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		UBorder* Zone = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), ZoneNames[Index]);
		Zone->SetPadding(FMargin(0));
		FSlateBrush Slice = Brush(TEXT("Fish_Casting/Fish_Casting_Main"));
		Slice.SetUVRegion(FBox2f(FVector2f(Crop[Index], 0), FVector2f(Crop[Index + 1], 1))); Zone->SetBrush(Slice);
		UCanvasPanelSlot* Slot = Track->AddChildToCanvas(Zone);
		Slot->SetAnchors(FAnchors(Boundaries[Index], 0, Boundaries[Index + 1], 1)); Slot->SetOffsets(FMargin(0));
	}
	UImage* Marker = Image(Track, TEXT("CastMarker"), TEXT("Fish_Casting/Fish_Casting_Player"), FVector2D::ZeroVector, FVector2D(28, 74));
	UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot);
	MarkerSlot->SetAnchors(FAnchors(0, .5)); MarkerSlot->SetAlignment(FVector2D(.5));
	UCanvasPanel* Result = Panel(TEXT("FishResultPanel"), FVector2D(24, -24), FVector2D(419, 185));
	UCanvasPanelSlot* ResultSlot = Cast<UCanvasPanelSlot>(Result->GetParent()->Slot);
	ResultSlot->SetAnchors(FAnchors(0, 1)); ResultSlot->SetAlignment(FVector2D(0, 1));
	Image(Result, TEXT("CatchWoodFrame"), TEXT("Fish_Catch/Fish_Catch_Background"), FVector2D::ZeroVector, FVector2D(419, 185));
	UCanvasPanel* Success = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FishResultSuccessContent"));
	Place(Result, Success, FVector2D::ZeroVector, FVector2D(419, 185));
	Image(Success, TEXT("CatchIconPaper"), TEXT("Fish_Catch/Fish_Catch_Icon"), FVector2D(7, 7), FVector2D(168, 170));
	Image(Success, TEXT("CatchInformationPaper"), TEXT("Fish_Catch/Fish_Catch_Information"), FVector2D(180, 18), FVector2D(231, 157));
	Image(Success, TEXT("CatchNameSticker"), TEXT("Fish_Catch/Fish_Catch_Name"), FVector2D(195, 23), FVector2D(198, 72));
	Image(Success, TEXT("CatchFishClip"), TEXT("Fish_Catch/Fish_Catch_Clip"), FVector2D(75, 10), FVector2D(31, 16));
	UScaleBox* FishFit = Tree->ConstructWidget<UScaleBox>(); FishFit->SetStretch(EStretch::ScaleToFit);
	Place(Success, FishFit, FVector2D(26, 31), FVector2D(130));
	UImage* Fish = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ResultIcon")); FishFit->SetContent(Fish);
	Text(Success, TEXT("ResultNameLabel"), TEXT("물고기"), FVector2D(205, 41), FVector2D(178, 32), 20);
	Text(Success, TEXT("ResultSizeLabel"), TEXT("30 cm"), FVector2D(194, 108), FVector2D(200, 28), 18);
	Text(Success, TEXT("CatchCaption"), TEXT("물고기를 낚았어요!"), FVector2D(194, 142), FVector2D(200, 22), 14);
	UCanvasPanel* Failure = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FishResultFailureContent"));
	Failure->SetVisibility(ESlateVisibility::Collapsed);
	Place(Result, Failure, FVector2D::ZeroVector, FVector2D(419, 185));
	Image(Failure, TEXT("CatchFailurePaper"), TEXT("Fish_Game/Fish_Game_Background1"), FVector2D(9, 9), FVector2D(401, 167));
	Text(Failure, TEXT("ResultLabel"), TEXT("물고기를 놓쳤습니다"), FVector2D(20, 70), FVector2D(379, 40), 22);
	UCanvasPanel* Notice = Panel(TEXT("FishNoticePanel"), FVector2D(24, -24), FVector2D(634, 171));
	auto* NoticeSlot = CastChecked<UCanvasPanelSlot>(Notice->GetParent()->Slot);
	NoticeSlot->SetAnchors(FAnchors(0, 1)); NoticeSlot->SetAlignment(FVector2D(0, 1));
	Notice->GetParent()->SetRenderTransformPivot(FVector2D(0, 1)); Notice->GetParent()->SetRenderScale(FVector2D(.9f));
	UCanvasPanel* Cross = Panel(TEXT("FishNoticeCrossPanel"), FVector2D(960, 400), FVector2D(100, 102));
	Image(Cross, TEXT("NoticeCrossBubble"), TEXT("Fish_Ballon/Fish_Ballon_Cant"), FVector2D(0, 0), FVector2D(100, 102));
	Image(Notice, TEXT("NoticeArtwork"), TEXT("Fish_Game/Fish_Game_Background1"), FVector2D(0, 0), FVector2D(634, 171));
	UTextBlock* NoticeLabel = Text(Notice, TEXT("NoticeLabel"), TEXT("물가에서 가까운 물을 향해 사용해 주세요"), FVector2D(46, 42), FVector2D(542, 90), 22);
	NoticeLabel->SetAutoWrapText(true);
	UCanvasPanel* Bite = Panel(TEXT("FishBitePanel"), FVector2D(1510, 400), FVector2D(120, 148));
	Image(Bite, TEXT("BiteBubbleArtwork"), TEXT("Fish_Ballon/Fish_Ballon_Catch"), FVector2D::ZeroVector, FVector2D(120, 123));
	return InitializeDesignerLayout(Blueprint);
#else
	return false;
#endif
}
