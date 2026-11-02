#include "PSFishingWaterRegion.h"
#include "Components/BoxComponent.h"
#include "../World/PSGridWorld.h"

APSFishingWaterRegion::APSFishingWaterRegion()
{
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterRegion"));
	SetRootComponent(Bounds);
	Bounds->SetBoxExtent(FVector(400, 400, 1000));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
}

bool APSFishingWaterRegion::ContainsWaterPoint(const FVector& Point, const APSGridWorld* Grid) const
{
	if (TargetGrid && TargetGrid != Grid) return false;
	const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(Point);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y;
}
