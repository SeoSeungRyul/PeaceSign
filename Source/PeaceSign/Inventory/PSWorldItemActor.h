#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSItemTypes.h"
#include "PSWorldItemActor.generated.h"

class UStaticMeshComponent;
class APSGridWorld;
class UTexture2D;
class UMaterialInterface;
class UPSGatheringWorldComponent;

UCLASS()
class PEACESIGN_API APSWorldItemActor : public AActor
{
	GENERATED_BODY()
public:
	APSWorldItemActor();
	void InitializeItem(const FPSItemStack& InItem, APSGridWorld* Grid = nullptr);
	void InitializeGatheringItem(const FPSItemStack& InItem, UPSGatheringWorldComponent* World, FGuid Id,
		UTexture2D* Icon, float CellSize, bool bPlayHop);
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ItemIcon;
	UPROPERTY(VisibleAnywhere) FPSItemStack Item;
	UPROPERTY() TObjectPtr<UMaterialInterface> GatheringMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> IconMaterial;
	TWeakObjectPtr<UPSGatheringWorldComponent> GatheringWorld;
	TWeakObjectPtr<APSGridWorld> PickupGrid;
	FGuid GatheringDropId;
	float GatheringCellSize = 0;
	float HopElapsed = 0;
	bool bLanding = false;
	void UpdateIconPosition(float HopHeight);
	void TryCollect(AActor* OtherActor);
};
