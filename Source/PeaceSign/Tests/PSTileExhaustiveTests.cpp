#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "../World/PSDirtAutoTileSet.h"
#include "../World/PSWaterAutoTile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSTileExhaustiveTest, "PeaceSign.Grid.ExhaustiveTileDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSTileExhaustiveTest::RunTest(const FString& Parameters)
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("TileDiagnostics/Exhaustive");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FIntPoint Offsets[] = {{0,-1},{1,0},{0,1},{-1,0},{-1,-1},{1,-1},{1,1},{-1,1}};
	FString Summary = TEXT("family,raw_masks,normalized_land_masks,missing_exact_masks,pair_layouts,large_seam_layouts,unique_large_seams,max_mismatch_pixels\n");
	for (int32 Family = 0; Family < 2; ++Family)
	{
		const bool bWater = Family != 0;
		const FString Name = bWater ? TEXT("Water") : TEXT("Dirt");
		struct FTile
		{
			TArray64<uint8> Data;
			FColor Pixel(int32 X, int32 Y, bool Water) const
			{
				const int32 SX = Water ? Y : X, SY = Water ? 143 - X : Y;
				const int64 I = (static_cast<int64>(SY) * 144 + SX) * 4;
				return FColor(Data[I+2], Data[I+1], Data[I], 255);
			}
		};
		TArray<FTile> Tiles;
		Tiles.SetNum(70);
		bool bReadable = true;
		for (uint8 V = 1; V <= 69; ++V)
		{
			UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, bWater
				? PSWaterAutoTile::GetVariantTexturePath(V) : PSDirtAutoTile::GetDefaultVariantTexturePath(V));
			const bool bValid = Texture && Texture->Source.GetSizeX() == 144 && Texture->Source.GetSizeY() == 144
				&& Texture->Source.GetFormat() == TSF_BGRA8 && Texture->Source.GetMipData(Tiles[V].Data, 0);
			TestTrue(*FString::Printf(TEXT("%s source %d readable at expected size"), *Name, V), bValid);
			bReadable &= bValid;
		}
		if (!bReadable) continue;
		const auto Select = [bWater](uint16 Mask, uint32 Seed = 0u)
		{
			return bWater ? PSWaterAutoTile::SelectVariant(Mask, Seed) : PSDirtAutoTile::SelectDefaultVariant(Mask, Seed);
		};
		const auto Normalize = [](uint16 Mask)
		{
			uint16 N = Mask & 271;
			if ((Mask & 9) == 9) N |= Mask & 16;
			if ((Mask & 3) == 3) N |= Mask & 32;
			if ((Mask & 6) == 6) N |= Mask & 64;
			if ((Mask & 12) == 12) N |= Mask & 128;
			return N;
		};
		TSet<uint16> NormalizedMasks, Missing;
		FString Coverage = TEXT("world_mask,texture_mask,selected_variant,exact\n");
		for (uint16 Mask = 0; Mask < 512; ++Mask)
		{
			const uint16 Expected = Normalize(bWater ? PSWaterAutoTile::ToTextureMask(Mask) : Mask);
			for (uint32 Seed : {0u, 17u, 1337u})
			{
				const uint8 V = Select(Mask, Seed);
				if (!TestTrue(TEXT("All masks/seeds select a valid texture"), V > 0 && V <= 69)) continue;
				const uint16 Actual = bWater ? PSWaterAutoTile::GetVariantMask(V) : PSDirtAutoTile::GetDefaultVariantMask(V);
				if (Mask & 256) TestEqual(TEXT("Cardinal occupancy is preserved for every mask/seed"), Actual & 271, Expected & 271);
				if (Seed != 0) continue;
				const bool bExact = !(Mask & 256) || Actual == Expected;
				if (Mask & 256)
				{
					NormalizedMasks.Add(Expected);
					if (!bExact) Missing.Add(Expected);
				}
				Coverage += FString::Printf(TEXT("%u,%u,%u,%d\n"), Mask, Expected, V, bExact ? 1 : 0);
			}
		}
		// Enumerate a shared 4x3 neighborhood for horizontal pairs, 3x4 for
		// vertical pairs. This covers ALL 4096 mutually consistent layouts per axis.
		struct FSeam { int32 Count = 0; int32 Worst = 0; uint16 Layout = 0; };
		TMap<uint32, FSeam> Seams;
		int32 BadLayouts = 0, WorstMismatch = 0;
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			const int32 W = Axis == 0 ? 4 : 3, H = Axis == 0 ? 3 : 4;
			for (uint16 Layout = 0; Layout < 4096; ++Layout)
			{
				const auto Occupied = [Layout, W, H](int32 X, int32 Y)
				{
					return X >= 0 && Y >= 0 && X < W && Y < H && (Layout & (1 << (Y * W + X))) != 0;
				};
				const auto MaskAt = [&Occupied, &Offsets](int32 X, int32 Y)
				{
					uint16 M = Occupied(X, Y) ? 256 : 0;
					for (int32 D = 0; D < 8; ++D) if (Occupied(X + Offsets[D].X, Y + Offsets[D].Y)) M |= 1 << D;
					return M;
				};
				const uint8 A = Select(MaskAt(1, 1)), B = Select(MaskAt(2 - Axis, 1 + Axis));
				int32 Mismatch = 0;
				for (int32 S = 0; S < 144; ++S)
				{
					const FColor PA = Tiles[A].Pixel(Axis == 0 ? 143 : S, Axis == 0 ? S : 143, bWater);
					const FColor PB = Tiles[B].Pixel(Axis == 0 ? 0 : S, Axis == 0 ? S : 0, bWater);
					const bool LandA = bWater ? PA.B > PA.G : PA.R > PA.G;
					const bool LandB = bWater ? PB.B > PB.G : PB.R > PB.G;
					Mismatch += LandA != LandB;
				}
				WorstMismatch = FMath::Max(WorstMismatch, Mismatch);
				if (Mismatch <= 3) continue; // Allow three antialiased boundary pixels.
				++BadLayouts;
				FSeam& Seam = Seams.FindOrAdd((Axis << 16) | (A << 8) | B);
				++Seam.Count;
				if (Mismatch > Seam.Worst) { Seam.Worst = Mismatch; Seam.Layout = Layout; }
			}
		}
		TArray<uint32> Keys; Seams.GetKeys(Keys);
		Keys.Sort([&Seams](uint32 A, uint32 B) { return Seams[A].Worst != Seams[B].Worst ? Seams[A].Worst > Seams[B].Worst : A < B; });
		FString SeamCsv = TEXT("axis,variant_a,variant_b,layout_count,worst_mismatch_pixels,example_layout\n");
		constexpr int32 CellPixels = 48, Panel = CellPixels * 4, Columns = 4;
		const int32 Cases = FMath::Min(Keys.Num(), 16), Rows = FMath::Max(1, (Cases + Columns - 1) / Columns);
		TArray<FColor> Sheet; Sheet.Init(FColor(80, 80, 80), Panel * Columns * Panel * Rows);
		for (int32 I = 0; I < Keys.Num(); ++I)
		{
			const uint32 Key = Keys[I]; const FSeam& Seam = Seams[Key];
			const int32 Axis = Key >> 16, W = Axis == 0 ? 4 : 3, H = Axis == 0 ? 3 : 4;
			SeamCsv += FString::Printf(TEXT("%d,%d,%d,%d,%d,%u\n"), Axis, (Key >> 8) & 255, Key & 255, Seam.Count, Seam.Worst, Seam.Layout);
			if (I >= Cases) continue;
			for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X)
			{
				uint16 Mask = (Seam.Layout & (1 << (Y * W + X))) ? 256 : 0;
				for (int32 D = 0; D < 8; ++D)
				{
					const int32 NX = X + Offsets[D].X, NY = Y + Offsets[D].Y;
					if (NX >= 0 && NY >= 0 && NX < W && NY < H && (Seam.Layout & (1 << (NY * W + NX)))) Mask |= 1 << D;
				}
				const uint8 V = Select(Mask);
				for (int32 PY = 0; PY < CellPixels; ++PY) for (int32 PX = 0; PX < CellPixels; ++PX)
				{
					const int32 OutX = (I % Columns) * Panel + X * CellPixels + PX;
					const int32 OutY = (I / Columns) * Panel + Y * CellPixels + PY;
					Sheet[OutY * Panel * Columns + OutX] = Tiles[V].Pixel(PX * 3, PY * 3, bWater);
				}
			}
			// Mark only the shared edge being audited, not surrounding edges.
			for (int32 S = 0; S < CellPixels; ++S)
			{
				const int32 X = I % Columns * Panel + (Axis == 0 ? CellPixels * 2 : CellPixels + S);
				const int32 Y = I / Columns * Panel + (Axis == 0 ? CellPixels + S : CellPixels * 2);
				Sheet[Y * Panel * Columns + X] = FColor::Red;
			}
		}
		TArray<uint16> MissingList = Missing.Array(); MissingList.Sort();
		FString MissingText;
		for (uint16 M : MissingList) MissingText += FString::Printf(TEXT("%u "), M);
		AddInfo(FString::Printf(TEXT("%s: %d normalized land masks, missing [%s], %d/8192 layouts exceed 3-pixel seam tolerance; %d unique pairs; worst %d/144 pixels"),
			*Name, NormalizedMasks.Num(), *MissingText, BadLayouts, Seams.Num(), WorstMismatch));
		Summary += FString::Printf(TEXT("%s,512,%d,%d,8192,%d,%d,%d\n"), *Name, NormalizedMasks.Num(), Missing.Num(), BadLayouts, Seams.Num(), WorstMismatch);
		TestTrue(TEXT("Coverage CSV exported"), FFileHelper::SaveStringToFile(Coverage, *(Dir / (Name + TEXT("Coverage.csv")))));
		TestTrue(TEXT("Seam CSV exported"), FFileHelper::SaveStringToFile(SeamCsv, *(Dir / (Name + TEXT("Seams.csv")))));
		TestTrue(TEXT("Diagnostic sheet exported"), FImageUtils::SaveImageByExtension(*(Dir / (Name + TEXT("Seams.png"))), FImageView(Sheet.GetData(), Panel * Columns, Panel * Rows)));
	}
	TestTrue(TEXT("Diagnostic summary exported"), FFileHelper::SaveStringToFile(Summary, *(Dir / TEXT("Summary.csv"))));
	// A passing diagnostic means the sweep completed, not that every visual seam passed.
	return true;
}
#endif
