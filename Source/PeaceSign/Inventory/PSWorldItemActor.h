#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSItemTypes.h"
#include "PSWorldItemActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class PEACESIGN_API APSWorldItemActor : public AActor
{
	GENERATED_BODY()
public:
	APSWorldItemActor();
	void InitializeItem(const FPSItemStack& InItem);

private:
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) FPSItemStack Item;
};
