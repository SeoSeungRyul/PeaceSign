#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSFishingTypes.h"
#include "PSFishingWaterRegion.generated.h"

class UBoxComponent;
class APSGridWorld;

/** Author a sea, river or lake area without changing its terrain or collision. */
UCLASS()
class PEACESIGN_API APSFishingWaterRegion : public AActor
{
	GENERATED_BODY()
public:
	APSFishingWaterRegion();
	bool ContainsWaterPoint(const FVector& Point, const APSGridWorld* Grid) const;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Bounds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fishing") EPSFishingLocation Location = EPSFishingLocation::Lake;
	/** Higher priority wins. Equal priority is resolved by actor path for stable results. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fishing") int32 Priority = 0;
	/** Optional: restrict this region to one grid when multiple worlds share a level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fishing") TObjectPtr<APSGridWorld> TargetGrid;
};
