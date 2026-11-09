#include "PSFishingWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "../PSPlayerController.h"
#include "UObject/ConstructorHelpers.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

const FSlateBrush& FPSFishingArrowBrushes::Get(const EPSFishingDirection Direction) const
{
	switch (Direction)
	{
	case EPSFishingDirection::Left: return Left;
	case EPSFishingDirection::Down: return Down;
	case EPSFishingDirection::Right: return Right;
	default: return Up;
	}
}

UPSFishingWidget::UPSFishingWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PendingSlotBrush = FSlateRoundedBoxBrush(FLinearColor(1, .91f, .72f), 14, FLinearColor(.69f, .43f, .25f), 4);
	CurrentSlotBrush = FSlateRoundedBoxBrush(FLinearColor(1, .79f, .24f), 14, FLinearColor(.91f, .31f, .16f), 4);
	CompleteSlotBrush = FSlateRoundedBoxBrush(FLinearColor(.35f, .88f, .73f), 14, FLinearColor(.05f, .43f, .35f), 4);
	WrongSlotBrush = FSlateRoundedBoxBrush(FLinearColor(.85f, .42f, .37f), 14, FLinearColor(.65f, .2f, .15f), 4);
	CurrentDirectionMarkerBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	CastMarkerBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	const auto Arrow = [](const TCHAR* Path)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path);
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Texture);
		Brush.SetImageSize(FVector2D(52, 52));
		return Brush;
	};
	UpArrowBrush = Arrow(TEXT("/Game/UI/Fishing/Fish_Game/Arrow/Arrow_Normal/Arrow_T.Arrow_T"));
	LeftArrowBrush = Arrow(TEXT("/Game/UI/Fishing/Fish_Game/Arrow/Arrow_Normal/Arrow_L.Arrow_L"));
	DownArrowBrush = Arrow(TEXT("/Game/UI/Fishing/Fish_Game/Arrow/Arrow_Normal/Arrow_B.Arrow_B"));
	RightArrowBrush = Arrow(TEXT("/Game/UI/Fishing/Fish_Game/Arrow/Arrow_Normal/Arrow_R.Arrow_R"));
}

void UPSFishingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	UpdateVisibility();
}

bool UPSFishingWidget::ConnectDesignerLayout()
{
	if (!WidgetTree) return false;
	Panel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("FishMinigamePanel")));
	FishCastPanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("FishCastPanel")));
	FishResultPanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("FishResultPanel")));
	FishNoticePanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("FishNoticePanel")));
	FishNoticeCrossPanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("FishNoticeCrossPanel")));
	FishBitePanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("FishBitePanel")));
	StateLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("StateLabel")));
	ResultLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ResultLabel")));
	ResultNameLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ResultNameLabel")));
	ResultSizeLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ResultSizeLabel")));
	NoticeLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("NoticeLabel")));
	NoticeArtwork = Cast<UImage>(WidgetTree->FindWidget(TEXT("NoticeArtwork")));
	FishResultSuccessContent = WidgetTree->FindWidget(TEXT("FishResultSuccessContent"));
	FishResultFailureContent = WidgetTree->FindWidget(TEXT("FishResultFailureContent"));
	CastLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CastLabel")));
	ResultIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("ResultIcon")));
	TimeBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("TimeBar")));
	FishDirectionRow = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("FishDirectionRow")));
	FishCastTrack = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("FishCastTrack")));
	CastMarker = WidgetTree->FindWidget(TEXT("CastMarker"));
	CastGreen = Cast<UBorder>(WidgetTree->FindWidget(TEXT("CastGreen")));
	CastYellow = Cast<UBorder>(WidgetTree->FindWidget(TEXT("CastYellow")));
	CastRed = Cast<UBorder>(WidgetTree->FindWidget(TEXT("CastRed")));
	CastYellowRight = Cast<UBorder>(WidgetTree->FindWidget(TEXT("CastYellowRight")));
	CastGreenRight = Cast<UBorder>(WidgetTree->FindWidget(TEXT("CastGreenRight")));
	DirectionBoxes.Reset(); DirectionImages.Reset(); DirectionLabels.Reset(); DirectionMarkers.Reset();
	for (int32 Index = 0; Index < FMath::Clamp(VisibleDirectionSlots, 1, 15); ++Index)
	{
		DirectionBoxes.Add(Cast<UBorder>(WidgetTree->FindWidget(*FString::Printf(TEXT("FishDirectionBox%d"), Index))));
		DirectionImages.Add(Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("DirectionImage%d"), Index))));
		DirectionLabels.Add(Cast<UTextBlock>(WidgetTree->FindWidget(*FString::Printf(TEXT("DirectionLabel%d"), Index))));
		DirectionMarkers.Add(Cast<UImage>(WidgetTree->FindWidget(*FString::Printf(TEXT("DirectionMarker%d"), Index))));
	}
	DirectionHasAnswer.Init(false, DirectionImages.Num());
	DirectionDone.Init(false, DirectionImages.Num());
	return Panel && FishCastPanel && FishResultPanel && StateLabel && TimeBar;
}

void UPSFishingWidget::BuildWidgetTree()
{
	if (!WidgetTree || Panel) return;
	// Preserve the designer's tree, including any intentionally removed optional controls.
	if (!WidgetTree->RootWidget) BuildDefaultLayout(WidgetTree);
	ConnectDesignerLayout();
}

bool UPSFishingWidget::InitializeDesignerLayout(UObject* WidgetBlueprintAsset)
{
#if WITH_EDITOR
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprintAsset);
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(StaticClass()) || !Blueprint->WidgetTree) return false;
	bool bChanged = false;
	if (!Blueprint->WidgetTree->RootWidget)
	{
		BuildDefaultLayout(Blueprint->WidgetTree);
		bChanged = true;
	}
	else if (!Blueprint->WidgetTree->FindWidget(TEXT("FishBitePanel")))
	{
		AddBitePanel(Blueprint->WidgetTree);
		bChanged = true;
	}
	// Programmatic additions need stable editor IDs before any compilation (UE 5.8).
	Blueprint->WidgetTree->ForEachWidget([Blueprint, &bChanged](UWidget* Widget)
	{
		if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
		{
			Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
			bChanged = true;
		}
	});
	if (bChanged) FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
#else
	return false;
#endif
}

bool UPSFishingWidget::RebuildPrototypeDesignerLayout(UObject* WidgetBlueprintAsset)
{
#if WITH_EDITOR
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprintAsset);
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(StaticClass())) return false;
	Blueprint->Modify();
	Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, NAME_None, RF_Transactional);
	Blueprint->WidgetVariableNameToGuidMap.Reset();
	BuildDefaultLayout(Blueprint->WidgetTree);
	return InitializeDesignerLayout(Blueprint);
#else
	return false;
#endif
}

void UPSFishingWidget::UpdateVisibility()
{
	const bool bCast = DisplayState == EPSFishingState::CastWindup || DisplayState == EPSFishingState::CastTiming;
	const bool bBite = DisplayState == EPSFishingState::BiteWindow;
	const bool bGame = (bBite && !FishBitePanel) || DisplayState == EPSFishingState::Minigame;
	const bool bResult = DisplayState == EPSFishingState::Success || DisplayState == EPSFishingState::Failure;
	if (Panel) Panel->SetVisibility(bGame ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (FishCastPanel) FishCastPanel->SetVisibility(bCast ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (FishResultPanel) FishResultPanel->SetVisibility(bResult ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (FishNoticePanel) FishNoticePanel->SetVisibility(NoticeRemaining > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (FishNoticeCrossPanel) FishNoticeCrossPanel->SetVisibility(NoticeRemaining > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (FishBitePanel) FishBitePanel->SetVisibility(bBite ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (TimeBar) TimeBar->SetVisibility(DisplayState == EPSFishingState::Minigame ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetVisibility(bCast || bGame || bBite || bResult || NoticeRemaining > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

FVector2D UPSFishingWidget::ClampPanelPosition(const FVector2D Position, const FVector2D Size,
	const FVector2D Alignment, const FVector2D ViewportSize)
{
	FVector2D Result = Position;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const double Min = 8 + Size[Axis] * Alignment[Axis];
		const double Max = ViewportSize[Axis] - 8 - Size[Axis] * (1 - Alignment[Axis]);
		Result[Axis] = Max >= Min ? FMath::Clamp(Position[Axis], Min, Max)
			: ViewportSize[Axis] * .5 + Size[Axis] * (Alignment[Axis] - .5);
	}
	return Result;
}

void UPSFishingWidget::UpdatePlayerPosition()
{
	APlayerController* Controller = GetOwningPlayer();
	if (!bFollowPlayer || !Controller || !Controller->GetPawn()) return;
	int32 Width = 0, Height = 0;
	Controller->GetViewportSize(Width, Height);
	const float Scale = FMath::Max(.01f, UWidgetLayoutLibrary::GetViewportScale(Controller));
	const FVector2D ViewportSize(Width / Scale, Height / Scale);
	const auto Move = [Controller, ViewportSize](UBorder* Border, const FVector& WorldLocation, const FVector2D Offset)
	{
		if (!Border) return;
		FVector2D Position;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, WorldLocation + FVector(0, 0, 70), Position, true)) return;
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Border->Slot))
		{
			FVector2D PanelPosition = Position + Offset;
			if (ViewportSize.X > 0 && ViewportSize.Y > 0)
			{
				const FVector2D Size = CanvasSlot->GetAutoSize() ? Border->GetDesiredSize() : CanvasSlot->GetSize();
				PanelPosition = ClampPanelPosition(PanelPosition, Size, CanvasSlot->GetAlignment(), ViewportSize);
			}
			CanvasSlot->SetPosition(PanelPosition);
		}
	};
	const FVector PlayerLocation = Controller->GetPawn()->GetActorLocation();
	FVector FishingLocation = PlayerLocation;
	if (const APSPlayerController* FishingController = Cast<APSPlayerController>(Controller))
	{
		FVector FocusLocation;
		if (FishingController->GetFishingFocusLocation(FocusLocation)) FishingLocation = FocusLocation;
	}
	Move(Panel, FishingLocation, MinigameOffset);
	Move(FishBitePanel, FishingLocation, BiteOffset);
	Move(FishCastPanel, PlayerLocation, CastingOffset);
	Move(FishNoticeCrossPanel, PlayerLocation, NoticeOffset);
}

void UPSFishingWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetOwningPlayer() && GetOwningPlayer()->IsPaused()) return;
	NoticeRemaining = FMath::Max(0.0f, NoticeRemaining - InDeltaTime);
	if (WrongInputRemaining > 0)
	{
		WrongInputRemaining = FMath::Max(0.f, WrongInputRemaining - InDeltaTime);
		if (WrongInputRemaining == 0) UpdateDirectionBoxStyles();
	}
	UpdatePlayerPosition();
	UpdateVisibility();
}

void UPSFishingWidget::ShowNotice(const FText& Reason, const EPSFishingNoticeType Type)
{
	BuildWidgetTree();
	// Keep lettering and decorations supplied as part of the original UI images.
	const FSlateBrush& Brush = Type == EPSFishingNoticeType::RodRequired ? RodNoticeBrush
		: Type == EPSFishingNoticeType::WaterRequired ? WaterNoticeBrush : GeneralNoticeBrush;
	const bool bBakedText = NoticeArtwork && Brush.GetResourceObject() && Type != EPSFishingNoticeType::General;
	if (NoticeArtwork && Brush.GetResourceObject()) NoticeArtwork->SetBrush(Brush);
	if (NoticeLabel)
	{
		NoticeLabel->SetText(Reason);
		NoticeLabel->SetFont(WithProjectFont(NoticeLabel->GetFont()));
		NoticeLabel->SetVisibility(bBakedText ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	NoticeRemaining = 2;
	UpdateVisibility();
	UpdatePlayerPosition();
}

int32 UPSFishingWidget::GetCurrentDirectionSlot() const
{
	return FMath::Clamp(CurrentDirectionSlot, 0, FMath::Max(0, DirectionImages.Num() - 1));
}

void UPSFishingWidget::ShowWrongInput()
{
	if (DisplayState != EPSFishingState::Minigame) return;
	WrongInputRemaining = FMath::Max(0.f, WrongInputFlashDuration);
	UpdateDirectionBoxStyles();
}

void UPSFishingWidget::UpdateDirectionBoxStyles()
{
	for (int32 Index = 0; Index < DirectionBoxes.Num(); ++Index)
	{
		if (!DirectionBoxes[Index]) continue;
		const bool bCurrent = DirectionHasAnswer[Index] && Index == GetCurrentDirectionSlot();
		DirectionBoxes[Index]->SetBrush(DirectionDone[Index] ? CompleteSlotBrush
			: bCurrent ? (WrongInputRemaining > 0 ? WrongSlotBrush : CurrentSlotBrush) : PendingSlotBrush);
	}
}

void UPSFishingWidget::Refresh(const EPSFishingState State, const float TimeRemaining, const float StateDuration,
	const TArray<EPSFishingDirection>& Sequence, const int32 SequenceIndex,
	const FText& FishName, const int32 FishSizeCm, UTexture2D* FishIcon, const float ResultOpacity,
	const float CastPosition, const float GreenEnd, const float YellowEnd, const FPSFishingCastBands& Bands)
{
	BuildWidgetTree();
	// New fishing activity replaces an earlier unavailable-action notification.
	if (State != EPSFishingState::Idle) NoticeRemaining = 0;
	if (State != EPSFishingState::Minigame || DisplayState != State || DisplayedSequenceIndex != SequenceIndex) WrongInputRemaining = 0;
	DisplayedSequenceIndex = SequenceIndex;
	DisplayState = State;
	UpdateVisibility();
	UpdatePlayerPosition();
	if (TimeBar) TimeBar->SetPercent(StateDuration > 0 ? FMath::Clamp(TimeRemaining / StateDuration, 0.f, 1.f) : 0);
	if (FishDirectionRow) FishDirectionRow->SetVisibility(State == EPSFishingState::Minigame ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (FishResultPanel) FishResultPanel->SetRenderOpacity(FMath::Clamp(ResultOpacity, 0.f, 1.f));
	if (ResultIcon)
	{
		ResultIcon->SetVisibility(State == EPSFishingState::Success && FishIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (FishIcon) ResultIcon->SetBrushFromTexture(FishIcon, true);
	}
	const bool bSuccess = State == EPSFishingState::Success;
	if (FishResultSuccessContent) FishResultSuccessContent->SetVisibility(bSuccess ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (FishResultFailureContent) FishResultFailureContent->SetVisibility(bSuccess ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	const bool bSplitResult = ResultNameLabel && ResultSizeLabel;
	if (ResultNameLabel)
	{
		ResultNameLabel->SetText(bSuccess ? FishName : FText::GetEmpty());
		ResultNameLabel->SetVisibility(bSuccess && bSplitResult ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ResultSizeLabel)
	{
		ResultSizeLabel->SetText(bSuccess ? FText::Format(NSLOCTEXT("Fishing", "Size", "{0} cm"), FText::AsNumber(FishSizeCm)) : FText::GetEmpty());
		ResultSizeLabel->SetVisibility(bSuccess && bSplitResult ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ResultLabel)
	{
		ResultLabel->SetText(bSuccess
			? FText::Format(NSLOCTEXT("Fishing", "Success", "{0}  {1}cm 획득!"), FishName, FText::AsNumber(FishSizeCm))
			: NSLOCTEXT("Fishing", "Failure", "물고기를 놓쳤습니다"));
		ResultLabel->SetVisibility(bSuccess && bSplitResult ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (StateLabel)
	{
		if (State == EPSFishingState::WaitingForBite) StateLabel->SetText(NSLOCTEXT("Fishing", "Waiting", "입질을 기다리는 중  F / 우클릭 취소"));
		if (State == EPSFishingState::BiteWindow) StateLabel->SetText(NSLOCTEXT("Fishing", "Bite", "! 입질 !"));
		if (State == EPSFishingState::Minigame) StateLabel->SetText(NSLOCTEXT("Fishing", "Minigame", "노란 칸의 화살표 방향을 입력하세요"));
	}
	if (CastLabel) CastLabel->SetText(State == EPSFishingState::CastWindup
		? NSLOCTEXT("Fishing", "Windup", "던질 준비 중...") : NSLOCTEXT("Fishing", "Timing", "우클릭으로 투척!"));
	const auto Zone = [](UBorder* Border, const float Left, const float Right)
	{
		if (Border) if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Border->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(Left, 0, Right, 1));
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetOffsets(FMargin(0));
		}
	};
	const FVector4f Edges = GreenEnd >= 0 && YellowEnd >= 0
		? PSFishing::GetCastBandEdges(GreenEnd, YellowEnd) : Bands.GetEdges();
	Zone(CastGreen, 0, Edges.X); Zone(CastYellow, Edges.X, Edges.Y); Zone(CastRed, Edges.Y, Edges.Z);
	Zone(CastYellowRight, Edges.Z, Edges.W); Zone(CastGreenRight, Edges.W, 1);
	if (CastMarker)
	{
		if (UImage* Image = Cast<UImage>(CastMarker); Image && CastMarkerBrush.DrawAs != ESlateBrushDrawType::NoDrawType)
			Image->SetBrush(CastMarkerBrush);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CastMarker->Slot))
		{
			const float X = FMath::Clamp(FMath::IsFinite(CastPosition) ? CastPosition : 0.f, 0.f, 1.f);
			CanvasSlot->SetAnchors(FAnchors(X, .5f));
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
			CanvasSlot->SetAlignment(FVector2D(.5f, .5f));
		}
	}
	const FSlateBrush* Arrows[] = {&UpArrowBrush, &LeftArrowBrush, &DownArrowBrush, &RightArrowBrush};
	for (int32 Index = 0; Index < DirectionImages.Num(); ++Index)
	{
		const int32 SequenceSlot = SequenceIndex + Index - GetCurrentDirectionSlot();
		const bool bHas = Sequence.IsValidIndex(SequenceSlot);
		const bool bDone = bHas && SequenceSlot < SequenceIndex;
		const bool bCurrent = bHas && Index == GetCurrentDirectionSlot();
		DirectionHasAnswer[Index] = bHas;
		DirectionDone[Index] = bDone;
		if (DirectionImages[Index])
		{
			DirectionImages[Index]->SetVisibility(bHas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bHas)
			{
				const EPSFishingDirection Direction = Sequence[SequenceSlot];
				const FSlateBrush& StateBrush = (bDone ? CompleteArrowBrushes : CurrentArrowBrushes).Get(Direction);
				FSlateBrush Brush = (bDone || bCurrent) && StateBrush.GetResourceObject()
					? StateBrush : *Arrows[FMath::Clamp(static_cast<int32>(Direction), 0, 3)];
				Brush.SetUVRegion(FBox2f(FVector2f(0), FVector2f(1)));
				DirectionImages[Index]->SetBrush(Brush);
			}
		}
		if (DirectionLabels[Index])
		{
			DirectionLabels[Index]->SetText(FText::GetEmpty());
			DirectionLabels[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (DirectionMarkers[Index])
		{
			if (CurrentDirectionMarkerBrush.DrawAs != ESlateBrushDrawType::NoDrawType) DirectionMarkers[Index]->SetBrush(CurrentDirectionMarkerBrush);
			DirectionMarkers[Index]->SetVisibility(bCurrent ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	UpdateDirectionBoxStyles();
}
