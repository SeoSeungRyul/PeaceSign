#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSGatheringTypes.h"
#include "PSGatheringTreeActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class PEACESIGN_API APSGatheringTreeActor : public AActor
{
	GENERATED_BODY()
public:
	APSGatheringTreeActor();
	virtual void Tick(float DeltaSeconds) override;
	void Configure(const FPSTreeRecord& Tree, float CellSize);
	void React(bool bFalling);
private:
	UPROPERTY() TObjectPtr<USceneComponent> Pivot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Trunk;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Crown;
	float AnimationTime = 0;
	bool bFall = false;
	bool bStump = false;
	float Size = 100;
};
