#include "PSGatheringTreeActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APSGatheringTreeActor::APSGatheringTreeActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("TreePivot"));
	Pivot->SetupAttachment(RootComponent);
	Trunk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trunk"));
	Trunk->SetupAttachment(Pivot);
	Crown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crown"));
	Crown->SetupAttachment(Pivot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Trunk->SetStaticMesh(Cylinder.Object);
	Crown->SetStaticMesh(Sphere.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ColorMaterial(TEXT("/Game/Art/Gathering/M_GatheringColor.M_GatheringColor"));
	if (ColorMaterial.Succeeded())
	{
		Trunk->SetMaterial(0, ColorMaterial.Object);
		Crown->SetMaterial(0, ColorMaterial.Object);
	}
	Trunk->SetCastShadow(false);
	Crown->SetCastShadow(false);
	Trunk->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trunk->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Trunk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APSGatheringTreeActor::Configure(const FPSTreeRecord& Tree, const float CellSize)
{
	Size = CellSize;
	bStump = Tree.Health <= 0;
	const float Height = bStump ? 0.22f : 1.35f;
	Trunk->SetRelativeLocation(FVector(0, 0, Size * Height * 0.5f));
	Trunk->SetRelativeScale3D(FVector(Size / 100 * 0.3f, Size / 100 * 0.3f, Size / 100 * Height));
	Trunk->SetCollisionEnabled(bStump ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	Crown->SetRelativeLocation(FVector(0, 0, Size * 1.7f));
	Crown->SetRelativeScale3D(FVector(Size / 100 * 1.4f, Size / 100 * 1.4f, Size / 100 * 1.55f));
	Crown->SetVisibility(!bStump);
	if (UMaterialInstanceDynamic* Material = Trunk->CreateAndSetMaterialInstanceDynamic(0))
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.24f, 0.1f, 0.04f));
	if (UMaterialInstanceDynamic* Material = Crown->CreateAndSetMaterialInstanceDynamic(0))
		Material->SetVectorParameterValue(TEXT("Color"), Tree.Species == EPSTreeSpecies::Maple
			? FLinearColor(0.65f, 0.19f, 0.025f) : FLinearColor(0.1f, 0.4f, 0.06f));
}

void APSGatheringTreeActor::React(const bool bFalling)
{
	if (bFall || bStump) return;
	bFall = bFalling;
	AnimationTime = 0;
	if (bFall) Trunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(true);
}

void APSGatheringTreeActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AnimationTime += DeltaSeconds;
	const float Duration = bFall ? 0.6f : 0.35f;
	const float Alpha = FMath::Clamp(AnimationTime / Duration, 0.0f, 1.0f);
	Pivot->SetRelativeRotation(FRotator(bFall ? Alpha * 85 : FMath::Sin(Alpha * UE_PI * 4) * (1 - Alpha) * 7, 0, 0));
	if (Alpha < 1) return;
	Pivot->SetRelativeRotation(FRotator::ZeroRotator);
	if (bFall)
	{
		bStump = true;
		Crown->SetVisibility(false);
		Trunk->SetRelativeLocation(FVector(0, 0, Size * 0.11f));
		Trunk->SetRelativeScale3D(FVector(Size / 100 * 0.3f, Size / 100 * 0.3f, Size / 100 * 0.22f));
	}
	SetActorTickEnabled(false);
}
