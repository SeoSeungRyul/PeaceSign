#include "PSGatheringTreeActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "SpriteEditorOnlyTypes.h"
#include "TextureCompiler.h"
#include "Misc/PackageName.h"
#endif

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
	TreeSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("TreeSprite"));
	TreeSprite->SetupAttachment(Pivot);
	// Sprite horizontal = world Y, sprite up = world X in the orthographic view.
	TreeSprite->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector::RightVector, FVector::ForwardVector).Rotator());
	TreeSprite->SetRelativeLocation(FVector(0,0,30));
	TreeSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TreeSprite->SetCastShadow(false);
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

void APSGatheringTreeActor::Configure(const FPSTreeRecord& Tree, const float CellSize, UPaperSprite* StandingSprite, const float HeightInCells)
{
	const float GrowthScale = Tree.IsSapling() ? 0.27f : 1.0f;
	const float Height = 1.35f * GrowthScale;
	Trunk->SetRelativeLocation(FVector(0, 0, CellSize * Height * 0.5f));
	Trunk->SetRelativeScale3D(FVector(0.3f, 0.3f, Height) * (CellSize / 100));
	Crown->SetRelativeLocation(FVector(0, 0, CellSize * 1.7f * GrowthScale));
	Crown->SetRelativeScale3D(FVector(1.4f, 1.4f, 1.55f) * (CellSize / 100 * GrowthScale));
	if (StandingSprite)
	{
		Trunk->SetVisibility(false); Crown->SetVisibility(false);
		// Above terrain props (top <= 0.6 cells), with stable depth between overlapping trees.
		const float Depth = 0.8f - 0.05f * FMath::Atan((Tree.Cell.X + Tree.Cell.Y * .01f) / 128.f);
		TreeSprite->SetRelativeLocation(FVector(0,0,CellSize * Depth));
		TreeSprite->SetRelativeScale3D(FVector(CellSize * GrowthScale * FMath::Max(0.5f, HeightInCells) / 224.0f));
		TreeSprite->SetSprite(StandingSprite);
	}
	if (UMaterialInstanceDynamic* Material = Trunk->CreateAndSetMaterialInstanceDynamic(0))
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.24f, 0.1f, 0.04f));
	if (UMaterialInstanceDynamic* Material = Crown->CreateAndSetMaterialInstanceDynamic(0))
		Material->SetVectorParameterValue(TEXT("Color"), Tree.Species == EPSTreeSpecies::Maple
			? FLinearColor(0.65f, 0.19f, 0.025f) : FLinearColor(0.1f, 0.4f, 0.06f));
}

UPaperSprite* APSGatheringTreeActor::CreateTreeSpriteAsset(UTexture2D* Texture, const FString& PackagePath, const bool bStumpSprite)
{
#if WITH_EDITOR
	if (!Texture || !PackagePath.StartsWith(TEXT("/Game/Art/Gathering/Trees/"))) return nullptr;
	const FString Name = FPackageName::GetLongPackageAssetName(PackagePath);
	if (UPaperSprite* Existing = LoadObject<UPaperSprite>(nullptr, *(PackagePath + TEXT(".") + Name))) return Existing;
	if (Texture->IsCompiling()) FTextureCompilingManager::Get().FinishCompilation({Texture});
	UPackage* Package = CreatePackage(*PackagePath);
	auto* Sprite = NewObject<UPaperSprite>(Package, *Name, RF_Public | RF_Standalone);
	FSpriteAssetInitParameters Params; Params.SetTextureAndFill(Texture); Params.SetPixelsPerUnrealUnit(1.0f);
	if (bStumpSprite) { Params.Offset = FIntPoint(0,208); Params.Dimension = FIntPoint(256,48); }
	Params.DefaultMaterialOverride = LoadObject<UMaterialInterface>(nullptr, TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
	Sprite->InitializeSprite(Params);
	Sprite->SetPivotMode(ESpritePivotMode::Custom, FVector2D(128,240));
	Sprite->MarkPackageDirty();
	return Sprite;
#else
	return nullptr;
#endif
}

void APSGatheringTreeActor::React(const bool bFalling)
{
	if (bFall) return;
	bFall = bFalling;
	AnimationTime = 0;
	if (bFall) Trunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(true);
}

void APSGatheringTreeActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AnimationTime += DeltaSeconds;
	const float Duration = bFall ? FallDuration : 0.35f;
	const float Alpha = FMath::Clamp(AnimationTime / Duration, 0.0f, 1.0f);
	const float Angle = bFall ? Alpha * 85 : PSGathering::GetShakeAngle(Alpha);
	Pivot->SetRelativeRotation(TreeSprite->GetSprite() ? FRotator(0,Angle,0) : FRotator(Angle,0,0));
	if (Alpha < 1) return;
	if (bFall)
	{
		Destroy();
		return;
	}
	Pivot->SetRelativeRotation(FRotator::ZeroRotator);
	SetActorTickEnabled(false);
}
