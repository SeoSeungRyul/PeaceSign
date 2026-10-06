// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSTileChunkActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UTexture2D;
class UBoxComponent;
class UPSDirtAutoTileSet;
struct FPSChunkData;

UCLASS()
class PEACESIGN_API APSTileChunkActor : public AActor
{
	GENERATED_BODY()

public:
	APSTileChunkActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	void Rebuild(const FPSChunkData& ChunkData, int32 ChunkSize, float CellSize,
		const UPSDirtAutoTileSet* DirtTileSet = nullptr);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DirtInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StoneInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Mining")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CopperOreInstances;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Mining")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> IronOreInstances;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Mining")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SilverOreInstances;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Mining")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GoldOreInstances;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Mining")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TitaniumOreInstances;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Mining")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LumistoneOreInstances;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Mining")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> AsteriumOreInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WaterInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TilledSoilInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Chunk")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SeedInstances;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UStaticMesh> TileMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UStaticMesh> SeedMesh;

	/** Mesh used by stones and ore objects placed above the ground layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UStaticMesh> MineableObjectMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UMaterialInterface> DirtMaterial;

	/** Fallback used when an auto-tile rule has no valid texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals|Dirt Auto Tile")
	TObjectPtr<UTexture2D> DirtTexture;

	/** Material with a SpriteTexture parameter used to draw auto-tile textures on the tile plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals|Dirt Auto Tile")
	TObjectPtr<UMaterialInterface> DirtTextureMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	TObjectPtr<UMaterialInterface> StoneMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Chunk|Visuals")
	float RenderZOffset = 1.0f;

private:
	UPROPERTY(Transient)
	TMap<uint8, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> DirtVariantInstances;

	UPROPERTY(Transient)
	TMap<uint8, TObjectPtr<UMaterialInstanceDynamic>> DirtVariantMaterials;

	// Separate collision volumes keep flat water and mineable object visuals unchanged.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> BlockingTileColliders;
	void RebuildBlockingCollision(const FPSChunkData& ChunkData, int32 ChunkSize, float CellSize);
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateDirtVariantComponent(uint8 Variant);
	void ApplyDirtVariantMaterial(uint8 Variant, const UPSDirtAutoTileSet* DirtTileSet);
	void ConfigureInstances(UHierarchicalInstancedStaticMeshComponent* Instances) const;
	void ApplyMaterials();
};
