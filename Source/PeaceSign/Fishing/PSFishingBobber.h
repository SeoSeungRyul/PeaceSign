#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSFishingBobber.generated.h"

class UStaticMeshComponent;
class UBillboardComponent;
class UTexture2D;

/** Collision-free replaceable fishing visual; updated by the paused-aware fishing state machine. */
UCLASS(Blueprintable)
class PEACESIGN_API APSFishingBobber : public AActor
{
	GENERATED_BODY()
public:
	APSFishingBobber();
	void UpdateVisual(const FVector& RodTip, const FVector& Landing, bool bBiting, float WaterElapsed, float RetrieveAlpha = 0, float FlightAlpha = 1,
		const FVector& ArcUp = FVector::UpVector);
	void ShowCatch(UTexture2D* Texture);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> FloatMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LineMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBillboardComponent> CatchImage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fishing") float BiteAmplitude = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fishing") float BiteFrequency = 9;
	/** Floating distance to either side of the landing point along screen-up, in world units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fishing|Float", meta=(ClampMin="0")) float FloatAmplitude = 6.f;
	/** Seconds per cycle of the idle float motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fishing|Float", meta=(ClampMin="0.1")) float FloatPeriod = 1.8f;
	/** Minimum arc height in the view plane, enlarged for casts along the view's up axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fishing", meta=(ClampMin="0")) float CastArcHeight = 140;
	/** Longest side of the catch image in world units, independent of texture resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fishing", meta=(ClampMin="1")) float CatchImageWorldSize = 64;
};
