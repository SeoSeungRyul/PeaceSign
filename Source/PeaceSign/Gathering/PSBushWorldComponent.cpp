#include "PSBushWorldComponent.h"
#include "PSGatheringWorldComponent.h"
#include "../World/PSGridWorld.h"
#include "../PSPlayerCharacter.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr float BushRestYaw = 90.0f;
}

UPSBushWorldComponent::UPSBushWorldComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

const FPSBushRecord* UPSBushWorldComponent::Find(const FIntPoint Cell) const
{
	return Bushes.FindByPredicate([Cell](const FPSBushRecord& Bush) { return Bush.Cell == Cell && Bush.RemainingHarvests > 0; });
}

bool UPSBushWorldComponent::HasBush(const FIntPoint Cell) const
{
	return bEnabled && Find(Cell) != nullptr;
}

bool UPSBushWorldComponent::Place(FPSBushRecord& Bush, FRandomStream& Random)
{
	auto* Grid = CastChecked<APSGridWorld>(GetOwner());
	for (int32 Attempt = 0; Attempt < 400; ++Attempt)
	{
		const FIntPoint Cell = PatchOrigin + FIntPoint(Random.RandRange(0, 19), Random.RandRange(0, 19));
		if (Grid->GetGroundTile(Cell) != EPSTileType::Grass || !Grid->CanPlaceGatheringTree(Cell)) continue;
		bool bApproachable = false;
		for (const FIntPoint Side : {FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1)})
			bApproachable |= Grid->GetWorldObjectType(Cell + Side) == EPSWorldObjectType::None
				&& Grid->Gathering->CanReachCell(Cell + Side, Cell, nullptr);
		if (!bApproachable) continue;
		const auto* Clock = GetWorld()->GetSubsystem<UPSGameTimeSubsystem>();
		Bush.Cell = Cell;
		Bush.ItemId = Clock && Clock->GetSeasonIndex() >= 2 ? TEXT("Forage.Blackberry") : TEXT("Forage.WildStrawberry");
		Bush.RemainingHarvests = 3;
		Bush.ReadyAt = 0;
		return true;
	}
	return false;
}

void UPSBushWorldComponent::Initialize()
{
	if (!bEnabled || bInitialized) return;
	bInitialized = true;
	FRandomStream Random(CastChecked<APSGridWorld>(GetOwner())->GetGatheringSeed() ^ 0x4B5534);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FPSBushRecord Bush;
		if (Place(Bush, Random)) Bushes.Add(Bush);
	}
	CastChecked<APSGridWorld>(GetOwner())->SaveGatheringState();
	RefreshActors();
}

void UPSBushWorldComponent::Restore(const TArray<FPSBushRecord>& Records, const bool bWasInitialized)
{
	ClearActors();
	Bushes = Records;
	bInitialized = bWasInitialized;
	NextSpawnCheck = -1;
}

void UPSBushWorldComponent::Reset()
{
	ClearActors();
	Bushes.Reset();
	bInitialized = false;
	Initialize();
}

bool UPSBushWorldComponent::CanReach(const APSPlayerCharacter* Player, const FIntPoint Cell) const
{
	const auto* Grid = CastChecked<APSGridWorld>(GetOwner());
	if (!IsValid(Player) || !HasBush(Cell) || Grid->GetGroundTile(Cell) != EPSTileType::Grass) return false;
	const FIntPoint From = Grid->WorldToCell(Player->GetActorLocation());
	return From == Cell || Grid->Gathering->CanReachCell(From, Cell, Player);
}

TOptional<FIntPoint> UPSBushWorldComponent::FindNearest(const APSPlayerCharacter* Player) const
{
	if (!Player) return {};
	const auto* Grid = CastChecked<APSGridWorld>(GetOwner());
	const FIntPoint From = Grid->WorldToCell(Player->GetActorLocation());
	const FIntPoint Facing = Player->GetFacingCellOffset();
	TOptional<FIntPoint> Best;
	int32 BestDistance = MAX_int32;
	int32 BestFacing = MIN_int32;
	for (const auto& Bush : Bushes)
	{
		if (!CanReach(Player, Bush.Cell)) continue;
		const FIntPoint Delta = Bush.Cell - From;
		const int32 Distance = Delta.SizeSquared();
		const int32 FacingScore = Delta.X * Facing.X + Delta.Y * Facing.Y;
		if (Distance < BestDistance || (Distance == BestDistance && FacingScore > BestFacing))
		{
			Best = Bush.Cell;
			BestDistance = Distance;
			BestFacing = FacingScore;
		}
	}
	return Best;
}

void UPSBushWorldComponent::SetHarvestShake(const FIntPoint Cell, const float Angle)
{
	if (AActor* BushActor = Actors.FindRef(Cell).Get(); IsValid(BushActor))
	{
		BushActor->SetActorRotation(FRotator(0, BushRestYaw + Angle, 0));
	}
}

bool UPSBushWorldComponent::Harvest(APSPlayerCharacter* Player, const FIntPoint Cell, UPSInventoryComponent* Inventory)
{
	auto* Grid = CastChecked<APSGridWorld>(GetOwner());
	if (bHarvesting || !IsValid(Inventory) || !Grid->Gathering->IsUnlocked() || !CanReach(Player, Cell)) return false;
	auto* Bush = Bushes.FindByPredicate([Cell](const FPSBushRecord& Record) { return Record.Cell == Cell && Record.RemainingHarvests > 0; });
	const int64 Now = Grid->Gathering->Now();
	if (!Bush || Now < Bush->ReadyAt) return false;
	TGuardValue<bool> Guard(bHarvesting, true);
	FRandomStream Random(Grid->GetGatheringSeed() ^ GetTypeHash(Cell) ^ Bush->HarvestSequence++);
	const int32 Quantity = Random.RandRange(1, 3);
	const FName ItemId = Bush->ItemId;
	--Bush->RemainingHarvests;
	Bush->ReadyAt = Now + (Bush->RemainingHarvests > 0 ? 2 : 3) * 48;
	Grid->SaveGatheringState();

	int32 Accepted = Quantity;
	while (Accepted > 0 && !Inventory->CanAddItemVariant(EPSItemType::GatheredFruit, ItemId, Accepted)) --Accepted;
	if (Accepted > 0 && !Inventory->AddItemVariant(EPSItemType::GatheredFruit, ItemId, Accepted)) Accepted = 0;
	if (Accepted < Quantity)
	{
		Grid->Gathering->DropItem(Cell, EPSItemType::GatheredFruit, ItemId, Quantity - Accepted);
	}
	RefreshActors();
	return true;
}

void UPSBushWorldComponent::RefreshActors()
{
	auto* Grid = CastChecked<APSGridWorld>(GetOwner());
	for (auto It = Actors.CreateIterator(); It; ++It)
	{
		if (HasBush(It.Key()) && Grid->GetGroundTile(It.Key()) == EPSTileType::Grass) continue;
		if (IsValid(It.Value())) It.Value()->Destroy();
		It.RemoveCurrent();
	}
	if (!bEnabled || !Texture) return;
	for (const auto& Bush : Bushes)
	{
		if (Bush.RemainingHarvests <= 0 || Grid->GetGroundTile(Bush.Cell) != EPSTileType::Grass) continue;
		auto* Actor = Cast<AStaticMeshActor>(Actors.FindRef(Bush.Cell));
		if (!IsValid(Actor))
		{
			Actor = GetWorld()->SpawnActor<AStaticMeshActor>();
			if (!Actor) continue;
			auto* Mesh = Actor->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			auto* Material = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Game/Art/Tiles/Farm/M_FarmTileUV.M_FarmTileUV")), Actor);
			Material->SetTextureParameterValue(TEXT("SpriteTexture"), Texture);
			Mesh->SetMaterial(0, Material);
			Actor->SetActorLocation(Grid->CellToWorldCenter(Bush.Cell) + FVector(0, 0, Grid->GetCellSize() * 0.7f));
			Actor->SetActorRotation(FRotator(0, BushRestYaw, 0));
			// The entire transparent source canvas fits within one ground tile.
			Actor->SetActorScale3D(FVector(Grid->GetCellSize() / 100.f,
				Grid->GetCellSize() / 100.f * Texture->GetSizeY() / FMath::Max(1, Texture->GetSizeX()), 1));
			Actors.Add(Bush.Cell, Actor);
		}
	}
}

void UPSBushWorldComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(Delta, Type, Tick);
	if (!bEnabled) return;
	Initialize();
	auto* Grid = CastChecked<APSGridWorld>(GetOwner());
	const int64 Now = Grid->Gathering->Now();
	if (Now >= NextSpawnCheck)
	{
		NextSpawnCheck = Now + 2;
		bool bChanged = false;
		FRandomStream Random(Grid->GetGatheringSeed() ^ static_cast<int32>(Now));
		for (auto& Bush : Bushes)
			if (Bush.RemainingHarvests == 0 && Now >= Bush.ReadyAt) bChanged |= Place(Bush, Random);
		if (bChanged) Grid->SaveGatheringState();
	}
	RefreshActors();
}

void UPSBushWorldComponent::ClearActors()
{
	for (const auto& Pair : Actors) if (IsValid(Pair.Value)) Pair.Value->Destroy();
	Actors.Reset();
}

void UPSBushWorldComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearActors();
	Super::EndPlay(Reason);
}
