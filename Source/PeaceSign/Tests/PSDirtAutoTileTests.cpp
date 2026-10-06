// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "../World/PSDirtAutoTileSet.h"
#include "../World/PSGridWorld.h"
#include "../World/PSTileChunkActor.h"
#include "../World/PSTileTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPSDirtAutoTileTest,
	"PeaceSign.Grid.DirtAutoTile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSDirtAutoTileTest::RunTest(const FString& Parameters)
{
	struct FTilePixels
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray64<uint8> Data;
		bool IsGrass(int32 X, int32 Y) const
		{
			const int64 Offset = (static_cast<int64>(Y) * Width + X) * 4;
			return Data[Offset + 1] > Data[Offset + 2];
		}
	};
	TMap<uint8, FTilePixels> TilePixels;
	if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		const FStaticMeshLODResources& LOD = Plane->GetRenderData()->LODResources[0];
		for (uint32 Index = 0; Index < LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices(); ++Index)
		{
			const FVector3f Position = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Index);
			const FVector2f UV = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Index, 0);
			TestEqual(TEXT("Plane image right follows world +X"), UV.X, (Position.X + 50.0f) / 100.0f);
			TestEqual(TEXT("Plane image bottom follows world +Y"), UV.Y, (Position.Y + 50.0f) / 100.0f);
		}
	}
	TestEqual(TEXT("Plain grass uses Set4 MM"), PSDirtAutoTile::SelectDefaultVariant(0, 0), static_cast<uint8>(32));
	TestEqual(TEXT("Grass surrounded by dirt remains a full grass cell"),
		PSDirtAutoTile::SelectDefaultVariant(255, 0), static_cast<uint8>(32));
	TestEqual(TEXT("Irrelevant diagonals do not turn a straight edge into a narrow path"),
		PSDirtAutoTile::SelectDefaultVariant(462 | PSDirtAutoTile::NorthWest, 0), static_cast<uint8>(11));
	TestEqual(TEXT("Unsupported inner corners use solid dirt instead of inventing four holes"),
		PSDirtAutoTile::SelectDefaultVariant(511 & ~PSDirtAutoTile::NorthWest & ~PSDirtAutoTile::SouthEast, 0),
		static_cast<uint8>(23));
	TestEqual(TEXT("Isolated dirt uses Set1 MM"),
		PSDirtAutoTile::SelectDefaultVariant(PSDirtAutoTile::Center, 0), static_cast<uint8>(5));
	TestEqual(TEXT("Fully surrounded dirt uses Set3 MM"),
		PSDirtAutoTile::SelectDefaultVariant(511, 0), static_cast<uint8>(23));
	TestEqual(TEXT("Top edge uses continuous Set2 MT"),
		PSDirtAutoTile::SelectDefaultVariant(462, 0), static_cast<uint8>(11));
	TestEqual(TEXT("Left edge uses continuous Set2 LM"),
		PSDirtAutoTile::SelectDefaultVariant(359, 0), static_cast<uint8>(13));
	TestEqual(TEXT("Right edge uses continuous Set2 RM"),
		PSDirtAutoTile::SelectDefaultVariant(413, 0), static_cast<uint8>(15));
	TestEqual(TEXT("Bottom edge uses continuous Set2 MB"),
		PSDirtAutoTile::SelectDefaultVariant(315, 0), static_cast<uint8>(17));
	TestEqual(TEXT("Right-connected endpoint uses Set6 LH"),
		PSDirtAutoTile::SelectDefaultVariant(PSDirtAutoTile::Center | PSDirtAutoTile::East, 0),
		static_cast<uint8>(46));
	TestEqual(TEXT("Horizontal straight always uses the compatible Set6 piece"),
		PSDirtAutoTile::SelectDefaultVariant(
			PSDirtAutoTile::Center | PSDirtAutoTile::East | PSDirtAutoTile::West, 1234),
		static_cast<uint8>(47));
	TestEqual(TEXT("Vertical straight always uses the compatible Set6 piece"),
		PSDirtAutoTile::SelectDefaultVariant(
			PSDirtAutoTile::Center | PSDirtAutoTile::North | PSDirtAutoTile::South, 5678),
		static_cast<uint8>(50));

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
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path);
		TestNotNull(TEXT("Every imported dirt texture resolves from its default asset path"), Texture);
		if (Texture && Texture->Source.GetFormat() == TSF_BGRA8)
		{
			FTilePixels& Pixels = TilePixels.Add(Variant);
			Pixels.Width = Texture->Source.GetSizeX();
			Pixels.Height = Texture->Source.GetSizeY();
			TestTrue(TEXT("Source pixels are readable for tile seam validation"), Texture->Source.GetMipData(Pixels.Data, 0));
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("ExportTileVisuals")) && Texture)
		{
			TArray64<uint8> Pixels;
			if (Texture->Source.GetFormat() == TSF_BGRA8 && Texture->Source.GetMipData(Pixels, 0))
			{
				const FString Directory = FPaths::ProjectSavedDir() / TEXT("TileDiagnostics");
				IFileManager::Get().MakeDirectory(*Directory, true);
				const FImageView View(Pixels.GetData(), Texture->Source.GetSizeX(), Texture->Source.GetSizeY(),
					1, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
				FImageUtils::SaveImageByExtension(*(Directory / FString::Printf(TEXT("Tile_%02d.png"), Variant)), View);
			}
		}
	}

	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(
		EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	APSGridWorld* Grid = World->SpawnActor<APSGridWorld>();
	// Exercise the actual world mask builder against the mesh's UV directions.
	FPSChunkData& DirectionChunk = Grid->GetOrCreateChunk(FIntPoint::ZeroValue);
	for (FPSTileCell& Cell : DirectionChunk.Cells) Cell.GroundType = EPSTileType::Grass;
	const FIntPoint CenterCell(4, 4);
	const FIntPoint Offsets[] = {FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0),
		FIntPoint(-1, -1), FIntPoint(1, -1), FIntPoint(1, 1), FIntPoint(-1, 1)};
	DirectionChunk.Cells[PSGrid::LocalToIndex(CenterCell, Grid->ChunkSize)].GroundType = EPSTileType::Dirt;
	for (int32 Direction = 0; Direction < UE_ARRAY_COUNT(Offsets); ++Direction)
	{
		FPSTileCell& Neighbor = DirectionChunk.Cells[PSGrid::LocalToIndex(CenterCell + Offsets[Direction], Grid->ChunkSize)];
		Neighbor.GroundType = EPSTileType::Dirt;
		TestEqual(TEXT("World neighbors are mapped in texture UV order"),
			Grid->BuildDirtNeighborMask(CenterCell), static_cast<uint16>(PSDirtAutoTile::Center | (1 << Direction)));
		Neighbor.GroundType = EPSTileType::Grass;
	}
	Grid->LoadedChunks.Reset();

	// Validate artwork, not just IDs: a rounded grass patch must keep the
	// same terrain on both sides of each shared image edge, even under stones.
	FPSChunkData& SeamChunk = Grid->GetOrCreateChunk(FIntPoint::ZeroValue);
	for (FPSTileCell& Cell : SeamChunk.Cells)
	{
		Cell.GroundType = EPSTileType::Dirt;
		Cell.ObjectType = EPSWorldObjectType::None;
	}
	for (int32 Y = 2; Y <= 12; ++Y)
		for (int32 X = 2; X <= 12; ++X)
			if (FMath::Square(2 * (X - 7) + 1) + FMath::Square(2 * (Y - 7) + 1) <= 100)
				SeamChunk.Cells[PSGrid::LocalToIndex(FIntPoint(X, Y), Grid->ChunkSize)].GroundType = EPSTileType::Grass;
	const FPSChunkData WithoutObjects = Grid->BuildRenderChunkData(FIntPoint::ZeroValue, SeamChunk);
	for (FPSTileCell& Cell : SeamChunk.Cells) Cell.ObjectType = EPSWorldObjectType::Stone;
	const FPSChunkData WithObjects = Grid->BuildRenderChunkData(FIntPoint::ZeroValue, SeamChunk);
	for (int32 Index = 0; Index < WithoutObjects.Cells.Num(); ++Index)
		TestEqual(TEXT("Stone objects do not change any ground tile variant"),
			WithObjects.Cells[Index].Variant, WithoutObjects.Cells[Index].Variant);
	for (int32 Y = 2; Y < 13; ++Y)
	{
		for (int32 X = 2; X < 13; ++X)
		{
			const FTilePixels* Tile = TilePixels.Find(WithoutObjects.Cells[Y * Grid->ChunkSize + X].Variant);
			if (!Tile || Tile->Data.IsEmpty()) continue;
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				const int32 NeighborIndex = (Y + Axis) * Grid->ChunkSize + X + (1 - Axis);
				const FTilePixels* Neighbor = TilePixels.Find(WithoutObjects.Cells[NeighborIndex].Variant);
				if (!Neighbor || Neighbor->Data.IsEmpty()) continue;
				int32 Mismatches = 0;
				constexpr int32 Samples = 100;
				for (int32 Sample = 0; Sample < Samples; ++Sample)
				{
					const int32 TX = Axis == 0 ? Tile->Width - 1 : Sample * Tile->Width / Samples;
					const int32 TY = Axis == 0 ? Sample * Tile->Height / Samples : Tile->Height - 1;
					const int32 NX = Axis == 0 ? 0 : Sample * Neighbor->Width / Samples;
					const int32 NY = Axis == 0 ? Sample * Neighbor->Height / Samples : 0;
					Mismatches += Tile->IsGrass(TX, TY) != Neighbor->IsGrass(NX, NY);
				}
				TestTrue(*FString::Printf(TEXT("Grass patch seam at (%d,%d) axis %d has no large discontinuity (%d%%)"),
					X, Y, Axis, Mismatches), Mismatches <= 15);
			}
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ExportTileVisuals")))
	{
		constexpr int32 PixelsPerCell = 48;
		const int32 Size = Grid->ChunkSize * PixelsPerCell;
		TArray<FColor> Preview;
		Preview.SetNum(Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
			for (int32 X = 0; X < Size; ++X)
			{
				const uint8 Variant = WithoutObjects.Cells[(Y / PixelsPerCell) * Grid->ChunkSize + X / PixelsPerCell].Variant;
				const FTilePixels* Tile = TilePixels.Find(Variant);
				if (!Tile || Tile->Data.IsEmpty()) continue;
				const int32 SX = (X % PixelsPerCell) * Tile->Width / PixelsPerCell;
				const int32 SY = (Y % PixelsPerCell) * Tile->Height / PixelsPerCell;
				const int64 Offset = (static_cast<int64>(SY) * Tile->Width + SX) * 4;
				Preview[Y * Size + X] = FColor(Tile->Data[Offset + 2], Tile->Data[Offset + 1], Tile->Data[Offset], 255);
			}
		FImageUtils::SaveImageByExtension(*(FPaths::ProjectSavedDir() / TEXT("TileDiagnostics/GrassPatch.png")),
			FImageView(Preview.GetData(), Size, Size));
	}
	Grid->LoadedChunks.Reset();
	int32 DirtCount = 0;
	int32 GrassCount = 0;
	int32 IsolatedDirtCount = 0;
	for (int32 Y = -40; Y < 40; ++Y)
	{
		for (int32 X = -40; X < 40; ++X)
		{
			const FIntPoint Cell(X, Y);
			const EPSTileType Ground = Grid->GetGroundTile(Cell);
			if (Ground == EPSTileType::Grass) ++GrassCount;
			if (Ground != EPSTileType::Dirt) continue;
			++DirtCount;
			const bool bHasDirtNeighbor = Grid->GetGroundTile(Cell + FIntPoint(1, 0)) == EPSTileType::Dirt
				|| Grid->GetGroundTile(Cell + FIntPoint(-1, 0)) == EPSTileType::Dirt
				|| Grid->GetGroundTile(Cell + FIntPoint(0, 1)) == EPSTileType::Dirt
				|| Grid->GetGroundTile(Cell + FIntPoint(0, -1)) == EPSTileType::Dirt;
			if (!bHasDirtNeighbor) ++IsolatedDirtCount;
		}
	}
	TestTrue(TEXT("Grass is the dominant non-water terrain"), GrassCount > DirtCount * 4);
	TestTrue(TEXT("Dirt patches are present"), DirtCount > 100);
	TestTrue(TEXT("Dirt is clustered instead of mostly isolated noise"), IsolatedDirtCount * 5 < DirtCount);

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
