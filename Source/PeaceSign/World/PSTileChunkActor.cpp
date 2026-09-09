// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSTileChunkActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "PSTileTypes.h"

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
	GrassInstances->ClearInstances();
	DirtInstances->ClearInstances();
	StoneInstances->ClearInstances();
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
	TArray<FTransform> TilledSoilTransforms;
	TArray<FTransform> SeedTransforms;
	GrassTransforms.Reserve(ChunkData.Cells.Num());
	DirtTransforms.Reserve(ChunkData.Cells.Num());
	StoneTransforms.Reserve(ChunkData.Cells.Num());
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
				TargetTransforms = &StoneTransforms;
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
				const FVector SeedLocation(
					(static_cast<float>(LocalX) + 0.5f) * CellSize,
					(static_cast<float>(LocalY) + 0.5f) * CellSize,
					RenderZOffset + 7.0f);
				SeedTransforms.Emplace(FRotator::ZeroRotator, SeedLocation, FVector(TileScale * 0.10f));
			}
		}
	}

	GrassInstances->AddInstances(GrassTransforms, false, false, false);
	DirtInstances->AddInstances(DirtTransforms, false, false, false);
	StoneInstances->AddInstances(StoneTransforms, false, false, false);
	TilledSoilInstances->AddInstances(TilledSoilTransforms, false, false, false);
	SeedInstances->AddInstances(SeedTransforms, false, false, false);
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
	ApplyMaterial(SeedInstances, SeedMesh, GrassMaterial, FLinearColor(0.45f, 0.24f, 0.06f));
}
