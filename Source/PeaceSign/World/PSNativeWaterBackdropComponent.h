#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSNativeWaterBackdropComponent.generated.h"
class UTextureCube;
class USkyLightComponent;

/** Render-only suppression of the engine template's debug floor. Never changes collision. */
UCLASS()
class PEACESIGN_API UPSNativeWaterBackdropComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	void Update(bool bNativeWater);
	void UpdateReflections(bool bEnabled, UTextureCube* Cubemap);
	virtual void OnUnregister() override;
private:
	struct FHiddenFloor { TWeakObjectPtr<AActor> Actor; bool bGameHidden=false; bool bEditorHidden=false; };
	TArray<FHiddenFloor> Floors;
	struct FSkyState { TWeakObjectPtr<USkyLightComponent> Sky; uint8 SourceType=0; };
	TArray<FSkyState> Skies;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextureCube>> OriginalSkyCubemaps;
	void Restore();
	void RestoreReflections();
};
