#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PSPlayerStatusWidget.generated.h"

class UPSPlayerStatsComponent;
class UProgressBar;
class UTextBlock;

UCLASS()
class PEACESIGN_API UPSPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetStatsComponent(UPSPlayerStatsComponent* InStats);
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	UFUNCTION() void RefreshStats();
	UPROPERTY(Transient) TObjectPtr<UPSPlayerStatsComponent> Stats;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> Bars;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Labels;
};
