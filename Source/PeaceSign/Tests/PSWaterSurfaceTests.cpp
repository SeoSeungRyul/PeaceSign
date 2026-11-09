#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "ImageUtils.h"
#include "../World/PSTileChunkActor.h"
#include "../World/PSTileTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSWaterSurfaceTest, "PeaceSign.Grid.WaterSurfaceRendering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSWaterSurfaceTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ExportWaterSurfaceGPU")))
	{
		AddInfo(TEXT("GPU water comparison requires -ExportWaterSurfaceGPU and a real RHI."));
		return true;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor capture world"), World)) return false;
	auto* Renderer = World->SpawnActor<APSTileChunkActor>();
	Renderer->SetActorLocation(FVector(50000, 50000, 0));
	FPSChunkData Chunk; Chunk.Cells.SetNum(1);
	Chunk.Cells[0].GroundType = EPSTileType::Water;
	Chunk.Cells[0].Variant = 19; // Rounded shoreline: cyan water, grass, dark shadow.
	Renderer->Rebuild(Chunk, 1, 100);
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Renderer->GetComponents(Components);
	UMaterialInstanceDynamic* Material = nullptr;
	for (auto* C : Components)
	{
		C->BuildTreeIfOutdated(false, true);
		if (C->GetFName() == TEXT("WaterVariant_19")) Material = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
	}
	if (!TestNotNull(TEXT("Animated water instance exists"), Material)) { Renderer->Destroy(); return false; }
	auto* Camera = World->SpawnActor<ASceneCapture2D>();
	Camera->SetActorLocation(FVector(50050, 50050, 400));
	Camera->SetActorRotation(FRotator(-90, 0, 0));
	auto* Capture = Camera->GetCaptureComponent2D();
	Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->OrthoWidth = 100;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActorComponents(Renderer);
	Capture->ShowFlags.SetAntiAliasing(false);
	auto* Target = NewObject<UTextureRenderTarget2D>(World);
	Target->InitCustomFormat(256, 256, PF_B8G8R8A8, false);
	Target->UpdateResourceImmediate(true); Capture->TextureTarget = Target;
	World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
	Capture->CaptureScene(); FlushRenderingCommands();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	FAssetCompilingManager::Get().FinishAllCompilation();
	const auto Snapshot = [&](float Ripple, float Shimmer, TArray<FColor>& Pixels)
	{
		Material->SetScalarParameterValue(TEXT("RippleStrength"), Ripple);
		Material->SetScalarParameterValue(TEXT("ShimmerStrength"), Shimmer);
		FlushRenderingCommands(); Capture->CaptureScene(); FlushRenderingCommands();
		return Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
	};
	TArray<FColor> Before, After;
	TestTrue(TEXT("Baseline water GPU image readable"), Snapshot(0, 0, Before));
	TestTrue(TEXT("Animated water GPU image readable"), Snapshot(.25f, .1f, After));
	if (Before.Num() == 256 * 256 && After.Num() == Before.Num())
	{
		int32 ChangedWater = 0, Grass = 0, ChangedGrass = 0, Shadow = 0, ChangedShadow = 0;
		for (int32 I = 0; I < Before.Num(); ++I)
		{
			const FColor A = Before[I], B = After[I];
			const bool Changed = FMath::Abs(int32(A.R)-B.R) + FMath::Abs(int32(A.G)-B.G) + FMath::Abs(int32(A.B)-B.B) > 3;
			if (A.G > A.B + 15 && A.G > A.R + 15) { ++Grass; ChangedGrass += Changed; }
			else if (A.B > A.G && A.G < 130 && A.B > 80) { ++Shadow; ChangedShadow += Changed; }
			else if (A.B > A.R + 30) ChangedWater += Changed;
		}
		AddInfo(FString::Printf(TEXT("GPU pixels: changed water %d; grass %d/%d; shadow %d/%d"), ChangedWater, ChangedGrass, Grass, ChangedShadow, Shadow));
		TestTrue(TEXT("GPU water surface visibly changes"), ChangedWater > 500);
		TestTrue(TEXT("GPU reference contains grass"), Grass > 500);
		TestEqual(TEXT("Grass stays unchanged"), ChangedGrass, 0);
		TestTrue(TEXT("GPU reference contains shoreline shadow"), Shadow > 100);
		TestEqual(TEXT("Dark shoreline shadow stays unchanged"), ChangedShadow, 0);
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("TileDiagnostics");
		IFileManager::Get().MakeDirectory(*Dir, true);
		TestTrue(TEXT("GPU baseline exported"), FImageUtils::SaveImageByExtension(*(Dir / TEXT("WaterSurfaceBefore.png")), FImageView(Before.GetData(), 256, 256)));
		TestTrue(TEXT("GPU effect exported"), FImageUtils::SaveImageByExtension(*(Dir / TEXT("WaterSurfaceAfter.png")), FImageView(After.GetData(), 256, 256)));
	}
	else AddError(TEXT("GPU capture dimensions invalid"));
	Camera->Destroy(); Renderer->Destroy();
	return true;
}
#endif
