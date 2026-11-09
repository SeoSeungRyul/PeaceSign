#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../Gathering/PSGatheringTreeActor.h"
#include "../World/PSGridWorld.h"
#include "../Inventory/PSInventoryComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSPlacedTreeSpriteTest, "PeaceSign.Gathering.PlacedSpritesAndInteractions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSPlacedTreeSpriteTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	const FString Slot = TEXT("TreeSpriteTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Grid = World->SpawnActorDeferred<APSGridWorld>(APSGridWorld::StaticClass(), FTransform::Identity);
	Grid->SaveSlotName = Slot; Grid->FinishSpawning(FTransform::Identity);
	auto* Gathering = Grid->Gathering.Get();
	Gathering->bEnableTestArea = false;
	Gathering->ItemTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataBase/DT_Item"));
	Gathering->TreeCutDropTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataBase/DT_TreeCutDrop"));
	Gathering->TreeShakeDropTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataBase/DT_TreeShakeDrop"));
	const TCHAR* Names[] = {TEXT("Oak"),TEXT("Maple"),TEXT("Cherry"),TEXT("Palm"),TEXT("Cactus"),TEXT("Birch")};
	const EPSTreeSpecies Species[] = {EPSTreeSpecies::Oak,EPSTreeSpecies::Maple,EPSTreeSpecies::Cherry,EPSTreeSpecies::Palm,EPSTreeSpecies::Cactus,EPSTreeSpecies::Birch};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		FPSTreeRecord Record; Record.Cell = FIntPoint(-4-(Index/3)*5, -7+(Index%3)*5);
		Record.Species = Species[Index]; Record.PlacementId = FName(*FString::Printf(TEXT("Test.%s"),Names[Index]));
		Gathering->PlacedTrees.Add(Record);
	}
	Grid->SetGroundTile(Gathering->PlacedTrees[0].Cell, EPSTileType::Water);
	Gathering->Restore({}, {}, true, true); // An already initialized old save still receives authored additions.
	TestEqual(TEXT("All six species are added to existing saves"), Gathering->EnsurePlacedTrees(), 6);
	TestEqual(TEXT("Authored placements are idempotent"), Gathering->EnsurePlacedTrees(), 0);
	TestEqual(TEXT("Existing water is never overwritten"), Grid->GetGroundTile(Gathering->PlacedTrees[0].Cell), EPSTileType::Water);
	const auto Records = Gathering->GetRecords();
	auto* Inventory = NewObject<UPSInventoryComponent>(World); Inventory->bAutoSave = false;
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const auto& Record = Records[Index];
		const FString Name = Record.PlacementId.ToString().RightChop(5);
		auto* Standing = LoadObject<UPaperSprite>(nullptr, *FString::Printf(TEXT("/Game/Art/Gathering/Trees/S_Tree_%s"), *Name));
		if (!TestNotNull(TEXT("Standing art loads"), Standing)) continue;
#if WITH_EDITOR
		TestEqual(TEXT("Separate source texture is 256 square"), Standing->GetSourceTexture()->GetImportedSize(), FIntPoint(256,256));
		TestTrue(TEXT("Bottom trunk pivot is shared across all sprites"), Standing->GetPivotPosition().Equals(FVector2D(128,240),.1));
#endif
		// Check terrain access before creating the isolated visual fixture. Runtime
		// actors are registered in Gathering so CanReach ignores the target's collider.
		bool bCanApproach = false;
		for (const FIntPoint Delta : {FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
			bCanApproach |= Grid->GetWorldObjectType(Record.Cell+Delta)==EPSWorldObjectType::None && Gathering->CanReach(Record.Cell+Delta,Record.Cell);
		TestTrue(TEXT("Every placed tree can be approached for interaction"), bCanApproach);
		auto* Actor = World->SpawnActor<APSGatheringTreeActor>(Grid->CellToWorldCenter(Record.Cell), FRotator::ZeroRotator);
		Actor->Configure(Record, Grid->GetCellSize(), Standing);
		auto* Visual = Actor->FindComponentByClass<UPaperSpriteComponent>();
		TestEqual(TEXT("Species art is assigned to the visual"), Visual->GetSprite(), Standing);
		TestTrue(TEXT("Sprite up points towards screen up"), FVector::DotProduct(Visual->GetComponentTransform().TransformVectorNoScale(FVector::UpVector),FVector::ForwardVector)>.99f);
		TestEqual(TEXT("Foliage does not block character movement"), Visual->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestTrue(TEXT("Foliage draws above ground props"), Visual->GetRelativeLocation().Z>Grid->GetCellSize()*.6f);
		Inventory->ResetToDefaults();
		TestTrue(TEXT("Placed tree supports shaking"), Gathering->Shake(Record.Cell));
		TestTrue(TEXT("Shaking starts the normal cooldown"), Gathering->GetTree(Record.Cell).NextShakeHalfHour>Gathering->Now());
		TestTrue(TEXT("Placed tree supports felling"), Gathering->Strike(Record.Cell, PSGathering::GetAxe(PSGathering::AxeId(9))));
		TestFalse(TEXT("Felled tree is removed from the saved live trees"), Gathering->HasTree(Record.Cell));
		const auto Drops = Gathering->GetDrops();
		for (const auto& Drop : Drops) TestTrue(TEXT("New art rewards remain collectible"), Gathering->CollectDrop(Drop.Id, Inventory));
		TestTrue(TEXT("Felling grants wood into inventory"), Inventory->CountItem(EPSItemType::Wood)>=3);
		Actor->React(true); Actor->Tick(.3f);
		TestFalse(TEXT("Tree stays visible during the fall"), Actor->IsActorBeingDestroyed());
		Actor->Tick(.31f);
		TestTrue(TEXT("Tree visual is removed at the end of the fall"), Actor->IsActorBeingDestroyed() || !IsValid(Actor));
	}
	Grid->SaveWorld(); Grid->LoadWorld();
	Gathering->InitializeResources();
	TestEqual(TEXT("Reload does not respawn felled authored trees"), Gathering->GetRecords().Num(), 0);
	TestEqual(TEXT("Removed placement IDs survive reload"), Gathering->GetRemovedPlacements().Num(), 6);
	auto LegacyStump = Records[0];
	LegacyStump.Health = 0;
	Gathering->Restore({LegacyStump}, {}, true, true);
	TestFalse(TEXT("Legacy stump no longer occupies its tile"), Gathering->OccupiesTreeCell(LegacyStump.Cell));
	TestTrue(TEXT("Legacy stump still suppresses its authored placement"), Gathering->GetRemovedPlacements().Contains(LegacyStump.PlacementId));
	World->DestroyWorld(false); UGameplayStatics::DeleteGameInSlot(Slot,0);
	return true;
}
#endif
