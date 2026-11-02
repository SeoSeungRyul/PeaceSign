#include "PSWorldItemActor.h"

#include "PSInventoryComponent.h"
#include "../Gathering/PSGatheringWorldComponent.h"
#include "../PSPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

APSWorldItemActor::APSWorldItemActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.25f;
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
}

void APSWorldItemActor::InitializeItem(const FPSItemStack& InItem)
{
	Item = InItem;
	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		Material->SetVectorParameterValue(TEXT("Color"), PSItems::GetDefinition(Item).Color);
}

void APSWorldItemActor::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32,
	bool, const FHitResult&)
{
	TryCollect(OtherActor);
}

void APSWorldItemActor::InitializeGatheringItem(const FPSItemStack& InItem, UPSGatheringWorldComponent* World, const FGuid Id)
{
	if (GatheringMaterial) Mesh->SetMaterial(0, GatheringMaterial);
	InitializeItem(InItem);
	GatheringWorld = World;
	GatheringDropId = Id;
	SetActorTickEnabled(true);
}

void APSWorldItemActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TArray<AActor*> Overlaps;
	Mesh->GetOverlappingActors(Overlaps, APawn::StaticClass());
	for (AActor* Other : Overlaps) { TryCollect(Other); if (IsActorBeingDestroyed()) break; }
}

void APSWorldItemActor::TryCollect(AActor* OtherActor)
{
	if (IsActorBeingDestroyed()) return;
	const APawn* Pawn = Cast<APawn>(OtherActor);
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
