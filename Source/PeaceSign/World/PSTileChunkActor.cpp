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

	StoneInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("StoneInstances"));
	StoneInstances->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultTileMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (DefaultTileMesh.Succeeded())
	{
		TileMesh = DefaultTileMesh.Object;
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

	if (!TileMesh || ChunkData.Cells.Num() != ChunkSize * ChunkSize)
	{
		return;
	}

	const float TileScale = CellSize / 100.0f;
	TArray<FTransform> GrassTransforms;
	TArray<FTransform> DirtTransforms;
	TArray<FTransform> StoneTransforms;
	GrassTransforms.Reserve(ChunkData.Cells.Num());
	DirtTransforms.Reserve(ChunkData.Cells.Num());
	StoneTransforms.Reserve(ChunkData.Cells.Num());

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
		}
	}

	GrassInstances->AddInstances(GrassTransforms, false, false, false);
	DirtInstances->AddInstances(DirtTransforms, false, false, false);
	StoneInstances->AddInstances(StoneTransforms, false, false, false);
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
		UMaterialInterface* Material,
		const FLinearColor& DefaultColor)
	{
		Instances->SetStaticMesh(TileMesh);
		if (!Material)
		{
			return;
		}

		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), DefaultColor);
		Instances->SetMaterial(0, DynamicMaterial);
	};

	ApplyMaterial(GrassInstances, GrassMaterial, FLinearColor(0.12f, 0.45f, 0.08f));
	ApplyMaterial(DirtInstances, DirtMaterial, FLinearColor(0.38f, 0.16f, 0.05f));
	ApplyMaterial(StoneInstances, StoneMaterial, FLinearColor(0.35f, 0.37f, 0.4f));
}
