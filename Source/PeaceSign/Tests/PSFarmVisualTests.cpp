#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../World/PSFarmAutoTile.h"
#include "../World/PSTileStamp.h"
#include "../World/PSGridWorld.h"
#include "../World/PSFarmTargetMarker.h"
#include "../World/PSTileChunkActor.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "RenderingThread.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "Editor.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSFarmVisualTest, "PeaceSign.Farming.TileArtwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPSFarmVisualTest::RunTest(const FString& Parameters)
{
	TArray<UTexture2D*> Textures;
	Textures.SetNum(PSFarmAutoTile::VariantCount + 1);
	for (uint8 V = 1; V <= PSFarmAutoTile::VariantCount; ++V)
	{
		Textures[V] = LoadObject<UTexture2D>(nullptr, PSFarmAutoTile::GetTexturePath(V));
		TestNotNull(TEXT("Every source farm image exists"), Textures[V]);
		TestEqual(TEXT("Every supplied piece can be selected exactly"), PSFarmAutoTile::SelectExact(PSFarmAutoTile::GetMask(V)), V);
	}
	UTexture2D* Wet = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Art/Tiles/Farm/Farm_Water.Farm_Water"));
	TestNotNull(TEXT("Water overlay exists"), Wet);
	TSet<uint16> Normalized;
	const uint16 CornerBits[] = {25, 35, 140, 70};
	for (uint16 Mask = 256; Mask < 512; ++Mask)
	{
		const uint16 Expected = PSFarmAutoTile::NormalizeMask(Mask);
		Normalized.Add(Expected);
		for (uint8 Corner = 0; Corner < 4; ++Corner)
		{
			const uint8 V = PSFarmAutoTile::SelectCorner(Mask, Corner);
			TestTrue(TEXT("All neighbor arrangements have four valid corner sources"), V > 0 && V <= PSFarmAutoTile::VariantCount);
			TestEqual(TEXT("Selected corner preserves both edges and the inner diagonal"),
				PSFarmAutoTile::GetMask(V) & CornerBits[Corner], Expected & CornerBits[Corner]);
		}
	}
	TestEqual(TEXT("All 47 normalized farm shapes are covered"), Normalized.Num(), 47);
	for (int32 Set = 1; Set <= 4; ++Set)
	{
		FPSTileStamp Stamp; Stamp.bFarm = true; Stamp.Set = Set;
		const FIntPoint Size = PSTileStamp::GetSize(Stamp);
		int32 Pieces = 0;
		for (int32 Y = 0; Y < Size.Y; ++Y) for (int32 X = 0; X < Size.X; ++X)
		{
			FPSTileCell Cell;
			if (!PSTileStamp::BuildCell(Stamp, FIntPoint(X, Y), Cell)) continue;
			++Pieces;
			TestEqual(TEXT("Farm stamp creates tillable farmland"), Cell.GroundType, EPSTileType::TilledSoil);
			TestTrue(TEXT("Farm stamp preserves exact artwork"), Cell.bFixedVariant && Cell.Variant <= 23);
		}
		TestEqual(TEXT("Farm source stamp piece count"), Pieces, Set == 1 ? 9 : Set == 2 ? 3 : 4);
	}
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	World->InitializeActorsForPlay(FURL());
	APSGridWorld* Grid = World->SpawnActor<APSGridWorld>();
	Grid->SaveSlotName = TEXT("FarmVisual_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Grid->SetGroundTile(FIntPoint(-1, -1), EPSTileType::TilledSoil);
	Grid->SetGroundTile(FIntPoint(0, -1), EPSTileType::TilledSoil);
	for (const FIntPoint Cell : {FIntPoint(-1, -1), FIntPoint(0, -1)})
	{
		const FIntPoint CC = PSGrid::CellToChunk(Cell, Grid->ChunkSize);
		const auto Render = Grid->BuildRenderChunkData(CC, Grid->GetOrCreateChunk(CC));
		const auto& Tile = Render.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, Grid->ChunkSize), Grid->ChunkSize)];
		TestEqual(TEXT("Horizontal farm strip connects across negative chunk boundary"), Tile.Variant,
			static_cast<uint8>(Cell.X == -1 ? 10 : 12));
	}
	APSTileChunkActor* Renderer = World->SpawnActor<APSTileChunkActor>();
	FPSChunkData Data;
	Data.Cells.SetNum(1);
	Data.Cells[0].GroundType = EPSTileType::TilledSoil;
	Data.Cells[0].bWatered = true;
	Data.Cells[0].CropType = EPSCropType::TestCrop;
	for (float CellSize : {100.0f, 50.0f})
	{
		Renderer->Rebuild(Data, 1, CellSize);
		TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
		Renderer->GetComponents(Components);
		int32 BaseParts = 0, WetParts = 0;
		float BaseZ = 0, WetZ = 0;
		for (auto* C : Components)
		{
			if (C->GetName().StartsWith(TEXT("FarmVariant_")))
			{
				BaseParts += C->GetInstanceCount();
				if (C->GetInstanceCount() == 0) continue;
				FTransform Transform; C->GetInstanceTransform(0, Transform, false);
				BaseZ = Transform.GetLocation().Z;
				auto* Material = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
				TestNotNull(TEXT("Corner has dynamic UV material"), Material);
				if (Material) TestEqual(TEXT("Corner samples half-size UV region"), Material->K2_GetVectorParameterValue(TEXT("UVScale")).R, 0.5f);
				if (Material) TestTrue(TEXT("Farm shader supports instanced static meshes"), Material->GetMaterial()->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
			}
			if (C->GetFName() == TEXT("WetSoilInstances"))
			{
				WetParts = C->GetInstanceCount();
				if (WetParts > 0) { FTransform Transform; C->GetInstanceTransform(0, Transform, false); WetZ = Transform.GetLocation().Z; }
				auto* Material = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
				TestTrue(TEXT("Water material masks transparent pixels"), Material && Material->GetBlendMode() == BLEND_Masked);
				if (Material) TestTrue(TEXT("Overlay uses provided Farm_Water image"), Material->K2_GetTextureParameterValue(TEXT("SpriteTexture")) == Wet);
				TestEqual(TEXT("Water overlay has no collision"), C->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			}
		}
		TestEqual(TEXT("Single farm composes four rounded corners"), BaseParts, 4);
		TestEqual(TEXT("One wet tile has one overlay"), WetParts, 1);
		TestEqual(TEXT("Water is exactly Z+1 independent of cell size"), WetZ - BaseZ, 1.0f);
	}
	Data.Cells[0].bWatered = false;
	Renderer->Rebuild(Data, 1, 100.0f);
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Renderer->GetComponents(Components);
	for (auto* C : Components) if (C->GetFName() == TEXT("WetSoilInstances")) TestEqual(TEXT("Drying clears overlay instances"), C->GetInstanceCount(), 0);
#if WITH_EDITOR
	if (FParse::Param(FCommandLine::Get(), TEXT("ExportFarmVisuals")) && Wet)
	{
		TArray<TArray64<uint8>> Pixels; Pixels.SetNum(Textures.Num());
		bool bReadable = true;
		for (int32 V = 1; V < Textures.Num(); ++V)
			bReadable &= Textures[V] && Textures[V]->Source.GetSizeX() == 144 && Textures[V]->Source.GetSizeY() == 144
				&& Textures[V]->Source.GetFormat() == TSF_BGRA8 && Textures[V]->Source.GetMipData(Pixels[V], 0);
		TArray64<uint8> WetPixels;
		bReadable &= Wet->Source.GetSizeX() == 144 && Wet->Source.GetSizeY() == 144 && Wet->Source.GetFormat() == TSF_BGRA8 && Wet->Source.GetMipData(WetPixels, 0);
		TestTrue(TEXT("Artwork pixels readable"), bReadable);
		if (bReadable)
		{
			TArray<FColor> Preview; Preview.SetNum(144 * 8 * 144 * 4);
			for (int32 Layout = 0; Layout < 16; ++Layout)
			{
				const uint16 Mask = 256 | Layout;
				for (int32 WetState = 0; WetState < 2; ++WetState)
					for (int32 Y = 0; Y < 144; ++Y) for (int32 X = 0; X < 144; ++X)
					{
						uint8 V = PSFarmAutoTile::SelectExact(Mask);
						if (!V) V = PSFarmAutoTile::SelectCorner(Mask, (Y >= 72 ? 2 : 0) + (X >= 72 ? 1 : 0));
						const int32 I = (Y * 144 + X) * 4;
						FColor Color(Pixels[V][I+2], Pixels[V][I+1], Pixels[V][I], 255);
						if (WetState)
						{
							const float A = WetPixels[I+3] / 255.0f >= 0.3333f ? 1.0f : 0.0f;
							Color.R = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Color.R), static_cast<float>(WetPixels[I+2]), A));
							Color.G = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Color.G), static_cast<float>(WetPixels[I+1]), A));
							Color.B = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Color.B), static_cast<float>(WetPixels[I]), A));
						}
						Preview[((Layout / 4) * 144 + Y) * 144 * 8 + ((Layout % 4) * 2 + WetState) * 144 + X] = Color;
					}
			}
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("TileDiagnostics");
			IFileManager::Get().MakeDirectory(*Dir, true);
			TestTrue(TEXT("Dry/wet artwork comparison exported"), FImageUtils::SaveImageByExtension(*(Dir / TEXT("FarmDryWet.png")), FImageView(Preview.GetData(), 144 * 8, 144 * 4)));
		}
	}
#endif
#if WITH_EDITOR
	if (FParse::Param(FCommandLine::Get(), TEXT("RenderFarmScene")))
	{
		UWorld* CaptureWorld = GEditor->GetEditorWorldContext().World();
		APSTileChunkActor* CaptureRenderer = CaptureWorld->SpawnActor<APSTileChunkActor>();
		CaptureRenderer->SetActorLocation(FVector(50000, 50000, 0));
		FPSChunkData SceneData; SceneData.Cells.SetNum(9);
		for (int32 I = 0; I < 9; ++I)
		{
			SceneData.Cells[I].GroundType = EPSTileType::TilledSoil;
			SceneData.Cells[I].Variant = I + 1;
			SceneData.Cells[I].bWatered = (I % 2) == 0;
		}
		SceneData.Cells[4].CropType = EPSCropType::TestCrop;
		CaptureRenderer->Rebuild(SceneData, 3, 100.0f);
		APSFarmTargetMarker* GreenMarker = CaptureWorld->SpawnActor<APSFarmTargetMarker>();
		APSFarmTargetMarker* RedMarker = CaptureWorld->SpawnActor<APSFarmTargetMarker>();
		GreenMarker->ShowTarget(FVector(50050, 50050, 0), 100, true);
		RedMarker->ShowTarget(FVector(50250, 50250, 0), 100, false);
		TArray<UHierarchicalInstancedStaticMeshComponent*> SceneComponents;
		CaptureRenderer->GetComponents(SceneComponents);
		for (auto* C : SceneComponents) C->BuildTreeIfOutdated(false, true);
		GreenMarker->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>()->BuildTreeIfOutdated(false, true);
		RedMarker->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>()->BuildTreeIfOutdated(false, true);
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FAssetCompilingManager::Get().FinishAllCompilation();
		for (auto* C : SceneComponents) C->MarkRenderStateDirty();
		GreenMarker->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>()->MarkRenderStateDirty();
		RedMarker->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>()->MarkRenderStateDirty();
		ASceneCapture2D* Camera = CaptureWorld->SpawnActor<ASceneCapture2D>();
		Camera->SetActorLocation(FVector(50150, 50150, 1000));
		Camera->SetActorRotation(FRotator(-90, 0, 0));
		USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->ProjectionType = ECameraProjectionMode::Perspective;
		Capture->FOVAngle = 25;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyActorComponents(CaptureRenderer);
		Capture->ShowOnlyActorComponents(GreenMarker);
		Capture->ShowOnlyActorComponents(RedMarker);
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(World);
		Target->InitCustomFormat(512, 512, PF_B8G8R8A8, false);
		Target->UpdateResourceImmediate(true);
		Capture->TextureTarget = Target;
		CaptureWorld->SendAllEndOfFrameUpdates();
		FlushRenderingCommands();
		// On-demand material permutations are submitted by the first draw.
		// Warm up, finish those jobs, then capture the ready frame.
		Capture->CaptureScene();
		FlushRenderingCommands();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FAssetCompilingManager::Get().FinishAllCompilation();
		FlushRenderingCommands();
		Capture->CaptureScene();
		FlushRenderingCommands();
		TArray<FColor> Pixels;
		TestTrue(TEXT("Actual GPU farm capture readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
		if (Pixels.Num() == 512 * 512)
		{
			int32 VisiblePixels = 0;
			for (const FColor Pixel : Pixels) VisiblePixels += Pixel.R > 70 && Pixel.R > Pixel.G + 20;
			TestTrue(TEXT("GPU capture contains visible farm artwork"), VisiblePixels > 10000);
			int32 GreenPixels = 0, RedPixels = 0;
			for (const FColor Pixel : Pixels)
			{
				GreenPixels += Pixel.G > 150 && Pixel.G > Pixel.R + 50 && Pixel.G > Pixel.B + 50;
				RedPixels += Pixel.R > 180 && Pixel.R > Pixel.G + 100 && Pixel.R > Pixel.B + 100;
			}
			TestTrue(TEXT("GPU capture contains usable green target border"), GreenPixels > 300);
			TestTrue(TEXT("GPU capture contains unusable red target border"), RedPixels > 300);
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("TileDiagnostics");
			IFileManager::Get().MakeDirectory(*Dir, true);
			TestTrue(TEXT("Actual GPU farm capture exported"), FImageUtils::SaveImageByExtension(*(Dir / TEXT("FarmScene.png")), FImageView(Pixels.GetData(), 512, 512)));
		}
		Camera->Destroy();
		GreenMarker->Destroy();
		RedMarker->Destroy();
		CaptureRenderer->Destroy();
	}
#endif
	const FString TestSaveSlot = Grid->SaveSlotName;
	World->DestroyWorld(false);
	UGameplayStatics::DeleteGameInSlot(TestSaveSlot, 0);
	return true;
}
#endif
