#include "PSWorldItemActor.h"

#include "PSInventoryComponent.h"
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../PSPlayerController.h"
#include "../World/PSGridWorld.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr float HopDuration = 0.42f;
	constexpr float PickupDelay = HopDuration + 0.15f;
}

APSWorldItemActor::APSWorldItemActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ColorMaterial(TEXT("/Game/Art/Gathering/M_GatheringColor.M_GatheringColor"));
	GatheringMaterial = ColorMaterial.Object;
	Mesh->SetWorldScale3D(FVector(0.18f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);
	ItemIcon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemIcon"));
	ItemIcon->SetupAttachment(Mesh);
	ItemIcon->SetAbsolute(false, false, true);
	ItemIcon->SetRelativeRotation(FRotator(0, 90, 0));
	ItemIcon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ItemIcon->SetGenerateOverlapEvents(false);
	ItemIcon->SetCastShadow(false);
	ItemIcon->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IconShader(TEXT("/Game/Art/Tiles/Farm/M_FarmTileUV.M_FarmTileUV"));
	ItemIcon->SetStaticMesh(Plane.Object);
	IconMaterial = IconShader.Object;
}

void APSWorldItemActor::InitializeItem(const FPSItemStack& InItem, APSGridWorld* Grid)
{
	Item = InItem;
	PickupGrid = Grid;
	if (Grid)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorTickEnabled(true);
	}
	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		Material->SetVectorParameterValue(TEXT("Color"), PSItems::GetDefinition(Item).Color);
}

void APSWorldItemActor::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32,
	bool, const FHitResult&)
{
	TryCollect(OtherActor);
}

void APSWorldItemActor::InitializeGatheringItem(const FPSItemStack& InItem, UPSGatheringWorldComponent* World,
	const FGuid Id, UTexture2D* Icon, const float CellSize, const bool bPlayHop)
{
	if (GatheringMaterial) Mesh->SetMaterial(0, GatheringMaterial);
	InitializeItem(InItem, World ? Cast<APSGridWorld>(World->GetOwner()) : nullptr);
	GatheringWorld = World;
	GatheringDropId = Id;
	GatheringCellSize = CellSize;
	bLanding = bPlayHop;
	if (Icon && IconMaterial && ItemIcon->GetStaticMesh())
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(IconMaterial, this);
		Material->SetTextureParameterValue(TEXT("SpriteTexture"), Icon);
		ItemIcon->SetMaterial(0, Material);
		Mesh->SetVisibility(false);
		ItemIcon->SetWorldScale3D(FVector(0.68f * CellSize / 100.0f));
		ItemIcon->SetVisibility(true);
		UpdateIconPosition(0);
	}
	SetActorTickEnabled(true);
}

void APSWorldItemActor::UpdateIconPosition(const float HopHeight)
{
	// The camera is above the XY grid. Animate along screen-up and above terrain props.
	ItemIcon->SetWorldLocation(GetActorLocation() + GatheringCellSize * FVector(HopHeight, 0, 0.72f));
}

void APSWorldItemActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bLanding)
	{
		HopElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(HopElapsed / HopDuration, 0.0f, 1.0f);
		if (ItemIcon->IsVisible()) UpdateIconPosition(1.7f * Alpha * (1 - Alpha));
		if (HopElapsed < PickupDelay) return;
		bLanding = false;
	}
	if (!PickupGrid.IsValid()) return;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		TryCollect(*It);
		if (IsActorBeingDestroyed()) break;
	}
}

void APSWorldItemActor::TryCollect(AActor* OtherActor)
{
	if (IsActorBeingDestroyed() || bLanding) return;
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (PickupGrid.IsValid() && (!Pawn
		|| PickupGrid->WorldToCell(Pawn->GetActorLocation()) != PickupGrid->WorldToCell(GetActorLocation()))) return;
	APSPlayerController* Controller = Pawn ? Cast<APSPlayerController>(Pawn->GetController()) : nullptr;
	UPSInventoryComponent* Inventory = Controller ? Controller->GetInventoryComponent() : nullptr;
	if (GatheringDropId.IsValid())
	{
		if (GatheringWorld.IsValid() && GatheringWorld->CollectDrop(GatheringDropId, Inventory)) Destroy();
		return;
	}
	if (Inventory && !Item.IsEmpty() && Inventory->AddItemVariant(
		Item.ItemType, Item.ItemId, Item.Quantity, Item.CropId, Item.CurrentDurability)) Destroy();
}
