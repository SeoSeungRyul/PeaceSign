#pragma once

#include "CoreMinimal.h"
#include "PSUIWidget.h"
#include "../Fishing/PSFishingTypes.h"
#include "PSFishingWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UCanvasPanel;
class UWidgetTree;

USTRUCT(BlueprintType)
struct FPSFishingArrowBrushes
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FSlateBrush Up;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FSlateBrush Left;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FSlateBrush Down;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FSlateBrush Right;
	const FSlateBrush& Get(EPSFishingDirection Direction) const;
};

UCLASS()
class PEACESIGN_API UPSFishingWidget : public UPSUIWidget
{
	GENERATED_BODY()
public:
	UPSFishingWidget(const FObjectInitializer& ObjectInitializer);
	void Refresh(EPSFishingState State, float TimeRemaining, float StateDuration,
		const TArray<EPSFishingDirection>& Sequence, int32 SequenceIndex,
		const FText& FishName, int32 FishSizeCm, UTexture2D* FishIcon = nullptr, float ResultOpacity = 1.0f,
		float CastPosition = 0.0f, float GreenEnd = -1, float YellowEnd = -1,
		const FPSFishingCastBands& Bands = FPSFishingCastBands());
	void ShowNotice(const FText& Reason, EPSFishingNoticeType Type = EPSFishingNoticeType::General);
	void ShowWrongInput();
	/** Seeds an empty Widget Blueprint once. Existing designer edits are preserved. */
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool InitializeDesignerLayout(UObject* WidgetBlueprintAsset);
	/** Explicit editor-only reset of the prototype. Never called during normal initialization. */
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool RebuildPrototypeDesignerLayout(UObject* WidgetBlueprintAsset);
	/** One-time authoring of the imported artwork; subsequent Designer edits are preserved. */
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool ApplyArtworkDesignerLayout(UObject* WidgetBlueprintAsset);
	/** Targeted layout migration. User correction: seven slots with the fourth slot current. */
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool UpgradeOctober26Layout(UObject* WidgetBlueprintAsset);
	/** Rename existing panel widgets in place, retaining layout, bindings and animations. */
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool AddFishPanelPrefixes(UObject* WidgetBlueprintAsset);
	/** Move feedback to the bottom left and split the overhead X, preserving the existing art. */
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool UpdateFeedbackLayout(UObject* WidgetBlueprintAsset);
	/** Remove only the legacy bite input hint, retaining the supplied exclamation artwork. */
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool RemoveBiteInputHint(UObject* WidgetBlueprintAsset);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Layout") bool bFollowPlayer = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Layout") FVector2D MinigameOffset = FVector2D(0, -100);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Layout") FVector2D CastingOffset = FVector2D(100, -100);
	/** Screen offset of the overhead X. The notice banner is anchored in Designer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Layout") FVector2D NoticeOffset = FVector2D(0, -120);
	/** Exclamation bubble sits immediately above the bobber, independently of error notices. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Layout") FVector2D BiteOffset = FVector2D(0, -24);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Layout", meta=(ClampMin="1", ClampMax="15")) int32 VisibleDirectionSlots = 7;
	/** Zero-based: 3 is the fourth visible slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Layout", meta=(ClampMin="0", ClampMax="14")) int32 CurrentDirectionSlot = 3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art", meta=(ClampMin="0")) float WrongInputFlashDuration = .2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush PendingSlotBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush CurrentSlotBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush CompleteSlotBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush WrongSlotBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush UpArrowBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush LeftArrowBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush DownArrowBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush RightArrowBrush;
	/** Unassigned state images fall back to the four normal arrow brushes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FPSFishingArrowBrushes CurrentArrowBrushes;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FPSFishingArrowBrushes CompleteArrowBrushes;
	/** Optional overrides; leave unassigned to use the images authored in Designer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush CurrentDirectionMarkerBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush CastMarkerBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush GeneralNoticeBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush RodNoticeBrush;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fishing|Art") FSlateBrush WaterNoticeBrush;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	friend class FPSFishingTest;
	void BuildWidgetTree();
	bool ConnectDesignerLayout();
	static void BuildDefaultLayout(UWidgetTree* Tree);
	static void AddBitePanel(UWidgetTree* Tree);
	void UpdatePlayerPosition();
	void UpdateVisibility();
	void UpdateDirectionBoxStyles();
	int32 GetCurrentDirectionSlot() const;
	static FVector2D ClampPanelPosition(FVector2D Position, FVector2D Size, FVector2D Alignment, FVector2D ViewportSize);
	EPSFishingState DisplayState = EPSFishingState::Idle;
	float NoticeRemaining = 0;
	float WrongInputRemaining = 0;
	int32 DisplayedSequenceIndex = INDEX_NONE;
	TArray<bool> DirectionHasAnswer;
	TArray<bool> DirectionDone;
	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StateLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UImage> ResultIcon;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> TimeBar;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UHorizontalBox> FishDirectionRow;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> DirectionBoxes;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> DirectionImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> DirectionLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> DirectionMarkers;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> FishCastPanel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> FishResultPanel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> FishNoticePanel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> FishNoticeCrossPanel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> FishBitePanel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ResultLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ResultNameLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ResultSizeLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CastLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> NoticeLabel;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UImage> NoticeArtwork;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UWidget> FishResultSuccessContent;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UWidget> FishResultFailureContent;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UCanvasPanel> FishCastTrack;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UWidget> CastMarker;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> CastGreen;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> CastYellow;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> CastRed;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> CastYellowRight;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> CastGreenRight;
};
