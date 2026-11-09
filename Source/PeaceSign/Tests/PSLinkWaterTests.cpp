#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "WaterBodyCustomActor.h"
#include "WaterBodyComponent.h"
#include "WaterSubsystem.h"
#include "WaterWaves.h"
#include "GerstnerWaterWaves.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "ImageUtils.h"
#include "SceneViewExtension.h"
#include "../World/PSLinkWaterComparison.h"
#include "Tests/AutomationCommon.h"

class FPSLinkCapture : public IAutomationLatentCommand
{
	TFunction<void()> Work;
public:
	explicit FPSLinkCapture(TFunction<void()> InWork) : Work(MoveTemp(InWork)) {}
	virtual bool Update() override { Work(); return true; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSLinkWaterTest, "PeaceSign.Grid.LinkWaterComparison",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSLinkWaterTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), World)) return false;
	auto* Pond = World->SpawnActor<APSLinkWaterComparison>();
	Pond->SetActorLocation(FVector(50000,50000,0));
	Pond->RefreshComparison();
	auto* Body = Cast<AWaterBodyCustom>(Pond->NativeWater->GetChildActor());
	if (!TestNotNull(TEXT("Actual Water Body Custom exists"), Body)) { Pond->Destroy(); return false; }
	TestNotNull(TEXT("Native mesh override"), Body->GetWaterBodyComponent()->GetWaterMeshOverride());
	TestNotNull(TEXT("Native Water Waves Asset"), Pond->Waves.Get());
	const UWaterWaves* Waves = Pond->Waves ? Pond->Waves->GetWaterWaves() : nullptr;
	if (TestNotNull(TEXT("Gerstner source"), Waves))
	{
		TestTrue(TEXT("Waves stay below 20cm rim"), Waves->GetMaxWaveHeight() > 0 && Waves->GetMaxWaveHeight() < 18);
		double MaxChange = 0;
		for (int32 X=0; X<5; ++X) for(int32 Y=0; Y<5; ++Y)
		{
			FVector P(50125+X*50,50125+Y*50,0);
			MaxChange = FMath::Max(MaxChange, FMath::Abs(double(Waves->GetSimpleWaveHeightAtPosition(P,512,0) - Waves->GetSimpleWaveHeightAtPosition(P,512,0.7f))));
		}
		TestTrue(TEXT("Native waves actually change height over time"), MaxChange > 0.1);
		AddInfo(FString::Printf(TEXT("Native waves maximum sampled time displacement: %.3f cm"), MaxChange));
	}
	Pond->bUseLinkWater = false; Pond->RefreshComparison();
	TestTrue(TEXT("Legacy switch hides native body"), Body->IsHidden());
	Pond->bUseLinkWater = true; Pond->RefreshComparison();
	TestFalse(TEXT("Native switch unhides body"), Body->IsHidden());
	TestFalse(TEXT("Preview cannot change gameplay collision"), Pond->GetActorEnableCollision());
	if (FParse::Param(FCommandLine::Get(), TEXT("ExportLinkWaterGPU")))
	{
		auto Work = [this, World, Pond, Body]()
		{
		auto* Light = World->SpawnActor<ADirectionalLight>();
		Light->SetActorRotation(FRotator(-50,-30,0));
		Light->GetLightComponent()->SetIntensity(6);
		auto* Camera = World->SpawnActor<ASceneCapture2D>();
		Camera->SetActorLocation(FVector(49800,50250,550));
		Camera->SetActorRotation(FRotator(-50,0,0));
		auto* Capture = Camera->GetCaptureComponent2D();
		// SceneCapture does not automatically opt into the world's native Water
		// view extension. Without it the wave GPU buffers are never bound.
		Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
		Capture->ProjectionType=ECameraProjectionMode::Orthographic; Capture->OrthoWidth=650;
		Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
		Capture->PostProcessSettings.AutoExposureBias = 1;
		Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyActorComponents(Pond); Capture->ShowOnlyActorComponents(Body);
		TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
		Pond->GetComponents(Components);
		for(auto* C:Components) C->BuildTreeIfOutdated(false,true);
		auto* Target=NewObject<UTextureRenderTarget2D>(World);
		Target->InitCustomFormat(512,512,PF_B8G8R8A8,false); Target->UpdateResourceImmediate(true);
		Capture->TextureTarget=Target;
		World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
		Capture->CaptureScene(); FlushRenderingCommands();
		if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FAssetCompilingManager::Get().FinishAllCompilation();
		auto* Subsystem=World->GetSubsystem<UWaterSubsystem>();
		auto* Time=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.OverrideWavesTime"));
		const float OldTime=Time ? Time->GetFloat() : -1;
		TArray<FColor> Before,After;
		for(int32 Frame=0; Frame<2; ++Frame)
		{
			if(Time) Time->Set(Frame==0 ? 0.0f : 0.7f, ECVF_SetByCode);
			if(Subsystem) Subsystem->Tick(0);
			World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
			Capture->CaptureScene(); FlushRenderingCommands();
			TestTrue(TEXT("Native GPU frame readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Frame==0 ? Before : After));
		}
		if(Time) Time->Set(OldTime,ECVF_SetByCode);
		if(Subsystem) Subsystem->Tick(0);
		if(Before.Num()==512*512 && After.Num()==Before.Num())
		{
			int32 Changed=0,Visible=0;
			for(int32 I=0;I<Before.Num();++I)
			{
				Visible += Before[I].G>20;
				Changed += FMath::Abs(int32(Before[I].R)-After[I].R)+FMath::Abs(int32(Before[I].G)-After[I].G)+FMath::Abs(int32(Before[I].B)-After[I].B)>6;
			}
			AddInfo(FString::Printf(TEXT("Native water GPU: visible %d changed %d"),Visible,Changed));
			TestTrue(TEXT("Visible native comparison"), Visible>10000);
			AddWarning(TEXT("SceneCapture does not verify native Water view data; use PS.Water.LinkSmoke in the real game viewport for animation."));
			const FString Dir=FPaths::ProjectSavedDir()/TEXT("TileDiagnostics"); IFileManager::Get().MakeDirectory(*Dir,true);
			FImageUtils::SaveImageByExtension(*(Dir/TEXT("LinkWater0.png")),FImageView(Before.GetData(),512,512));
			FImageUtils::SaveImageByExtension(*(Dir/TEXT("LinkWater1.png")),FImageView(After.GetData(),512,512));
		}
		Camera->Destroy(); Light->Destroy();
		Pond->Destroy();
		};
		// Native water uploads its body/wave buffers from BeginFrame delegates.
		// A synchronous capture immediately after spawning cannot verify them.
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FPSLinkCapture(Work));
		return true;
	}
	Pond->Destroy();
	return true;
}
#endif
