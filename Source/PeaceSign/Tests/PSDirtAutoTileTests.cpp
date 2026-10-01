// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "../World/PSDirtAutoTileSet.h"
#include "../World/PSTileChunkActor.h"
#include "../World/PSTileTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPSDirtAutoTileTest,
	"PeaceSign.Grid.DirtAutoTile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSDirtAutoTileTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Plain grass uses Set4 MM"), PSDirtAutoTile::SelectDefaultVariant(0, 0), static_cast<uint8>(32));
	TestEqual(TEXT("Isolated dirt uses Set1 MM"),
		PSDirtAutoTile::SelectDefaultVariant(PSDirtAutoTile::Center, 0), static_cast<uint8>(5));
	TestEqual(TEXT("Fully surrounded dirt uses Set3 MM"),
		PSDirtAutoTile::SelectDefaultVariant(511, 0), static_cast<uint8>(23));
	TestEqual(TEXT("Right-connected endpoint uses Set6 LH"),
		PSDirtAutoTile::SelectDefaultVariant(PSDirtAutoTile::Center | PSDirtAutoTile::East, 0),
		static_cast<uint8>(46));

	const uint16 UnsupportedDiagonal = PSDirtAutoTile::Center | PSDirtAutoTile::NorthWest;
	const uint8 Fallback = PSDirtAutoTile::SelectDefaultVariant(UnsupportedDiagonal, 1234);
	TestEqual(TEXT("Unsupported diagonals preserve the center and cardinal connections"),
		PSDirtAutoTile::GetDefaultVariantMask(Fallback), PSDirtAutoTile::Center);

	for (uint16 Mask = 0; Mask < 512; ++Mask)
	{
		const uint8 Variant = PSDirtAutoTile::SelectDefaultVariant(Mask, Mask * 7919u);
		TestTrue(TEXT("Every dirt/grass neighborhood has a safe visual fallback"), Variant != PSDirtAutoTile::NoVariant);
		TestNotNull(TEXT("Every selected default variant has a texture path"),
			PSDirtAutoTile::GetDefaultVariantTexturePath(Variant));
		if ((Mask & PSDirtAutoTile::Center) != 0)
		{
			const uint16 ChosenMask = PSDirtAutoTile::GetDefaultVariantMask(Variant);
			TestEqual(TEXT("Dirt fallback keeps all cardinal connections"),
				ChosenMask & (PSDirtAutoTile::Center | PSDirtAutoTile::CardinalMask),
				Mask & (PSDirtAutoTile::Center | PSDirtAutoTile::CardinalMask));
		}
	}

	for (uint8 Variant = 1; Variant <= 51; ++Variant)
	{
		const TCHAR* Path = PSDirtAutoTile::GetDefaultVariantTexturePath(Variant);
		TestNotNull(TEXT("Every imported dirt texture resolves from its default asset path"),
			LoadObject<UTexture2D>(nullptr, Path));
	}

	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(
		EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	APSTileChunkActor* Renderer = World->SpawnActor<APSTileChunkActor>();
	FPSChunkData Chunk;
	Chunk.Cells.SetNum(1);
	Chunk.Cells[0].GroundType = EPSTileType::Dirt;
	Chunk.Cells[0].Variant = 23;
	Renderer->Rebuild(Chunk, 1, 100.0f);
	UHierarchicalInstancedStaticMeshComponent* VariantComponent = nullptr;
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Renderer->GetComponents(Components);
	for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
		if (Component->GetFName() == TEXT("DirtVariant_23")) VariantComponent = Component;
	TestNotNull(TEXT("A variant creates its HISM renderer"), VariantComponent);
	if (VariantComponent)
	{
		TestEqual(TEXT("The variant renderer receives one tile"), VariantComponent->GetInstanceCount(), 1);
		TestNotNull(TEXT("The variant renderer receives a textured material"), VariantComponent->GetMaterial(0));
	}
	World->DestroyWorld(false);
	return true;
}

#endif
