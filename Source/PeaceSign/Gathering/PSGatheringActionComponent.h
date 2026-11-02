#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSGatheringTypes.h"
#include "PSGatheringActionComponent.generated.h"

class APSGridWorld;
class APSPlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPSGatheringStarted, bool, bIsChopping, FIntPoint, Cell, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPSGatheringFinished, bool, bSucceeded);

UCLASS(ClassGroup=(Gathering), Blueprintable, meta=(BlueprintSpawnableComponent))
class PEACESIGN_API UPSGatheringActionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPSGatheringActionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	// Returns true for every tree target, including failed actions: never fall through to farming/fishing.
	bool HandleTree(APSGridWorld* Grid, FIntPoint Cell);
	UFUNCTION(BlueprintPure, Category="Gathering") bool IsGatheringActive() const { return bActive; }
	UFUNCTION(BlueprintCallable, Category="Gathering") void Cancel();
	UPROPERTY(BlueprintAssignable, Category="Gathering") FPSGatheringStarted OnStarted;
	UPROPERTY(BlueprintAssignable, Category="Gathering") FPSGatheringFinished OnFinished;
private:
	friend class FPSGatheringTest;
	bool IsContextValid() const;
	void Advance(float DeltaSeconds);
	void Feedback(const FString& Message) const;
	TWeakObjectPtr<APSGridWorld> ActionGrid;
	TWeakObjectPtr<APSPlayerCharacter> ActionPawn;
	FIntPoint Target;
	FPSItemStack Tool;
	FPSAxeDefinition Axe;
	int32 Slot = INDEX_NONE;
	int32 ExpectedHealth = 0;
	float Elapsed = 0;
	bool bChopping = false;
	bool bActive = false;
	bool bCommitted = false;
	bool bSuccess = false;
};
