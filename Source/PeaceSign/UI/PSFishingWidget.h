#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Fishing/PSFishingTypes.h"
#include "PSFishingWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

UCLASS()
class PEACESIGN_API UPSFishingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Refresh(EPSFishingState State, float TimeRemaining, float StateDuration,
		const TArray<EPSFishingDirection>& Sequence, int32 SequenceIndex,
		const FText& FishName, int32 FishSizeCm, UTexture2D* FishIcon = nullptr, float ResultOpacity = 1.0f);

protected:
	virtual void NativeOnInitialized() override;

private:
	friend class FPSFishingTest;
	void BuildWidgetTree();
	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StateLabel;
	UPROPERTY(Transient) TObjectPtr<UImage> ResultIcon;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> TimeBar;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> DirectionRow;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> DirectionBoxes;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> DirectionImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> DirectionLabels;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> DirectionAtlas;
};
