#pragma once
#include "PSTileChunkActor.h"
#include "PSLinkWaterComparison.generated.h"

class UChildActorComponent;
class UWaterWavesAsset;

/** Isolated 5x5 comparison pond. Does not edit GridWorld, saves, or gameplay. */
UCLASS()
class PEACESIGN_API APSLinkWaterComparison : public APSTileChunkActor
{
	GENERATED_BODY()
public:
	APSLinkWaterComparison();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Water Comparison") void RefreshComparison();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Comparison") FString PreviewStatus;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Comparison") bool bUseLinkWater = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Comparison") TObjectPtr<UWaterWavesAsset> Waves;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Comparison") TObjectPtr<UMaterialInterface> NativeWaterMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Comparison") TObjectPtr<UChildActorComponent> NativeWater;
private:
	UPROPERTY() TObjectPtr<UMaterialInterface> LegacyWaterMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> ShoreMaterial;
	UPROPERTY() TObjectPtr<UStaticMesh> WaterMesh;
};
