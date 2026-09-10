#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../PSEquipmentTypes.h"
#include "PSPlayerStatusWidget.generated.h"

class UPSPlayerStatsComponent;
class UProgressBar;
class UTextBlock;
class UBorder;

UCLASS()
class PEACESIGN_API UPSPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetStatsComponent(UPSPlayerStatsComponent* InStats);
	void SetEquipment(EPSEquipment InEquipment);
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void BuildEquipmentPanel();
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EquipmentLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EquipmentHint;
	UPROPERTY(Transient) TObjectPtr<UBorder> EquipmentPanel;
	UFUNCTION() void RefreshStats();
	UPROPERTY(Transient) TObjectPtr<UPSPlayerStatsComponent> Stats;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> Bars;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Labels;
};
