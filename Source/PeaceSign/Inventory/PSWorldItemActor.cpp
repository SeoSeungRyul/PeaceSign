#include "PSWorldItemActor.h"

#include "PSInventoryComponent.h"
#include "../PSPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

APSWorldItemActor::APSWorldItemActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);
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
}

void APSWorldItemActor::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32,
	bool, const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	APSPlayerController* Controller = Pawn ? Cast<APSPlayerController>(Pawn->GetController()) : nullptr;
	UPSInventoryComponent* Inventory = Controller ? Controller->GetInventoryComponent() : nullptr;
	if (Inventory && !Item.IsEmpty() && Inventory->AddItemVariant(
		Item.ItemType, Item.ItemId, Item.Quantity, Item.CropId, Item.CurrentDurability)) Destroy();
}
