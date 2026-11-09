#pragma once

#include "PSUIWidget.h"
#include "PSBushPromptWidget.generated.h"

UCLASS()
class PEACESIGN_API UPSBushPromptWidget : public UPSUIWidget
{
	GENERATED_BODY()
public:
	void ShowPrompt(const FString& Message, float Progress);
protected:
	virtual void NativeOnInitialized() override;
private:
	UPROPERTY() TObjectPtr<class UTextBlock> Label;
	UPROPERTY() TObjectPtr<class UProgressBar> Gauge;
};
