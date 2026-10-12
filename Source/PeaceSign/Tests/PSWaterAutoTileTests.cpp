#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "../World/PSWaterAutoTile.h"
#include "../World/PSGridWorld.h"
#include "../World/PSTileChunkActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSWaterAutoTileTest, "PeaceSign.Grid.WaterAutoTile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSWaterAutoTileTest::RunTest(const FString& Parameters)
{
	struct FPixels
	{
		TArray64<uint8> Data;
		int32 Size = 0;
		bool IsWater(int32 X, int32 Y) const
		{
			// Sample rotated artwork in world-grid image coordinates.
			const int64 Offset = (static_cast<int64>(Size - 1 - X) * Size + Y) * 4;
			return Data[Offset] > Data[Offset + 1]; // BGRA: blue dominates green.
		}
	};
	TMap<uint8, FPixels> Pixels;
	for (uint8 V = 1; V <= PSWaterAutoTile::VariantCount; ++V)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, PSWaterAutoTile::GetVariantTexturePath(V));
		TestNotNull(TEXT("All 69 canonical water assets resolve"), Texture);
		if (!Texture) continue;
		TestEqual(TEXT("Water tile width"), Texture->Source.GetSizeX(), int64(144));
		TestEqual(TEXT("Water tile height"), Texture->Source.GetSizeY(), int64(144));
		TestEqual(TEXT("Readable BGRA source"), Texture->Source.GetFormat(), TSF_BGRA8);
		if (Texture->Source.GetFormat() != TSF_BGRA8) continue;
		FPixels& P = Pixels.Add(V);
		P.Size = Texture->Source.GetSizeX();
		TestTrue(TEXT("Water source pixels readable"), Texture->Source.GetMipData(P.Data, 0));
	}
	TestNull(TEXT("Invalid water variant has no asset"), PSWaterAutoTile::GetVariantTexturePath(0));
	const uint16 AddedMasks[] = {398, 334, 283, 299, 295, 285, 327, 397, 287, 303, 399, 335, 351, 431, 267, 269, 263, 270};
	const FIntPoint SamplePoints[] = {{72,0}, {143,72}, {72,143}, {0,72},
		{0,0}, {143,0}, {143,143}, {0,143}, {72,72}};
	for (int32 I = 0; I < UE_ARRAY_COUNT(AddedMasks); ++I)
	{
		const uint8 Variant = 52 + I;
		uint16 WorldMask = AddedMasks[I];
		// Three inverse quarter turns convert source topology to world topology.
		for (int32 Turn = 0; Turn < 3; ++Turn) WorldMask = PSWaterAutoTile::ToTextureMask(WorldMask);
		for (uint32 Seed : {0u, 17u, 1337u})
			TestEqual(TEXT("New topology selects exact water piece after artwork rotation"),
				PSWaterAutoTile::SelectVariant(WorldMask, Seed), Variant);
		const FPixels* P = Pixels.Find(Variant);
		if (!P || P->Data.IsEmpty()) continue;
		uint16 SourceMask = 0;
		for (int32 Bit = 0; Bit < 9; ++Bit)
			if (P->IsWater(P->Size - 1 - SamplePoints[Bit].Y, SamplePoints[Bit].X)) SourceMask |= 1 << Bit;
		TestEqual(TEXT("Imported pixels agree with new connection rule"), SourceMask, AddedMasks[I]);
	}
	TestNull(TEXT("Invalid rotated stamp has no asset"),
		PSWaterAutoTile::GetVariantTexturePath(PSWaterAutoTile::GetFixedTextureVariant(0)));
	const uint16 WorldDirections[] = {PSDirtAutoTile::East, PSDirtAutoTile::South, PSDirtAutoTile::West,
		PSDirtAutoTile::North, PSDirtAutoTile::NorthEast, PSDirtAutoTile::SouthEast,
		PSDirtAutoTile::SouthWest, PSDirtAutoTile::NorthWest};
	for (int32 D = 0; D < 8; ++D)
		TestEqual(TEXT("Inverse rotation maps each cardinal/diagonal correctly"),
			PSWaterAutoTile::ToTextureMask(PSDirtAutoTile::Center | WorldDirections[D]), uint16(256 | (1 << D)));
	for (uint8 V = 1; V <= PSWaterAutoTile::VariantCount; ++V)
	{
		const uint8 Mapped = PSWaterAutoTile::GetFixedTextureVariant(V);
		TestEqual(TEXT("Fixed stamp rotation preserves its entire occupancy mask"),
			PSWaterAutoTile::GetVariantMask(Mapped),
			PSWaterAutoTile::ToTextureMask(PSWaterAutoTile::GetVariantMask(V)));
	}
	for (uint16 Mask = 256; Mask < 512; ++Mask)
	{
		const uint8 V = PSWaterAutoTile::SelectVariant(Mask, 0);
		TestNotNull(TEXT("Every water mask selects artwork"), PSWaterAutoTile::GetVariantTexturePath(V));
		TestEqual(TEXT("Fallback preserves center and cardinal connectivity"),
			PSWaterAutoTile::GetVariantMask(V) & 271, PSWaterAutoTile::ToTextureMask(Mask) & 271);
	}
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	APSGridWorld* Grid = World->SpawnActor<APSGridWorld>();
	const int32 N = Grid->ChunkSize;
	// Rectangle straddles both chunk axes: mask must sample world data, not local arrays.
	for (int32 CY = 0; CY < 2; ++CY)
		for (int32 CX = 0; CX < 2; ++CX)
		{
			FPSChunkData& Chunk = Grid->GetOrCreateChunk(FIntPoint(CX, CY));
			for (int32 Y = 0; Y < N; ++Y)
				for (int32 X = 0; X < N; ++X)
				{
					FPSTileCell& Cell = Chunk.Cells[Y * N + X];
					const int32 WX = CX * N + X, WY = CY * N + Y;
					Cell.GroundType = WX >= N - 3 && WX <= N + 2 && WY >= N - 3 && WY <= N + 2
						? EPSTileType::Water : EPSTileType::Grass;
					Cell.ObjectType = EPSWorldObjectType::None;
				}
		}
	const FIntPoint Center(N - 1, N - 1);
	TestEqual(TEXT("Water connects across chunk corner"), Grid->BuildTerrainNeighborMask(Center, EPSTileType::Water), uint16(511));
	const FIntPoint Offsets[] = { {0,-1}, {1,0}, {0,1}, {-1,0}, {-1,-1}, {1,-1}, {1,1}, {-1,1} };
	for (int32 D = 0; D < 8; ++D)
	{
		const FIntPoint C = Center + Offsets[D];
		FPSChunkData& Chunk = Grid->GetOrCreateChunk(PSGrid::CellToChunk(C, N));
		FPSTileCell& Cell = Chunk.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(C, N), N)];
		Cell.GroundType = EPSTileType::Grass;
		TestEqual(TEXT("Water mask uses correct UV direction"), Grid->BuildTerrainNeighborMask(Center, EPSTileType::Water),
			static_cast<uint16>(511 & ~(1 << D)));
		Cell.GroundType = EPSTileType::Water;
	}
	TMap<FIntPoint, uint8> Variants;
	for (int32 CY = 0; CY < 2; ++CY)
		for (int32 CX = 0; CX < 2; ++CX)
		{
			FPSChunkData& Source = Grid->GetOrCreateChunk(FIntPoint(CX, CY));
			const FPSChunkData Before = Grid->BuildRenderChunkData(FIntPoint(CX, CY), Source);
			for (FPSTileCell& Cell : Source.Cells) Cell.ObjectType = EPSWorldObjectType::Stone;
			const FPSChunkData After = Grid->BuildRenderChunkData(FIntPoint(CX, CY), Source);
			for (int32 Y = 0; Y < N; ++Y)
				for (int32 X = 0; X < N; ++X)
				{
					const int32 I = Y * N + X;
					TestEqual(TEXT("Objects cannot change water or grass visuals"), Before.Cells[I].Variant, After.Cells[I].Variant);
					Variants.Add(FIntPoint(CX * N + X, CY * N + Y), Before.Cells[I].Variant);
				}
		}
	// Validate actual image edges, including all four rounded corners.
	for (int32 Y = N - 4; Y <= N + 3; ++Y)
		for (int32 X = N - 4; X <= N + 3; ++X)
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				const FPixels* A = Pixels.Find(Variants.FindRef(FIntPoint(X, Y)));
				const FPixels* B = Pixels.Find(Variants.FindRef(FIntPoint(X + 1 - Axis, Y + Axis)));
				if (!A || !B || A->Data.IsEmpty() || B->Data.IsEmpty()) continue;
				int32 Mismatch = 0;
				for (int32 S = 0; S < 100; ++S)
					Mismatch += A->IsWater(Axis == 0 ? A->Size - 1 : S * A->Size / 100, Axis == 0 ? S * A->Size / 100 : A->Size - 1)
						!= B->IsWater(Axis == 0 ? 0 : S * B->Size / 100, Axis == 0 ? S * B->Size / 100 : 0);
				TestTrue(*FString::Printf(TEXT("Water shoreline seam (%d,%d), axis %d: %d%% mismatch"), X, Y, Axis, Mismatch), Mismatch <= 15);
			}
	if (FParse::Param(FCommandLine::Get(), TEXT("ExportTileVisuals")))
	{
		constexpr int32 CellPixels = 72, Size = 8 * CellPixels;
		TArray<FColor> Preview;
		Preview.Init(FColor::Magenta, Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
			for (int32 X = 0; X < Size; ++X)
			{
				// Export in the actual camera orientation (+X screen up, +Y right).
				const int32 WX = Size - 1 - Y, WY = X;
				const FPixels* P = Pixels.Find(Variants.FindRef(FIntPoint(N - 4 + WX / CellPixels, N - 4 + WY / CellPixels)));
				if (!P || P->Data.IsEmpty()) continue;
				const int32 SX = (WY % CellPixels) * P->Size / CellPixels;
				const int32 SY = P->Size - 1 - (WX % CellPixels) * P->Size / CellPixels;
				const int64 I = (static_cast<int64>(SY) * P->Size + SX) * 4;
				Preview[Y * Size + X] = FColor(P->Data[I+2], P->Data[I+1], P->Data[I], 255);
			}
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("TileDiagnostics");
		IFileManager::Get().MakeDirectory(*Dir, true);
		TestTrue(TEXT("Water preview exported"), FImageUtils::SaveImageByExtension(*(Dir / TEXT("WaterLake.png")), FImageView(Preview.GetData(), Size, Size)));
	}
	// Exercise all new topologies through real grid rendering, not only the selector.
	// Export in camera orientation so source-image shadows remain at screen top.
	constexpr int32 PreviewCell = 48, CaseSize = PreviewCell * 3, PreviewWidth = CaseSize * 4;
	TArray<FColor> MixedPreview;
	constexpr int32 CaseRows = (UE_ARRAY_COUNT(AddedMasks) + 3) / 4;
	MixedPreview.Init(FColor::Magenta, PreviewWidth * CaseSize * CaseRows);
	const FIntPoint MixedCenter(4, 4);
	for (int32 Case = 0; Case < UE_ARRAY_COUNT(AddedMasks); ++Case)
	{
		Grid->LoadedChunks.Reset();
		FPSChunkData& Chunk = Grid->GetOrCreateChunk(FIntPoint::ZeroValue);
		for (FPSTileCell& Cell : Chunk.Cells)
		{
			Cell = FPSTileCell();
			Cell.GroundType = EPSTileType::Grass;
		}
		uint16 WorldMask = AddedMasks[Case];
		for (int32 Turn = 0; Turn < 3; ++Turn) WorldMask = PSWaterAutoTile::ToTextureMask(WorldMask);
		Chunk.Cells[MixedCenter.Y * N + MixedCenter.X].GroundType = EPSTileType::Water;
		for (int32 D = 0; D < 8; ++D)
			if (WorldMask & (1 << D))
			{
				const FIntPoint C = MixedCenter + Offsets[D];
				Chunk.Cells[C.Y * N + C.X].GroundType = EPSTileType::Water;
			}
		const FPSChunkData Render = Grid->BuildRenderChunkData(FIntPoint::ZeroValue, Chunk);
		const uint8 V = Render.Cells[MixedCenter.Y * N + MixedCenter.X].Variant;
		TestEqual(TEXT("Grid uses new water topology"), V, uint8(52 + Case));
		const FPixels* A = Pixels.Find(V);
		for (int32 D = 0; D < 4; ++D)
		{
			const FIntPoint C = MixedCenter + Offsets[D];
			// Plain grass uses the family's grass pixels solely for seam comparison.
			const FPixels* B = Pixels.Find(Render.Cells[C.Y * N + C.X].Variant);
			if (!A || !B || A->Data.IsEmpty() || B->Data.IsEmpty()) continue;
			int32 Mismatch = 0;
			for (int32 S = 0; S < 144; ++S)
				Mismatch += A->IsWater(D == 1 ? 143 : D == 3 ? 0 : S, D == 0 ? 0 : D == 2 ? 143 : S)
					!= B->IsWater(D == 1 ? 0 : D == 3 ? 143 : S, D == 0 ? 143 : D == 2 ? 0 : S);
			TestTrue(*FString::Printf(TEXT("Water new case %d side %d seam mismatch %d/144"), Case, D, Mismatch), Mismatch <= 3);
		}
		for (int32 Y = 0; Y < CaseSize; ++Y)
			for (int32 X = 0; X < CaseSize; ++X)
			{
				const int32 WX = CaseSize - 1 - Y, WY = X;
				const FIntPoint C = MixedCenter - FIntPoint(1, 1) + FIntPoint(WX / PreviewCell, WY / PreviewCell);
				const FPixels* P = Pixels.Find(Render.Cells[C.Y * N + C.X].Variant);
				if (!P || P->Data.IsEmpty()) continue;
				const int32 SX = WY % PreviewCell * P->Size / PreviewCell;
				const int32 SY = P->Size - 1 - WX % PreviewCell * P->Size / PreviewCell;
				const int64 I = (static_cast<int64>(SY) * P->Size + SX) * 4;
				MixedPreview[((Case / 4) * CaseSize + Y) * PreviewWidth + (Case % 4) * CaseSize + X]
					= FColor(P->Data[I+2], P->Data[I+1], P->Data[I], 255);
			}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ExportTileVisuals")))
		TestTrue(TEXT("New water topology preview exported"), FImageUtils::SaveImageByExtension(
			*(FPaths::ProjectSavedDir() / TEXT("TileDiagnostics/WaterMixedCorners.png")),
			FImageView(MixedPreview.GetData(), PreviewWidth, CaseSize * CaseRows)));
	APSTileChunkActor* Renderer = World->SpawnActor<APSTileChunkActor>();
	FPSChunkData Single;
	Single.Cells.SetNum(1);
	Single.Cells[0].GroundType = EPSTileType::Water;
	Single.Cells[0].Variant = 23;
	Renderer->Rebuild(Single, 1, 100);
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Renderer->GetComponents(Components);
	UHierarchicalInstancedStaticMeshComponent* Water = nullptr;
	for (auto* C : Components) if (C->GetFName() == TEXT("WaterVariant_23")) Water = C;
	TestNotNull(TEXT("Water creates independent textured HISM"), Water);
	if (Water)
	{
		TestEqual(TEXT("Water rendered exactly once"), Water->GetInstanceCount(), 1);
		FTransform Instance;
		Water->GetInstanceTransform(0, Instance, false);
		const FVector ImageTop = Instance.GetRotation().RotateVector(FVector(0, -1, 0));
		const FVector ScreenUp = FRotator(-90, 0, 0).RotateVector(FVector::UpVector);
		TestTrue(TEXT("Water source shadow faces camera screen top"),
			FVector::DotProduct(ImageTop, ScreenUp) > 0.99);
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Water->GetMaterial(0));
		TestNotNull(TEXT("Water dynamic material"), Material);
		if (Material) TestNotNull(TEXT("Water SpriteTexture assigned"), Material->K2_GetTextureParameterValue(TEXT("SpriteTexture")));
		Renderer->Rebuild(Single, 1, 100);
		TestEqual(TEXT("Rebuild clears previous water instances"), Water->GetInstanceCount(), 1);
		Single.Cells[0].GroundType = EPSTileType::Grass;
		Single.Cells[0].Variant = 32;
		Renderer->Rebuild(Single, 1, 100);
		TestEqual(TEXT("Removed water leaves no stale visual"), Water->GetInstanceCount(), 0);
	}
	World->DestroyWorld(false);
	return true;
}
#endif
