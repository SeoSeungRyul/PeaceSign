#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSBushInteractionComponent.generated.h"

class APSGridWorld;
class APSPlayerCharacter;
class UPSBushPromptWidget;

UCLASS()
class PEACESIGN_API UPSBushInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPSBushInteractionComponent();
	virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	bool Press(APSGridWorld* Grid, bool bRightClick, TOptional<FIntPoint> Cell = {});
	void Release(bool bRightClick);
	void Cancel();
	bool IsActive() const { return bActive; }
	void Advance(float Delta);
private:
	APSPlayerCharacter* GetUsablePlayer() const;
	void UpdatePrompt();
	TWeakObjectPtr<APSGridWorld> ActionGrid;
	TWeakObjectPtr<APSPlayerCharacter> ActionPlayer;
	FIntPoint Target = FIntPoint::ZeroValue;
	FVector StartPosition = FVector::ZeroVector;
	float StartHealth = 0;
	float Elapsed = 0;
	int32 StartSlot = INDEX_NONE;
	int32 StartSequence = 0;
	bool bActive = false;
	bool bUsingRightClick = false;
	UPROPERTY() TObjectPtr<UPSBushPromptWidget> Prompt;
};
