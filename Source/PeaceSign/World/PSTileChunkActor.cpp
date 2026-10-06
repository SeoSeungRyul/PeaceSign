// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSTileChunkActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "PSTileTypes.h"
#include "PSDirtAutoTileSet.h"
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMineableObjectMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMineableObjectMesh.Succeeded())
	{
		MineableObjectMesh = DefaultMineableObjectMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultTileMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (DefaultTileMaterial.Succeeded())
	{
		GrassMaterial = DefaultTileMaterial.Object;
		DirtMaterial = DefaultTileMaterial.Object;
		StoneMaterial = DefaultTileMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> DefaultDirtTexture(
		TEXT("/Game/Art/Tiles/Dirt/Set3/5_Dirt_Set3_MM.5_Dirt_Set3_MM"));
	if (DefaultDirtTexture.Succeeded())
	{
		DirtTexture = DefaultDirtTexture.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultDirtTextureMaterial(
		TEXT("/Paper2D/OpaqueUnlitSpriteMaterial.OpaqueUnlitSpriteMaterial"));
	if (DefaultDirtTextureMaterial.Succeeded())
	{
		DirtTextureMaterial = DefaultDirtTextureMaterial.Object;
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
	for (UHierarchicalInstancedStaticMeshComponent* ObjectInstances : {
		StoneInstances, CopperOreInstances, IronOreInstances, SilverOreInstances, GoldOreInstances,
		TitaniumOreInstances, LumistoneOreInstances, AsteriumOreInstances})
	{
		ObjectInstances->SetStaticMesh(MineableObjectMesh);
	}
}

void APSTileChunkActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMaterials();
}

void APSTileChunkActor::Rebuild(
	const FPSChunkData& ChunkData,
	const int32 ChunkSize,
	const float CellSize,
	const UPSDirtAutoTileSet* DirtTileSet)
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
	for (const TPair<uint8, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair : DirtVariantInstances)
		if (Pair.Value) Pair.Value->ClearInstances();

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
	TMap<uint8, TArray<FTransform>> DirtVariantTransforms;
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
			TArray<FTransform>* GroundTargetTransforms = nullptr;
			const bool bUsesDirtVariant = Cell.Variant != PSDirtAutoTile::NoVariant
				&& (Cell.GroundType == EPSTileType::Grass || Cell.GroundType == EPSTileType::Dirt);
			switch (Cell.GroundType)
			{
			case EPSTileType::Grass:
				if (!bUsesDirtVariant) GroundTargetTransforms = &GrassTransforms;
				break;
			case EPSTileType::Dirt:
				if (!bUsesDirtVariant) GroundTargetTransforms = &DirtTransforms;
				break;
			case EPSTileType::TilledSoil:
				GroundTargetTransforms = &TilledSoilTransforms;
				break;
			case EPSTileType::Stone:
				// Legacy runtime data still gets a dirt floor below its migrated stone object.
				GroundTargetTransforms = &DirtTransforms;
				break;
			case EPSTileType::Water:
				GroundTargetTransforms = &WaterTransforms;
				break;
			default:
				break;
			}

			const FVector GroundLocation(
				(static_cast<float>(LocalX) + 0.5f) * CellSize,
				(static_cast<float>(LocalY) + 0.5f) * CellSize,
				RenderZOffset);
			if (bUsesDirtVariant)
			{
				DirtVariantTransforms.FindOrAdd(Cell.Variant).Emplace(
					FRotator::ZeroRotator, GroundLocation, FVector(TileScale));
			}
			else if (GroundTargetTransforms)
			{
				GroundTargetTransforms->Emplace(FRotator::ZeroRotator, GroundLocation, FVector(TileScale));
			}

			if (Cell.ObjectType == EPSWorldObjectType::Stone || Cell.GroundType == EPSTileType::Stone)
			{
				TArray<FTransform>* ObjectTransforms = &StoneTransforms;
				switch (Cell.MineralType)
				{
				case EPSMineralType::Copper: ObjectTransforms = &CopperOreTransforms; break;
				case EPSMineralType::Iron: ObjectTransforms = &IronOreTransforms; break;
				case EPSMineralType::Silver: ObjectTransforms = &SilverOreTransforms; break;
				case EPSMineralType::Gold: ObjectTransforms = &GoldOreTransforms; break;
				case EPSMineralType::Titanium: ObjectTransforms = &TitaniumOreTransforms; break;
				case EPSMineralType::Lumistone: ObjectTransforms = &LumistoneOreTransforms; break;
				case EPSMineralType::Asterium: ObjectTransforms = &AsteriumOreTransforms; break;
				default: break;
				}
				const FVector ObjectLocation = GroundLocation + FVector(0, 0, 30.0f * TileScale);
				ObjectTransforms->Emplace(FRotator::ZeroRotator, ObjectLocation,
					FVector(TileScale * 0.55f, TileScale * 0.55f, TileScale * 0.35f));
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
	for (const TPair<uint8, TArray<FTransform>>& Pair : DirtVariantTransforms)
	{
		UHierarchicalInstancedStaticMeshComponent* Instances = GetOrCreateDirtVariantComponent(Pair.Key);
		ApplyDirtVariantMaterial(Pair.Key, DirtTileSet);
		if (Instances) Instances->AddInstances(Pair.Value, false, false, false);
	}
}

UHierarchicalInstancedStaticMeshComponent* APSTileChunkActor::GetOrCreateDirtVariantComponent(const uint8 Variant)
{
	if (UHierarchicalInstancedStaticMeshComponent* Existing = DirtVariantInstances.FindRef(Variant))
		return Existing;
	if (Variant == PSDirtAutoTile::NoVariant) return nullptr;

	const FName ComponentName(*FString::Printf(TEXT("DirtVariant_%u"), Variant));
	UHierarchicalInstancedStaticMeshComponent* Instances =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(this, ComponentName, RF_Transient);
	Instances->SetupAttachment(SceneRoot);
	ConfigureInstances(Instances);
	Instances->RegisterComponent();
	DirtVariantInstances.Add(Variant, Instances);
	return Instances;
}

void APSTileChunkActor::ApplyDirtVariantMaterial(
	const uint8 Variant,
	const UPSDirtAutoTileSet* DirtTileSet)
{
	UHierarchicalInstancedStaticMeshComponent* Instances = DirtVariantInstances.FindRef(Variant);
	if (!Instances) return;

	UTexture2D* Texture = DirtTileSet ? DirtTileSet->GetVariantTexture(Variant) : nullptr;
	if (!Texture && (!DirtTileSet || DirtTileSet->Rules.IsEmpty()))
	{
		if (const TCHAR* TexturePath = PSDirtAutoTile::GetDefaultVariantTexturePath(Variant))
			Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
	}
	if (!Texture) Texture = DirtTexture;

	UMaterialInstanceDynamic* DynamicMaterial = DirtVariantMaterials.FindRef(Variant);
	if (!DynamicMaterial)
	{
		UMaterialInterface* Parent = DirtTextureMaterial ? DirtTextureMaterial : DirtMaterial;
		if (!Parent) return;
		DynamicMaterial = UMaterialInstanceDynamic::Create(Parent, this);
		DirtVariantMaterials.Add(Variant, DynamicMaterial);
		Instances->SetMaterial(0, DynamicMaterial);
	}
	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
	if (Texture) DynamicMaterial->SetTextureParameterValue(TEXT("SpriteTexture"), Texture);
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
		return Cell.GroundType == EPSTileType::Water
			|| Cell.ObjectType == EPSWorldObjectType::Stone
			|| Cell.GroundType == EPSTileType::Stone;
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
		const FLinearColor& DefaultColor,
		UTexture2D* Texture = nullptr)
	{
		Instances->SetStaticMesh(Mesh);
		if (!Material)
		{
			return;
		}

		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), DefaultColor);
		if (Texture)
		{
			DynamicMaterial->SetTextureParameterValue(TEXT("SpriteTexture"), Texture);
		}
		Instances->SetMaterial(0, DynamicMaterial);
	};

	ApplyMaterial(GrassInstances, TileMesh, GrassMaterial, FLinearColor(0.12f, 0.45f, 0.08f));
	ApplyMaterial(
		DirtInstances,
		TileMesh,
		DirtTexture && DirtTextureMaterial ? DirtTextureMaterial : DirtMaterial,
		FLinearColor::White,
		DirtTexture);
	ApplyMaterial(TilledSoilInstances, TileMesh, DirtMaterial, FLinearColor(0.25f, 0.10f, 0.03f));
	ApplyMaterial(StoneInstances, MineableObjectMesh, StoneMaterial, FLinearColor(0.35f, 0.37f, 0.4f));
	ApplyMaterial(CopperOreInstances, MineableObjectMesh, StoneMaterial, FLinearColor(0.72f, 0.30f, 0.14f));
	ApplyMaterial(IronOreInstances, MineableObjectMesh, StoneMaterial, FLinearColor(0.25f, 0.27f, 0.30f));
	ApplyMaterial(SilverOreInstances, MineableObjectMesh, StoneMaterial, FLinearColor(0.75f, 0.80f, 0.86f));
	ApplyMaterial(GoldOreInstances, MineableObjectMesh, StoneMaterial, FLinearColor(0.95f, 0.62f, 0.08f));
	ApplyMaterial(TitaniumOreInstances, MineableObjectMesh, StoneMaterial, FLinearColor(0.42f, 0.60f, 0.72f));
	ApplyMaterial(LumistoneOreInstances, MineableObjectMesh, StoneMaterial, FLinearColor(0.25f, 0.95f, 0.75f));
	ApplyMaterial(AsteriumOreInstances, MineableObjectMesh, StoneMaterial, FLinearColor(0.70f, 0.45f, 0.95f));
	ApplyMaterial(WaterInstances, TileMesh, StoneMaterial, FLinearColor(0.02f, 0.3f, 0.9f));
	ApplyMaterial(SeedInstances, SeedMesh, GrassMaterial, FLinearColor(0.45f, 0.24f, 0.06f));
}
