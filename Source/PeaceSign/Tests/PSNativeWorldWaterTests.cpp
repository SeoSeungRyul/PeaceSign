#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Components/ChildActorComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "WaterBodyCustomActor.h"
#include "WaterBodyComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "../World/PSNativeWaterBackdropComponent.h"
#include "../World/PSTileChunkActor.h"
#include "../World/PSGridWorld.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSNativeWorldWaterTest,"PeaceSign.Grid.NativeWorldWater",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPSNativeWorldWaterTest::RunTest(const FString& Parameters)
{
	UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if(!TestNotNull(TEXT("Editor world"),World)) return false;
	auto* Renderer=World->SpawnActor<APSTileChunkActor>(); Renderer->SetActorLocation(FVector(50000,50000,0));
	FPSChunkData Data; Data.Cells.SetNum(16);
	for(auto& Cell:Data.Cells) { Cell.GroundType=EPSTileType::Grass; Cell.Variant=32; }
	Data.Cells[5].GroundType=EPSTileType::Water; Data.Cells[5].Variant=19;
	Data.Cells[6].GroundType=EPSTileType::Water; Data.Cells[6].Variant=21;
	const auto Original=Data;
	Renderer->Rebuild(Data,4,100,nullptr,true);
	auto* Child=Renderer->FindComponentByClass<UChildActorComponent>();
	auto* Body=Child ? Cast<AWaterBodyCustom>(Child->GetChildActor()) : nullptr;
	if(!TestNotNull(TEXT("Water chunk creates native body"),Body)) { Renderer->Destroy(); return false; }
	TestFalse(TEXT("Native visual creates no gameplay collision"),Body->GetActorEnableCollision());
	TestTrue(TEXT("Native body is placed at chunk water bounds"),Body->GetActorLocation().Equals(FVector(50200,50150,-19),.1));
	TestNotNull(TEXT("Native waves are assigned"),Body->GetWaterWaves());
	Renderer->Rebuild(Data,4,100,nullptr,true);
	TestTrue(TEXT("Rebuild reuses body rather than spawning duplicates"),Child->GetChildActor()==Body);
	Renderer->SetActorHiddenInGame(true); TestTrue(TEXT("Hidden editor preview also hides native water"),Body->IsHidden());
	Renderer->SetActorHiddenInGame(false); TestFalse(TEXT("Native visibility restored"),Body->IsHidden());
	for(int32 I=0;I<Data.Cells.Num();++I)
	{
		TestEqual(TEXT("Terrain remains untouched"),Data.Cells[I].GroundType,Original.Cells[I].GroundType);
		TestEqual(TEXT("Auto-tile variant remains untouched"),Data.Cells[I].Variant,Original.Cells[I].Variant);
	}
	Renderer->Rebuild(Data,4,100,nullptr,false);
	TestFalse(TEXT("Legacy switch destroys owned native body"),IsValid(Body));
	TArray<UHierarchicalInstancedStaticMeshComponent*> Components; Renderer->GetComponents(Components);
	for(auto* C:Components) if(C->GetName()==TEXT("WaterVariant_19"))
	{
		auto* MID=Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
		TestTrue(TEXT("Legacy water material is restored"),MID && MID->Parent==LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Tiles/Water/MI_WaterSurface")));
	}
	for(auto& Cell:Data.Cells) { Cell.GroundType=EPSTileType::Empty; Cell.Variant=0; }
	Data.Cells[0].GroundType=EPSTileType::Water; Data.Cells[0].Variant=5;
	Data.Cells[1].GroundType=Data.Cells[4].GroundType=Data.Cells[5].GroundType=EPSTileType::Grass;
	Data.Cells[1].Variant=Data.Cells[4].Variant=Data.Cells[5].Variant=32;
	Renderer->Rebuild(Data,4,100,nullptr,true);
	Child=Renderer->FindComponentByClass<UChildActorComponent>();
	Body=Child ? Cast<AWaterBodyCustom>(Child->GetChildActor()) : nullptr;
	if(TestNotNull(TEXT("Partial border chunk native body"),Body))
		TestTrue(TEXT("Water rectangle is clamped to valid world cells"),Body->GetActorLocation().Equals(FVector(50100,50100,-19),.1));
	Data.Cells[0].GroundType=EPSTileType::Grass; Data.Cells[0].Variant=32;
	Renderer->Rebuild(Data,4,100,nullptr,true);
	TestFalse(TEXT("Removing last water cell removes native body"),IsValid(Body));
	Renderer->Destroy();
	auto* Grid=World->SpawnActor<APSGridWorld>();
	TestTrue(TEXT("Existing worlds default to native water rendering"),Grid->bUseNativeWater);
	// Read procedural terrain only: no save slot or ground data is modified.
	FIntPoint WaterCell=FIntPoint::ZeroValue; bool bFound=false;
	for(int32 Y=-24;Y<=24 && !bFound;++Y) for(int32 X=-24;X<=24;++X)
		if(Grid->GetGroundTile(FIntPoint(X,Y))==EPSTileType::Water) { WaterCell=FIntPoint(X,Y); bFound=true; break; }
	TestTrue(TEXT("Generated world contains water"),bFound);
	const auto Ground=Grid->CellToWorldCenter(WaterCell);
	const auto Water=Grid->WaterVisualLocation(WaterCell);
	TestTrue(TEXT("Fishing visual follows lowered water only"),Water.Z<Ground.Z && Water.X==Ground.X && Water.Y==Ground.Y);
	Grid->bUseNativeWater=false;
	TestTrue(TEXT("Fishing visual restores legacy height"),Grid->WaterVisualLocation(WaterCell).Equals(Ground));
	TestEqual(TEXT("Water gameplay terrain unchanged"),Grid->GetGroundTile(WaterCell),EPSTileType::Water);
	auto* Floor=World->SpawnActor<AStaticMeshActor>();
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/MapTemplates/SM_Template_Map_Floor")));
	Floor->GetStaticMeshComponent()->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/OpenWorldTemplate/LandscapeMaterial/MI_ProcGrid")));
	const auto Collision=Floor->GetStaticMeshComponent()->GetCollisionEnabled();
	auto* Backdrop=NewObject<UPSNativeWaterBackdropComponent>(Grid); Backdrop->RegisterComponent();
	Backdrop->Update(true);
	TestTrue(TEXT("Only template backdrop is temporarily hidden"),Floor->IsTemporarilyHiddenInEditor());
	TestEqual(TEXT("Backdrop collision is untouched"),Floor->GetStaticMeshComponent()->GetCollisionEnabled(),Collision);
	Backdrop->Update(false);
	TestFalse(TEXT("Backdrop visibility restores with legacy mode"),Floor->IsTemporarilyHiddenInEditor());
	Floor->Destroy();
	Grid->Destroy();
	return true;
}
#endif
