#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PSUIWidget.generated.h"

class UFont;

/** Shared typography for game UI, including nested Designer widgets and Slate text. */
UCLASS()
class PEACESIGN_API UPSUIWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPSUIWidget(const FObjectInitializer& ObjectInitializer);
	static UFont* GetProjectFont();
	/** Replace only the font family/typeface; retain size, spacing, outline and material. */
	static FSlateFontInfo WithProjectFont(FSlateFontInfo Font);
	static FSlateFontInfo MakeProjectFont(float Size);
	/** Text-only hover hints use the game font; artist-authored tooltip widgets are preserved. */
	UFUNCTION(BlueprintCallable, Category="UI") void SetProjectToolTipText(const FText& Text);
	virtual void SynchronizeProperties() override;
	/** Explicit authoring pass; never rebuilds or repositions the Designer tree. */
	UFUNCTION(BlueprintCallable, Category="UI|Editor") static int32 ApplyDesignerFont(UObject* WidgetBlueprintAsset);
protected:
	virtual void NativePreConstruct() override;
private:
	// A hard reference on the native CDO keeps the imported font and its faces in cooked games.
	UPROPERTY() TObjectPtr<UFont> ProjectFont;
	static int32 ApplyFontToTree(UWidgetTree* Tree, bool bModify);
	void ApplyProjectToolTip();
};
