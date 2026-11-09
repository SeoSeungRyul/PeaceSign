#include "PSLinkWaterComparison.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "WaterBodyCustomActor.h"
#include "WaterBodyComponent.h"
#include "WaterWaves.h"
#include "GerstnerWaterWaves.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "PSTileStamp.h"

APSLinkWaterComparison::APSLinkWaterComparison()
{
	NativeWater = CreateDefaultSubobject<UChildActorComponent>(TEXT("NativeWater"));
	NativeWater->SetupAttachment(GetRootComponent());
	NativeWater->SetChildActorClass(AWaterBodyCustom::StaticClass());
	LegacyWaterMaterial = WaterSurfaceMaterial;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shore(TEXT("/Game/Art/Tiles/Water/LinkPrototype/M_LinkWaterShore"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Water(TEXT("/Game/Art/Tiles/Water/LinkPrototype/MI_LinkWater"));
	static ConstructorHelpers::FObjectFinder<UWaterWavesAsset> Wave(TEXT("/Game/Art/Tiles/Water/LinkPrototype/WW_LinkWater"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Water/Meshes/S_WaterPlane_256"));
	ShoreMaterial = Shore.Object; NativeWaterMaterial = Water.Object; Waves = Wave.Object; WaterMesh = Mesh.Object;
}

void APSLinkWaterComparison::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshComparison();
}

void APSLinkWaterComparison::BeginPlay()
{
	Super::BeginPlay();
	RefreshComparison(); // Rebuild transient HISM components in loaded game maps.
}

void APSLinkWaterComparison::RefreshComparison()
{
	if (!GetWorld()) return;
	if (Waves) if (auto* Gerstner = Cast<UGerstnerWaterWaves>(Waves->GetWaterWaves())) Gerstner->RecomputeWaves(true);
	const bool bAssetsReady = ShoreMaterial && NativeWaterMaterial && WaterMesh && Waves && Waves->GetWaterWaves();
	const bool bHeightSafe = bAssetsReady && Waves->GetWaterWaves()->GetMaxWaveHeight() < 18;
	const bool bReady = bUseLinkWater && bAssetsReady && bHeightSafe;
	PreviewStatus = !bUseLinkWater ? TEXT("Legacy water") : !bAssetsReady ? TEXT("Missing native water assets; using legacy")
		: !bHeightSafe ? TEXT("Wave height >=18cm exceeds preview rim; lower wave amplitudes") : TEXT("Native Water Body Custom active");
	SetWaterSurfaceOverride(bReady ? ShoreMaterial.Get() : LegacyWaterMaterial.Get());
	// Upper artwork is a fixed rim 20cm above the water. Small preview waves
	// remain below it, so the rectangular native plane cannot cover grass.
	RenderZOffset = 20;
	FPSChunkData Pond; Pond.Cells.SetNum(25);
	for (auto& Cell : Pond.Cells) { Cell.GroundType = EPSTileType::Grass; Cell.Variant = 32; }
	FPSTileStamp Stamp; Stamp.Set = 3; Stamp.bWater = true; Stamp.Origin = FIntPoint(1, 1);
	for (int32 Y = 1; Y <= 3; ++Y) for (int32 X = 1; X <= 3; ++X)
		PSTileStamp::BuildCell(Stamp, FIntPoint(X,Y), Pond.Cells[Y * 5 + X]);
	Rebuild(Pond, 5, 100);
	SetActorEnableCollision(false);
	auto* Body = Cast<AWaterBodyCustom>(NativeWater->GetChildActor());
	if (!Body) return;
	Body->SetActorHiddenInGame(!bReady);
#if WITH_EDITOR
	Body->SetIsTemporarilyHiddenInEditor(!bReady);
#endif
	if (!bReady) return;
	auto* Component = Body->GetWaterBodyComponent();
	Component->SetMobility(EComponentMobility::Movable);
	const FVector Extent = WaterMesh->GetBounds().BoxExtent * 2;
	NativeWater->SetRelativeLocation(FVector(250, 250, 0));
	// Extend below the opaque rim to avoid exposing the background in oblique views.
	NativeWater->SetRelativeScale3D(FVector(400 / FMath::Max(1.0, Extent.X), 400 / FMath::Max(1.0, Extent.Y), 1));
	Body->SetActorTransform(NativeWater->GetComponentTransform());
	Component->SetWaterMeshOverride(WaterMesh);
	Component->SetWaterMaterial(NativeWaterMaterial);
	auto* Reference = NewObject<UWaterWavesAssetReference>(Body);
	Reference->SetWaterWavesAsset(Waves);
	Body->SetWaterWaves(Reference);
	FOnWaterBodyChangedParams Changed;
	Changed.bShapeOrPositionChanged = true;
	Component->UpdateAll(Changed);
	for (auto* MeshComponent : Component->GetStandardRenderableComponents())
	{
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetWorldTransform(Body->GetActorTransform());
	}
	UE_LOG(LogTemp, Display, TEXT("LINK_WATER_TRANSFORM body=%s expected=%s"), *Body->GetActorLocation().ToString(), *NativeWater->GetComponentLocation().ToString());
	Body->SetActorEnableCollision(false);
}
