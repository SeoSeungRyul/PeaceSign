#include "PSNativeWaterBackdropComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Components/SkyLightComponent.h"

void UPSNativeWaterBackdropComponent::Restore()
{
	for(const auto& State:Floors) if(auto* Floor=State.Actor.Get())
	{
		Floor->SetActorHiddenInGame(State.bGameHidden);
#if WITH_EDITOR
		Floor->SetIsTemporarilyHiddenInEditor(State.bEditorHidden);
#endif
	}
	Floors.Reset();
}

void UPSNativeWaterBackdropComponent::OnUnregister()
{
	Restore(); RestoreReflections(); Super::OnUnregister();
}

void UPSNativeWaterBackdropComponent::RestoreReflections()
{
	for(int32 I=0;I<Skies.Num();++I) if(auto* Sky=Skies[I].Sky.Get())
	{
		Sky->SourceType=static_cast<ESkyLightSourceType>(Skies[I].SourceType);
		Sky->SetCubemap(OriginalSkyCubemaps[I]); Sky->RecaptureSky();
	}
	Skies.Reset(); OriginalSkyCubemaps.Reset();
}

void UPSNativeWaterBackdropComponent::UpdateReflections(bool bEnabled,UTextureCube* Cubemap)
{
	// Never serialize altered lighting into the user's editor map. Only the
	// running world's copy is changed, and intensity/color remain authored.
	if(!bEnabled || !Cubemap || !GetWorld() || !GetWorld()->IsGameWorld()) { RestoreReflections(); return; }
	for(TActorIterator<ASkyLight> It(GetWorld());It;++It)
	{
		auto* Sky=It->GetLightComponent(); if(!Sky || !Sky->IsVisible()) continue;
		bool bKnown=false; for(const auto& State:Skies) bKnown |= State.Sky.Get()==Sky;
		if(!bKnown)
		{
			FSkyState State; State.Sky=Sky; State.SourceType=Sky->SourceType;
			Skies.Add(State); OriginalSkyCubemaps.Add(Sky->Cubemap);
		}
		if(Sky->SourceType!=SLS_SpecifiedCubemap || Sky->Cubemap!=Cubemap)
		{
			Sky->SourceType=SLS_SpecifiedCubemap; Sky->SetCubemap(Cubemap); Sky->RecaptureSky();
		}
	}
}

void UPSNativeWaterBackdropComponent::Update(bool bNativeWater)
{
	if(!bNativeWater || !GetWorld()) { Restore(); RestoreReflections(); return; }
	for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
	{
		auto* Mesh=It->GetStaticMeshComponent();
		if(!Mesh || !Mesh->GetStaticMesh() || !Mesh->GetMaterial(0)) continue;
		// Exact engine-template assets, not arbitrary authored floors or scenery.
		if(Mesh->GetStaticMesh()->GetPathName()!=TEXT("/Engine/MapTemplates/SM_Template_Map_Floor.SM_Template_Map_Floor")
			|| Mesh->GetMaterial(0)->GetPathName()!=TEXT("/Engine/OpenWorldTemplate/LandscapeMaterial/MI_ProcGrid.MI_ProcGrid")) continue;
		bool bKnown=false; for(const auto& State:Floors) bKnown |= State.Actor.Get()==*It;
		if(!bKnown)
		{
			FHiddenFloor State; State.Actor=*It; State.bGameHidden=It->IsHidden();
#if WITH_EDITOR
			State.bEditorHidden=It->IsTemporarilyHiddenInEditor();
#endif
			Floors.Add(State);
		}
		if(GetWorld()->IsGameWorld()) It->SetActorHiddenInGame(true);
#if WITH_EDITOR
		else It->SetIsTemporarilyHiddenInEditor(true);
#endif
	}
}
