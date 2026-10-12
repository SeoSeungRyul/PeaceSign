#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../World/PSTileStamp.h"
#include "../World/PSTileSetBoard.h"
#include "../World/PSDirtAutoTileSet.h"
#include "../World/PSWaterAutoTile.h"
#include "../World/PSGridWorld.h"
#include "../World/PSWorldSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSTileStampTest, "PeaceSign.Grid.TileStamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSTileStampTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	APSGridWorld* Grid = World->SpawnActor<APSGridWorld>();
	Grid->SaveSlotName = TEXT("TileStampTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	for (int32 Family = 0; Family < 2; ++Family)
		for (int32 Entry = 0; Entry < 12; ++Entry)
		{
			Grid->TileStamps.Reset();
			Grid->LoadedChunks.Reset();
			Grid->ModifiedChunks.Reset(); // Each layout case represents a fresh, unvisited world.
			FPSTileStamp Stamp;
			Stamp.Set = Entry < 7 ? FMath::Min(Entry + 1, 6) : Entry;
			Stamp.bVertical = Entry == 6;
			Stamp.bWater = Family != 0;
			Stamp.Origin = FIntPoint(15, -1); // Both positive and negative chunk seams.
			Grid->TileStamps.Add(Stamp);
			const FIntPoint Size = PSTileStamp::GetSize(Stamp);
			for (int32 Y = 0; Y < Size.Y; ++Y)
				for (int32 X = 0; X < Size.X; ++X)
				{
					const FIntPoint Cell = Stamp.Origin + FIntPoint(X, Y);
					FPSTileCell Expected;
					if (Stamp.Set == 11 && !((X == 1 && Y != 1) || (Y == 1 && X != 1)))
					{
						Expected.Variant = 42;
						TestFalse(TEXT("Sparse cross leaves empty positions untouched"), PSTileStamp::BuildCell(Stamp, Cell, Expected));
						TestEqual(TEXT("Cross holes do not modify output"), Expected.Variant, uint8(42));
						continue;
					}
					TestTrue(TEXT("Source piece built"), PSTileStamp::BuildCell(Stamp, Cell, Expected));
					const uint8 ExpectedVariant = Stamp.Set == 11 ? (Y == 0 ? 66 : Y == 2 ? 69 : X == 0 ? 67 : 68)
						: Stamp.Set == 10 ? 64 + X
						: Entry >= 7 ? 52 + (Entry - 7) * 4 + Y * 2 + X
						: Entry < 5 ? Entry * 9 + Y * 3 + X + 1
						: (Entry == 5 ? 46 + X : 49 + Y);
					TestEqual(TEXT("Original row-major source layout"), Expected.Variant, ExpectedVariant);
					TestEqual(TEXT("Unloaded ground lookup matches stamped terrain"), Grid->GetGroundTile(Cell), Expected.GroundType);
					const FIntPoint Chunk = PSGrid::CellToChunk(Cell, Grid->ChunkSize);
					const FPSChunkData Render = Grid->BuildRenderChunkData(Chunk, Grid->GetOrCreateChunk(Chunk));
					const FPSTileCell& Actual = Render.Cells[PSGrid::LocalToIndex(PSGrid::CellToLocal(Cell, Grid->ChunkSize), Grid->ChunkSize)];
					TestEqual(TEXT("Auto-tiling cannot replace stamped piece"), Actual.Variant, Expected.Variant);
					TestEqual(TEXT("Grass-centered water pieces keep water artwork"), Actual.bWaterArtwork, Stamp.bWater);
					TestTrue(TEXT("Stamp remains fixed"), Actual.bFixedVariant);
					TestEqual(TEXT("Stamp contains no procedural stones"), Actual.ObjectType, EPSWorldObjectType::None);
				}
		}
	FPSTileStamp Invalid; Invalid.Set = 12;
	FPSTileCell Unchanged; Unchanged.Variant = 42;
	TestFalse(TEXT("Invalid set rejected"), PSTileStamp::BuildCell(Invalid, FIntPoint::ZeroValue, Unchanged));
	Invalid.bWater = true;
	TestFalse(TEXT("Water Set12 not available"), PSTileStamp::BuildCell(Invalid, FIntPoint::ZeroValue, Unchanged));
	TestEqual(TEXT("Failed lookup does not touch output"), Unchanged.Variant, uint8(42));

	// Overlap order is stable. Player modifications saved later take precedence.
	Grid->TileStamps.Reset(); Grid->LoadedChunks.Reset();
	Grid->ModifiedChunks.Reset();
	FPSTileStamp Dirt; Dirt.Set = 2; Dirt.Origin = FIntPoint(15, -1);
	FPSTileStamp Water = Dirt; Water.bWater = true;
	Grid->TileStamps = {Dirt, Water};
	FPSTileCell Overlap;
	TestTrue(TEXT("Overlap resolved"), Grid->BuildStampedCell(Dirt.Origin, Overlap));
	TestTrue(TEXT("Last stamp wins"), Overlap.bWaterArtwork);
	const FIntPoint Center = Dirt.Origin + FIntPoint(1, 1);
	TestEqual(TEXT("Set2 MM remains a grass-centered island"), Grid->GetGroundTile(Center), EPSTileType::Grass);
	TestTrue(TEXT("Player can convert stamped grass to dirt"), Grid->SetGroundTile(Center, EPSTileType::Dirt));
	const FIntPoint CC = PSGrid::CellToChunk(Center, Grid->ChunkSize);
	const int32 Index = PSGrid::LocalToIndex(PSGrid::CellToLocal(Center, Grid->ChunkSize), Grid->ChunkSize);
	TestFalse(TEXT("Terrain editing releases stamp lock"), Grid->GetOrCreateChunk(CC).Cells[Index].bFixedVariant);
	Grid->LoadedChunks.Reset();
	TestEqual(TEXT("Saved gameplay edits override authored stamps"), Grid->GetGroundTile(Center), EPSTileType::Dirt);

	UPSWorldSaveGame* Save = NewObject<UPSWorldSaveGame>();
	FPSChunkSaveData Saved;
	Saved.Cells = {Overlap};
	Save->ModifiedChunks.Add(Saved);
	TArray<uint8> Bytes;
	TestTrue(TEXT("Stamp serializes with saves"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	UPSWorldSaveGame* Restored = Cast<UPSWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("Stamp save restored"), Restored);
	if (Restored && Restored->ModifiedChunks.Num() == 1 && Restored->ModifiedChunks[0].Cells.Num() == 1)
	{
		const FPSTileCell& Cell = Restored->ModifiedChunks[0].Cells[0];
		TestTrue(TEXT("Fixed piece flag persisted"), Cell.bFixedVariant);
		TestTrue(TEXT("Water artwork flag persisted"), Cell.bWaterArtwork);
		TestEqual(TEXT("Exact variant persisted"), Cell.Variant, Overlap.Variant);
	}

	APSTileSetBoard* Board = World->SpawnActor<APSTileSetBoard>();
	Board->RefreshBoard();
	TestTrue(TEXT("Board excluded from packaged gameplay"), Board->IsEditorOnly());
	TestFalse(TEXT("Board does not block gameplay"), Board->GetActorEnableCollision());
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Board->GetComponents(Components);
	int32 Count = 0;
	for (auto* C : Components)
	{
		Count += C->GetInstanceCount();
		const FString Name = C->GetName();
		if (C->GetInstanceCount() != 1) continue;
		const bool bWater = Name.StartsWith(TEXT("WaterVariant_"));
		const int32 Key = FCString::Atoi(*Name.RightChop(bWater ? 13 : 12));
		const int32 V = bWater ? Key : Key - 256;
		if (V < 1 || V > (bWater ? PSWaterAutoTile::VariantCount : PSDirtAutoTile::VariantCount)) continue;
		auto* Material = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
		TestNotNull(TEXT("Board has dynamic texture material"), Material);
		if (Material)
		{
			UTexture* Expected = LoadObject<UTexture2D>(nullptr, bWater
				? PSWaterAutoTile::GetVariantTexturePath(V) : PSDirtAutoTile::GetDefaultVariantTexturePath(V));
			TestTrue(TEXT("Board draws exact source texture"), Material->K2_GetTextureParameterValue(TEXT("SpriteTexture")) == Expected);
		}
	}
	TestEqual(TEXT("Board displays all 69 dirt and 69 water pieces exactly once"), Count, 138);
	Board->RefreshBoard();
	Count = 0;
	Board->GetComponents(Components);
	for (auto* C : Components) Count += C->GetInstanceCount();
	TestEqual(TEXT("Board refresh does not duplicate pieces"), Count, 138);
	UGameplayStatics::DeleteGameInSlot(Grid->SaveSlotName, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
