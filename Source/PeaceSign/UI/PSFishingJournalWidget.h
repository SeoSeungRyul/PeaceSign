#pragma once

#include "CoreMinimal.h"
#include "PSUIWidget.h"
#include "Components/Button.h"
#include "PSFishingJournalWidget.generated.h"

class UImage;
class UTextBlock;
class UUniformGridPanel;
class UCanvasPanel;
class UTexture2D;
class UWidgetTree;
class UPSFishingJournalWidget;

UCLASS()
class PEACESIGN_API UPSFishingJournalEntry : public UButton
{
	GENERATED_BODY()
public:
	void Setup(UPSFishingJournalWidget* Owner, FName Id);
private:
	UFUNCTION() void SelectEntry();
	UPROPERTY(Transient) TObjectPtr<UPSFishingJournalWidget> Journal;
	FName FishId;
};

/** Book layout with an editable Designer tree; only the left list is populated at runtime. */
UCLASS()
class PEACESIGN_API UPSFishingJournalWidget : public UPSUIWidget
{
	GENERATED_BODY()
public:
	UPSFishingJournalWidget(const FObjectInitializer& Initializer);
	UFUNCTION(BlueprintCallable, Category="Fishing|Journal") void RefreshJournal();
	UFUNCTION(BlueprintCallable, Category="Fishing|Journal") void SelectFish(FName Id);
	UFUNCTION(BlueprintCallable, Category="Fishing|Journal") void SelectCategory(int32 Index);
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool ApplyBookDesignerLayout(UObject* Asset);
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool AdjustBookTextAlignment(UObject* Asset);
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool AdjustBookTabs(UObject* Asset);
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool MatchInventoryNotebook(UObject* JournalAsset, UObject* InventoryAsset);
	UFUNCTION(BlueprintCallable, Category="Fishing|Editor") static bool InitializeDesignerLayout(UObject* Asset);
	UPROPERTY(EditDefaultsOnly, Category="Journal|Art") TArray<TObjectPtr<UTexture2D>> CategoryTitleTextures;
	UPROPERTY(EditDefaultsOnly, Category="Journal|Art") TArray<TObjectPtr<UTexture2D>> StarTextures;
	UPROPERTY(EditDefaultsOnly, Category="Journal|Art") TObjectPtr<UTexture2D> EntryNameTexture;
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
private:
	friend class FPSFishingOct26Test;
	friend class FPSBookJournalTest;
	friend struct FPSBookJournalSmoke;
	static void BuildLayout(UWidgetTree* Tree);
	UFUNCTION() void CloseJournal();
	UFUNCTION() void OnJournalChanged(FName Id);
	UFUNCTION() void PreviousPage();
	UFUNCTION() void NextPage();
	UFUNCTION() void InventoryTab();
	UFUNCTION() void FishTab();
	UFUNCTION() void GatheringTab();
	UFUNCTION() void FoodTab();
	UFUNCTION() void ArtifactTab();
	void UpdateCategoryTabs();
	void ClearDetails();
	TArray<FName> GetCategoryIds() const;
	FName SelectedFish;
	int32 Category = 0;
	int32 Page = 0;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UUniformGridPanel> FishList;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> FishName;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> FishDetails;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UImage> FishStar;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UImage> FishImage;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> FishSpecial;
	UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UImage> FishLock;
};
