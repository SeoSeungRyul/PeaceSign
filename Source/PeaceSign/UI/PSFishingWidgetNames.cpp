#include "PSFishingWidget.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#endif

bool UPSFishingWidget::AddFishPanelPrefixes(UObject* Asset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
	if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(StaticClass()) || !Blueprint->WidgetTree) return false;
	TArray<FString> Names = {TEXT("MinigamePanel"), TEXT("CastPanel"), TEXT("CastTrack"), TEXT("DirectionRow"),
		TEXT("ResultPanel"), TEXT("ResultSuccessContent"), TEXT("ResultFailureContent"), TEXT("NoticePanel"), TEXT("BitePanel")};
	for (int32 Index = 0; Index < 15; ++Index) Names.Add(FString::Printf(TEXT("DirectionBox%d"), Index));
	// Validate all collisions before renaming anything. Already migrated or absent optional panels are preserved.
	for (const FString& Name : Names)
		if (Blueprint->WidgetTree->FindWidget(*Name) && Blueprint->WidgetTree->FindWidget(*(TEXT("Fish") + Name))) return false;
	for (const FString& Name : Names)
	{
		if (UWidget* Widget = Blueprint->WidgetTree->FindWidget(*Name))
		{
			// Uses the same operation as Designer: retains GUIDs, graph references, bindings and animations.
			if (!FWidgetBlueprintOperationUtils::RenameWidget(Blueprint, Widget, TEXT("Fish") + Name)) return false;
		}
	}
	return true;
#else
	return false;
#endif
}

bool UPSFishingWidget::UpdateFeedbackLayout(UObject* Asset)
{
#if WITH_EDITOR
	auto* Blueprint = Cast<UWidgetBlueprint>(Asset);
	UWidgetTree* Tree = Blueprint ? Blueprint->WidgetTree : nullptr;
	auto* Root = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
	auto* Notice = Tree ? Cast<UBorder>(Tree->FindWidget(TEXT("FishNoticePanel"))) : nullptr;
	UWidget* Result = Tree ? Tree->FindWidget(TEXT("FishResultPanel")) : nullptr;
	auto* CrossPanel = Tree ? Cast<UBorder>(Tree->FindWidget(TEXT("FishNoticeCrossPanel"))) : nullptr;
	UWidget* Cross = Tree ? Tree->FindWidget(TEXT("NoticeCrossBubble")) : nullptr;
	if (!Cross && Tree) Cross = Tree->FindWidget(TEXT("NoticeCross"));
	if (!Root || !Notice || !Result || !Cast<UCanvasPanelSlot>(Notice->Slot)
		|| !Cast<UCanvasPanelSlot>(Result->Slot) || (!CrossPanel && !Cross)) return false;
	Blueprint->Modify(); Tree->Modify();
	const auto BottomLeft = [](UWidget* Widget)
	{
		Widget->Modify(); Widget->Slot->Modify();
		auto* Slot = CastChecked<UCanvasPanelSlot>(Widget->Slot);
		Slot->SetAnchors(FAnchors(0, 1)); Slot->SetAlignment(FVector2D(0, 1));
		Slot->SetPosition(FVector2D(24, -24));
	};
	BottomLeft(Result); BottomLeft(Notice);
	// The supplied banner is wider than the result card. Leave room for the
	// existing bottom-centred hotbar at the project's 1920x1080 design size.
	if (Tree->FindWidget(TEXT("NoticeArtwork")))
	{
		Notice->SetRenderTransformPivot(FVector2D(0, 1));
		Notice->SetRenderScale(FVector2D(.9f));
	}
	if (!CrossPanel)
	{
		// Reparent the supplied X artwork; never rebuild or replace the designer tree.
		Cross->Modify(); Cross->RemoveFromParent();
		CrossPanel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FishNoticeCrossPanel"));
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		CrossPanel->SetBrush(Empty); CrossPanel->SetPadding(FMargin(0));
		CrossPanel->SetContent(Cross);
		auto* Slot = Root->AddChildToCanvas(CrossPanel);
		Slot->SetPosition(FVector2D(960, 400)); Slot->SetSize(FVector2D(100, 102)); Slot->SetAlignment(FVector2D(.5f, 1));
		// Remove the gap that used to reserve space for the X in the same banner.
		if (UWidget* Art = Tree->FindWidget(TEXT("NoticeArtwork")))
			if (auto* ArtSlot = Cast<UCanvasPanelSlot>(Art->Slot))
			{
				const double Top = ArtSlot->GetPosition().Y;
				if (auto* Label = Tree->FindWidget(TEXT("NoticeLabel")))
					if (auto* LabelSlot = Cast<UCanvasPanelSlot>(Label->Slot)) LabelSlot->SetPosition(LabelSlot->GetPosition() - FVector2D(0, Top));
				ArtSlot->SetPosition(ArtSlot->GetPosition() - FVector2D(0, Top));
				auto* NoticeSlot = CastChecked<UCanvasPanelSlot>(Notice->Slot);
				NoticeSlot->SetSize(FVector2D(NoticeSlot->GetSize().X, FMath::Max(1., NoticeSlot->GetSize().Y - Top)));
			}
	}
	if (auto* Defaults = Blueprint->GeneratedClass ? Cast<UPSFishingWidget>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr)
	{
		Defaults->Modify(); Defaults->NoticeOffset = FVector2D(0, -120);
	}
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
