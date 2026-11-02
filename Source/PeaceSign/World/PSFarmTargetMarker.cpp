#include "PSFarmTargetMarker.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UTexture2D* MakeColorTexture(const FColor Color)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(1, 1, PF_B8G8R8A8);
		if (!Texture) return nullptr;
		void* Pixels = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Pixels, &Color, sizeof(FColor));
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

APSFarmTargetMarker::APSFarmTargetMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	Border = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FarmTargetBorder"));
	SetRootComponent(Border);
	Border->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Border->SetGenerateOverlapEvents(false);
	Border->SetCastShadow(false);
	Border->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	Border->SetStaticMesh(Mesh.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shader(TEXT("/Game/Art/Tiles/Farm/M_FarmTileUV.M_FarmTileUV"));
	Material = Shader.Object;
}

void APSFarmTargetMarker::ShowTarget(const FVector Center, const float CellSize, const bool bUsable)
{
	if (!DynamicMaterial && Material)
	{
		Green = MakeColorTexture(FColor(40, 240, 65));
		Red = MakeColorTexture(FColor(255, 45, 35));
		DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
		Border->SetMaterial(0, DynamicMaterial);
	}
	if (DynamicMaterial) DynamicMaterial->SetTextureParameterValue(TEXT("SpriteTexture"), bUsable ? Green.Get() : Red.Get());
	SetActorLocation(Center + FVector(0, 0, 4));
	SetActorHiddenInGame(false);
	if (CachedCellSize == CellSize) return;
	CachedCellSize = CellSize;
	const float Size = FMath::Max(1.0f, CellSize * 0.94f);
	const float Width = FMath::Min(Size * 0.1f, FMath::Max(1.0f, CellSize * 0.025f));
	Border->ClearInstances();
	for (float Sign : {-1.0f, 1.0f})
	{
		Border->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, Sign * (Size - Width) * 0.5f, 0), FVector(Size / 100, Width / 100, 1)));
		Border->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Sign * (Size - Width) * 0.5f, 0, 0), FVector(Width / 100, (Size - 2 * Width) / 100, 1)));
	}
}
