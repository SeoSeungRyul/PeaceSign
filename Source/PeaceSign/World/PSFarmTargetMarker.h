#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSFarmTargetMarker.generated.h"

/** A collision-free, unlit border; remains visible in packaged builds. */
UCLASS(NotBlueprintable, Transient)
class PEACESIGN_API APSFarmTargetMarker : public AActor
{
	GENERATED_BODY()
public:
	APSFarmTargetMarker();
	void ShowTarget(FVector Center, float CellSize, bool bUsable);
private:
	UPROPERTY() TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Border;
	UPROPERTY() TObjectPtr<class UMaterialInterface> Material;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> DynamicMaterial;
	UPROPERTY() TObjectPtr<class UTexture2D> Green;
	UPROPERTY() TObjectPtr<class UTexture2D> Red;
	float CachedCellSize = -1;
};
