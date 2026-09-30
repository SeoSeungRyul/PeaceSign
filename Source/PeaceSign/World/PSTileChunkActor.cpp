// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSTileChunkActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "PSTileTypes.h"
#include "PSCropGrowth.h"

APSTileChunkActor::APSTileChunkActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GrassInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GrassInstances"));
	GrassInstances->SetupAttachment(SceneRoot);

	DirtInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DirtInstances"));
	DirtInstances->SetupAttachment(SceneRoot);

	TilledSoilInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TilledSoilInstances"));
	TilledSoilInstances->SetupAttachment(SceneRoot);

	StoneInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("StoneInstances"));
	StoneInstances->SetupAttachment(SceneRoot);
	CopperOreInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CopperOreInstances"));
	CopperOreInstances->SetupAttachment(SceneRoot);
	IronOreInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("IronOreInstances"));
	IronOreInstances->SetupAttachment(SceneRoot);
	SilverOreInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SilverOreInstances"));
	SilverOreInstances->SetupAttachment(SceneRoot);
	GoldOreInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GoldOreInstances"));
	GoldOreInstances->SetupAttachment(SceneRoot);
	TitaniumOreInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TitaniumOreInstances"));
	TitaniumOreInstances->SetupAttachment(SceneRoot);
	LumistoneOreInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LumistoneOreInstances"));
	LumistoneOreInstances->SetupAttachment(SceneRoot);
	AsteriumOreInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("AsteriumOreInstances"));
	AsteriumOreInstances->SetupAttachment(SceneRoot);
	WaterInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("WaterInstances"));
	WaterInstances->SetupAttachment(SceneRoot);
	SeedInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SeedInstances"));
	SeedInstances->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultTileMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (DefaultTileMesh.Succeeded())
	{
		TileMesh = DefaultTileMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultSeedMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DefaultSeedMesh.Succeeded())
	{
		SeedMesh = DefaultSeedMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultTileMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (DefaultTileMaterial.Succeeded())
	{
		GrassMaterial = DefaultTileMaterial.Object;
		DirtMaterial = DefaultTileMaterial.Object;
		StoneMaterial = DefaultTileMaterial.Object;
	}

	ConfigureInstances(GrassInstances);
	ConfigureInstances(DirtInstances);
	ConfigureInstances(StoneInstances);
	ConfigureInstances(CopperOreInstances);
	ConfigureInstances(IronOreInstances);
	ConfigureInstances(SilverOreInstances);
	ConfigureInstances(GoldOreInstances);
	ConfigureInstances(TitaniumOreInstances);
	ConfigureInstances(LumistoneOreInstances);
	ConfigureInstances(AsteriumOreInstances);
	ConfigureInstances(WaterInstances);
	ConfigureInstances(TilledSoilInstances);
	ConfigureInstances(SeedInstances);
	SeedInstances->SetStaticMesh(SeedMesh);
}

void APSTileChunkActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMaterials();
}

void APSTileChunkActor::Rebuild(const FPSChunkData& ChunkData, const int32 ChunkSize, const float CellSize)
{
	RebuildBlockingCollision(ChunkData, ChunkSize, CellSize);
	GrassInstances->ClearInstances();
	DirtInstances->ClearInstances();
	StoneInstances->ClearInstances();
	CopperOreInstances->ClearInstances();
	IronOreInstances->ClearInstances();
	SilverOreInstances->ClearInstances();
	GoldOreInstances->ClearInstances();
	TitaniumOreInstances->ClearInstances();
	LumistoneOreInstances->ClearInstances();
	AsteriumOreInstances->ClearInstances();
	WaterInstances->ClearInstances();
	TilledSoilInstances->ClearInstances();
	SeedInstances->ClearInstances();

	if (!TileMesh || ChunkData.Cells.Num() != ChunkSize * ChunkSize)
	{
		return;
	}

	const float TileScale = CellSize / 100.0f;
	TArray<FTransform> GrassTransforms;
	TArray<FTransform> DirtTransforms;
	TArray<FTransform> StoneTransforms;
	TArray<FTransform> CopperOreTransforms;
	TArray<FTransform> IronOreTransforms;
	TArray<FTransform> SilverOreTransforms;
	TArray<FTransform> GoldOreTransforms;
	TArray<FTransform> TitaniumOreTransforms;
	TArray<FTransform> LumistoneOreTransforms;
	TArray<FTransform> AsteriumOreTransforms;
	TArray<FTransform> WaterTransforms;
	TArray<FTransform> TilledSoilTransforms;
	TArray<FTransform> SeedTransforms;
	GrassTransforms.Reserve(ChunkData.Cells.Num());
	DirtTransforms.Reserve(ChunkData.Cells.Num());
	StoneTransforms.Reserve(ChunkData.Cells.Num());
	CopperOreTransforms.Reserve(ChunkData.Cells.Num());
	IronOreTransforms.Reserve(ChunkData.Cells.Num());
	SilverOreTransforms.Reserve(ChunkData.Cells.Num());
	GoldOreTransforms.Reserve(ChunkData.Cells.Num());
	TitaniumOreTransforms.Reserve(ChunkData.Cells.Num());
	LumistoneOreTransforms.Reserve(ChunkData.Cells.Num());
	AsteriumOreTransforms.Reserve(ChunkData.Cells.Num());
	TilledSoilTransforms.Reserve(ChunkData.Cells.Num());
	SeedTransforms.Reserve(ChunkData.Cells.Num());

	for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
		{
			const FPSTileCell& Cell = ChunkData.Cells[PSGrid::LocalToIndex(FIntPoint(LocalX, LocalY), ChunkSize)];
			TArray<FTransform>* TargetTransforms = nullptr;
			switch (Cell.GroundType)
			{
			case EPSTileType::Grass:
				TargetTransforms = &GrassTransforms;
				break;
			case EPSTileType::Dirt:
				TargetTransforms = &DirtTransforms;
				break;
			case EPSTileType::TilledSoil:
				TargetTransforms = &TilledSoilTransforms;
				break;
			case EPSTileType::Stone:
				switch (Cell.MineralType)
				{
				case EPSMineralType::Copper: TargetTransforms = &CopperOreTransforms; break;
				case EPSMineralType::Iron: TargetTransforms = &IronOreTransforms; break;
				case EPSMineralType::Silver: TargetTransforms = &SilverOreTransforms; break;
				case EPSMineralType::Gold: TargetTransforms = &GoldOreTransforms; break;
				case EPSMineralType::Titanium: TargetTransforms = &TitaniumOreTransforms; break;
				case EPSMineralType::Lumistone: TargetTransforms = &LumistoneOreTransforms; break;
				case EPSMineralType::Asterium: TargetTransforms = &AsteriumOreTransforms; break;
				case EPSMineralType::None:
				default: TargetTransforms = &StoneTransforms; break;
				}
				break;
			case EPSTileType::Water:
				TargetTransforms = &WaterTransforms;
				break;
			default:
				break;
			}

			if (TargetTransforms)
			{
				const FVector Location(
					(static_cast<float>(LocalX) + 0.5f) * CellSize,
					(static_cast<float>(LocalY) + 0.5f) * CellSize,
					RenderZOffset);
				TargetTransforms->Emplace(FRotator::ZeroRotator, Location, FVector(TileScale));
			}
			if (Cell.CropType != EPSCropType::None)
			{
				const FVector SeedCenter(
					(static_cast<float>(LocalX) + 0.5f) * CellSize,
					(static_cast<float>(LocalY) + 0.5f) * CellSize,
					RenderZOffset + 5.0f * TileScale);
				const FVector2D Offsets[] = {FVector2D(0, 0), FVector2D(-0.22f, -0.22f),
					FVector2D(0.22f, 0.22f), FVector2D(-0.22f, 0.22f), FVector2D(0.22f, -0.22f)};
				const int32 Stage = FMath::Clamp<int32>(Cell.GrowthStage, 1, PSCropGrowth::MaxStage);
				for (int32 SeedIndex = 0; SeedIndex < Stage; ++SeedIndex)
				{
					const FVector Location = SeedCenter + FVector(Offsets[SeedIndex].X * CellSize, Offsets[SeedIndex].Y * CellSize, 0);
					SeedTransforms.Emplace(FRotator::ZeroRotator, Location, FVector(TileScale * 0.10f));
				}
			}
		}
	}

	GrassInstances->AddInstances(GrassTransforms, false, false, false);
	DirtInstances->AddInstances(DirtTransforms, false, false, false);
	StoneInstances->AddInstances(StoneTransforms, false, false, false);
	CopperOreInstances->AddInstances(CopperOreTransforms, false, false, false);
	IronOreInstances->AddInstances(IronOreTransforms, false, false, false);
	SilverOreInstances->AddInstances(SilverOreTransforms, false, false, false);
	GoldOreInstances->AddInstances(GoldOreTransforms, false, false, false);
	TitaniumOreInstances->AddInstances(TitaniumOreTransforms, false, false, false);
	LumistoneOreInstances->AddInstances(LumistoneOreTransforms, false, false, false);
	AsteriumOreInstances->AddInstances(AsteriumOreTransforms, false, false, false);
	WaterInstances->AddInstances(WaterTransforms, false, false, false);
	TilledSoilInstances->AddInstances(TilledSoilTransforms, false, false, false);
	SeedInstances->AddInstances(SeedTransforms, false, false, false);
}

void APSTileChunkActor::RebuildBlockingCollision(const FPSChunkData& ChunkData, const int32 ChunkSize, const float CellSize)
{
	for (UBoxComponent* Blocker : BlockingTileColliders)
	{
		if (Blocker) Blocker->DestroyComponent();
	}
	BlockingTileColliders.Reset();
	if (ChunkSize <= 0 || CellSize <= 0 || ChunkData.Cells.Num() != ChunkSize * ChunkSize) return;

	const auto BlocksPawn = [](const FPSTileCell& Cell)
	{
		return Cell.GroundType == EPSTileType::Water || Cell.GroundType == EPSTileType::Stone;
	};
	// Merge consecutive blocking cells in each row. Rebuilds split the span immediately
	// when mining changes a stone cell into dirt.
	for (int32 Y = 0; Y < ChunkSize; ++Y)
	{
		for (int32 X = 0; X < ChunkSize;)
		{
			if (!BlocksPawn(ChunkData.Cells[Y * ChunkSize + X])) { ++X; continue; }
			const int32 StartX = X;
			while (X < ChunkSize && BlocksPawn(ChunkData.Cells[Y * ChunkSize + X])) ++X;
			UBoxComponent* Blocker = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
			Blocker->SetupAttachment(SceneRoot);
			Blocker->SetRelativeLocation(FVector((StartX + (X - StartX) * 0.5f) * CellSize, (Y + 0.5f) * CellSize, 0));
			Blocker->SetBoxExtent(FVector((X - StartX) * CellSize * 0.5f, CellSize * 0.5f, 1000.0f));
			Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Blocker->SetCollisionObjectType(ECC_WorldStatic);
			Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
			Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			Blocker->SetGenerateOverlapEvents(false);
			Blocker->SetCanEverAffectNavigation(false);
			Blocker->SetHiddenInGame(true);
			Blocker->RegisterComponent();
			BlockingTileColliders.Add(Blocker);
		}
	}
}

void APSTileChunkActor::ConfigureInstances(UHierarchicalInstancedStaticMeshComponent* Instances) const
{
	Instances->SetStaticMesh(TileMesh);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetGenerateOverlapEvents(false);
	Instances->SetCastShadow(false);
	Instances->SetMobility(EComponentMobility::Movable);
}

void APSTileChunkActor::ApplyMaterials()
{
	const auto ApplyMaterial = [this](
		UHierarchicalInstancedStaticMeshComponent* Instances,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FLinearColor& DefaultColor)
	{
		Instances->SetStaticMesh(Mesh);
		if (!Material)
		{
			return;
		}

		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), DefaultColor);
		Instances->SetMaterial(0, DynamicMaterial);
	};

	ApplyMaterial(GrassInstances, TileMesh, GrassMaterial, FLinearColor(0.12f, 0.45f, 0.08f));
	ApplyMaterial(DirtInstances, TileMesh, DirtMaterial, FLinearColor(0.38f, 0.16f, 0.05f));
	ApplyMaterial(TilledSoilInstances, TileMesh, DirtMaterial, FLinearColor(0.25f, 0.10f, 0.03f));
	ApplyMaterial(StoneInstances, TileMesh, StoneMaterial, FLinearColor(0.35f, 0.37f, 0.4f));
	ApplyMaterial(CopperOreInstances, TileMesh, StoneMaterial, FLinearColor(0.72f, 0.30f, 0.14f));
	ApplyMaterial(IronOreInstances, TileMesh, StoneMaterial, FLinearColor(0.25f, 0.27f, 0.30f));
	ApplyMaterial(SilverOreInstances, TileMesh, StoneMaterial, FLinearColor(0.75f, 0.80f, 0.86f));
	ApplyMaterial(GoldOreInstances, TileMesh, StoneMaterial, FLinearColor(0.95f, 0.62f, 0.08f));
	ApplyMaterial(TitaniumOreInstances, TileMesh, StoneMaterial, FLinearColor(0.42f, 0.60f, 0.72f));
	ApplyMaterial(LumistoneOreInstances, TileMesh, StoneMaterial, FLinearColor(0.25f, 0.95f, 0.75f));
	ApplyMaterial(AsteriumOreInstances, TileMesh, StoneMaterial, FLinearColor(0.70f, 0.45f, 0.95f));
	ApplyMaterial(WaterInstances, TileMesh, StoneMaterial, FLinearColor(0.02f, 0.3f, 0.9f));
	ApplyMaterial(SeedInstances, SeedMesh, GrassMaterial, FLinearColor(0.45f, 0.24f, 0.06f));
}
