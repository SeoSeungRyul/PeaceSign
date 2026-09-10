// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSTileChunkActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
struct FPSChunkData;

UCLASS()
class PEACESIGN_API APSTileChunkActor : public AActor
{
	GENERATED_BODY()

public:
	APSTileChunkActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	void Rebuild(const FPSChunkData& ChunkData, int32 ChunkSize, float CellSize);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DirtInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StoneInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TilledSoilInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SeedInstances;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UStaticMesh> TileMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UStaticMesh> SeedMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UMaterialInterface> DirtMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UMaterialInterface> StoneMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	float RenderZOffset = 1.0f;

private:
	void ConfigureInstances(UHierarchicalInstancedStaticMeshComponent* Instances) const;
	void ApplyMaterials();
};
