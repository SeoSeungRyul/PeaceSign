#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PSGameTimeWidget.generated.h"

class UTextBlock;
class UPSGameTimeSubsystem;

UCLASS()
class PEACESIGN_API UPSGameTimeWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION() void RefreshClock();
	// UI
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimeLabel;
	UPROPERTY(Transient) TObjectPtr<UPSGameTimeSubsystem> GameTime;
};
