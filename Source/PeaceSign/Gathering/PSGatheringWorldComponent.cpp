#include "PSGatheringWorldComponent.h"
#include "../Data/PSGameDataTables.h"
#include "PSGatheringTreeActor.h"
#include "../World/PSGridWorld.h"
#include "../Inventory/PSWorldItemActor.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UPSGatheringWorldComponent::UPSGatheringWorldComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

FPSTreeRecord* UPSGatheringWorldComponent::FindTree(const FIntPoint Cell)
{
	return Trees.FindByPredicate([Cell](const FPSTreeRecord& Tree) { return Tree.Cell == Cell; });
}

bool UPSGatheringWorldComponent::HasTree(const FIntPoint Cell) const
{
	return Trees.ContainsByPredicate([Cell](const FPSTreeRecord& Tree) { return Tree.Cell == Cell; });
}

FPSTreeRecord UPSGatheringWorldComponent::GetTree(const FIntPoint Cell) const
{
	if (const auto* Tree = Trees.FindByPredicate([Cell](const FPSTreeRecord& T) { return T.Cell == Cell; })) return *Tree;
	FPSTreeRecord Empty;
	Empty.Cell = Cell;
	Empty.Health = 0;
	return Empty;
}

FPSTreeDefinition UPSGatheringWorldComponent::Definition(const EPSTreeSpecies Species) const
{
	FPSTreeDefinition Result = PSGathering::GetTree(Species);
	Result.MaxHealth = FMath::Max(1, TreeMaxHealth);
	return Result;
}

FPSAxeDefinition UPSGatheringWorldComponent::AxeDefinition(const FName ItemId) const
{
	FPSAxeDefinition Result = PSGathering::GetAxe(ItemId);
	if (AxeTable && AxeTable->GetRowStruct() == FPSAxeDefinition::StaticStruct())
		if (const auto* Row = AxeTable->FindRow<FPSAxeDefinition>(ItemId, TEXT("Gathering"))) Result = *Row;
	Result.Damage = FMath::Max(1, Result.Damage);
	Result.StaminaCost = FMath::IsFinite(Result.StaminaCost) ? FMath::Max(0.0f, Result.StaminaCost) : 10;
	Result.ExtraLogChance = FMath::IsFinite(Result.ExtraLogChance) ? FMath::Clamp(Result.ExtraLogChance, 0.0f, 1.0f) : 0;
	return Result;
}

int64 UPSGatheringWorldComponent::Now() const
{
	const auto* Clock = GetWorld() ? GetWorld()->GetSubsystem<UPSGameTimeSubsystem>() : nullptr;
	return Clock ? Clock->GetHalfHourIndex() : 12;
}

void UPSGatheringWorldComponent::Persist()
{
	if (auto* Grid = Cast<APSGridWorld>(GetOwner())) Grid->SaveGatheringState();
}

void UPSGatheringWorldComponent::SetUnlocked(const bool bValue)
{
	bUnlocked = bValue;
	Persist();
}

void UPSGatheringWorldComponent::InitializeResources()
{
	if (bResourcesInitialized) { RefreshActors(); return; }
	bResourcesInitialized = true;
	auto* Grid = Cast<APSGridWorld>(GetOwner());
	if (Grid && bEnableTestArea)
	{
		// Four authored 8x8 patches. West = meadow, east = forest. Seed controls species and placement.
		for (int32 PatchY = 0; PatchY < 2; ++PatchY)
			for (int32 PatchX = 0; PatchX < 2; ++PatchX)
			{
				FRandomStream Random(Grid->GetGatheringSeed() ^ ((PatchX + 1) * 73856093) ^ ((PatchY + 1) * 19349663));
				int32 Placed = 0;
				for (int32 Attempt = 0; Attempt < 96 && Placed < 3; ++Attempt)
				{
					const FIntPoint Cell = TestAreaOrigin + FIntPoint(PatchX * 8 + Random.RandRange(0, 7), PatchY * 8 + Random.RandRange(0, 7));
					if (!Grid->CanPlaceGatheringTree(Cell) || Trees.ContainsByPredicate([Cell](const FPSTreeRecord& Other)
						{ return FMath::Abs(Other.Cell.X - Cell.X) < 2 && FMath::Abs(Other.Cell.Y - Cell.Y) < 2; })) continue;
					FPSTreeRecord Tree;
					Tree.Cell = Cell;
					const bool bMain = Random.RandRange(0, 99) < 80;
					Tree.Species = (PatchX == 0 ? bMain : !bMain) ? EPSTreeSpecies::Oak : EPSTreeSpecies::Maple;
					Tree.Health = Definition(Tree.Species).MaxHealth;
					Trees.Add(Tree);
					++Placed;
				}
			}
	}
	Persist();
	RefreshActors();
}

void UPSGatheringWorldComponent::Restore(const TArray<FPSTreeRecord>& InTrees, const TArray<FPSGatheringDrop>& InDrops,
	const bool bInitialized, const bool bSavedUnlocked)
{
	ClearActors();
	Trees = InTrees;
	Drops = InDrops;
	for (auto& Drop : Drops) Drop.Item.ItemId = UPSGameDataLibrary::CanonicalItemID(Drop.Item.ItemId);
	bResourcesInitialized = bInitialized;
	bUnlocked = bSavedUnlocked;
}

void UPSGatheringWorldComponent::ClearActors()
{
	for (const auto& Pair : TreeActors) if (IsValid(Pair.Value)) Pair.Value->Destroy();
	for (const auto& Pair : DropActors) if (IsValid(Pair.Value)) Pair.Value->Destroy();
	TreeActors.Reset();
	DropActors.Reset();
}

void UPSGatheringWorldComponent::ResetResources()
{
	ClearActors();
	Trees.Reset();
	Drops.Reset();
	bResourcesInitialized = false;
	InitializeResources();
}

void UPSGatheringWorldComponent::AddDrop(const FIntPoint Cell, const EPSItemType Type, const FName ItemId, const int32 Quantity, const float RevealDelay)
{
	if (Quantity <= 0) return;
	FPSGatheringDrop Drop;
	Drop.Id = FGuid::NewGuid();
	Drop.Cell = Cell;
	Drop.Item.ItemType = Type;
	Drop.Item.ItemId = UPSGameDataLibrary::CanonicalItemID(ItemId);
	Drop.Item.Quantity = Quantity;
	Drop.RevealAtWorldSeconds = GetWorld()->GetTimeSeconds() + RevealDelay;
	Drops.Add(Drop);
}

bool UPSGatheringWorldComponent::ValidateRewardData(const FIntPoint Cell, const bool bChopping, FString& Error) const
{
	Error.Reset();
	if (!UsesDropTables()) return true; // Legacy worlds without the three tables retain prototype rewards.
	if (!UPSGameDataLibrary::ValidateTreeTables(ItemTable, TreeCutDropTable, TreeShakeDropTable, Error)) return false;
	const auto Data = Definition(GetTree(Cell).Species);
	TArray<FPSItemStack> Preview;
	return bChopping ? UPSGameDataLibrary::RollTreeCutDrop(ItemTable, TreeCutDropTable, Data.CutDropID, 0, Preview, Error)
		: UPSGameDataLibrary::RollTreeShakeDrop(ItemTable, TreeShakeDropTable, Data.ShakeDropID, 0, Preview, Error);
}

bool UPSGatheringWorldComponent::Strike(const FIntPoint Cell, const FPSAxeDefinition& Axe)
{
	auto* Tree = FindTree(Cell);
	if (!IsUnlocked() || !Tree || Tree->Health <= 0 || Axe.Damage <= 0) return false;
	const int32 NextHealth = FMath::Max(0, Tree->Health - Axe.Damage);
	TArray<FPSItemStack> Rewards;
	const int32 Seed = GetTypeHash(Cell) ^ CastChecked<APSGridWorld>(GetOwner())->GetGatheringSeed() ^ Tree->RewardSequence;
	FRandomStream Random(Seed ^ 0x17365AB);
	if (NextHealth == 0)
	{
		const auto Data = Definition(Tree->Species);
		if (UsesDropTables())
		{
			FString Error;
			if (!ValidateRewardData(Cell, true, Error) || !UPSGameDataLibrary::RollTreeCutDrop(ItemTable, TreeCutDropTable, Data.CutDropID, Seed, Rewards, Error))
			{ UE_LOG(LogTemp, Warning, TEXT("Tree cut data invalid: %s"), *Error); return false; }
			if (Random.FRand() < Axe.ExtraLogChance)
				for (auto& Reward : Rewards) if (Reward.ItemType == EPSItemType::Wood) { ++Reward.Quantity; break; }
		}
		else
		{
			FPSItemStack Logs; Logs.ItemType = EPSItemType::Wood; Logs.ItemId = Data.LogId;
			Logs.Quantity = Data.LogQuantity + (Random.FRand() < Axe.ExtraLogChance ? 1 : 0); Rewards.Add(Logs);
			FPSItemStack Seeds; Seeds.ItemType = EPSItemType::TreeSeed; Seeds.ItemId = Data.SeedId;
			Seeds.Quantity = Data.SeedQuantity; Rewards.Add(Seeds);
		}
	}
	Tree->Health = NextHealth;
	if (auto* Actor = TreeActors.FindRef(Cell).Get()) Actor->React(Tree->Health == 0);
	if (Tree->Health == 0)
	{
		++Tree->RewardSequence;
		for (const auto& Reward : Rewards) AddDrop(Cell, Reward.ItemType, Reward.ItemId, Reward.Quantity, 0.6f);
	}
	Persist(); // Pending rewards survive even if actor spawning fails.
	RefreshActors();
	return true;
}

bool UPSGatheringWorldComponent::Shake(const FIntPoint Cell, bool* bOutHadReward)
{
	if (bOutHadReward) *bOutHadReward = false;
	auto* Tree = FindTree(Cell);
	if (!IsUnlocked() || !Tree || Tree->Health <= 0) return false;
	if (auto* Actor = TreeActors.FindRef(Cell).Get()) Actor->React(false);
	if (Now() < Tree->NextShakeHalfHour) return false;
	const auto Data = Definition(Tree->Species);
	TArray<FPSItemStack> Rewards;
	if (UsesDropTables())
	{
		FString Error;
		const int32 Seed = GetTypeHash(Cell) ^ CastChecked<APSGridWorld>(GetOwner())->GetGatheringSeed() ^ Tree->RewardSequence ^ 0x7AB435;
		if (!ValidateRewardData(Cell, false, Error) || !UPSGameDataLibrary::RollTreeShakeDrop(ItemTable, TreeShakeDropTable, Data.ShakeDropID, Seed, Rewards, Error))
		{ UE_LOG(LogTemp, Warning, TEXT("Tree shake data invalid: %s"), *Error); return false; }
	}
	else
	{
		FPSItemStack Seeds; Seeds.ItemType = EPSItemType::TreeSeed; Seeds.ItemId = Data.SeedId; Seeds.Quantity = Data.SeedQuantity;
		Rewards.Add(Seeds);
	}
	Tree->NextShakeHalfHour = Now() + PSGathering::ShakeCooldown;
	++Tree->RewardSequence;
	for (const auto& Reward : Rewards) AddDrop(Cell, Reward.ItemType, Reward.ItemId, Reward.Quantity);
	if (bOutHadReward) *bOutHadReward = !Rewards.IsEmpty();
	Persist();
	RefreshActors();
	return true;
}

bool UPSGatheringWorldComponent::CanReach(const FIntPoint From, const FIntPoint Target, const AActor* Interactor) const
{
	const auto* Grid = Cast<APSGridWorld>(GetOwner());
	if (!Grid || !HasTree(Target)) return false;
	const FIntPoint Delta = Target - From;
	const int32 Distance = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
	const auto Ground = Grid->GetGroundTile(From);
	if (Distance < 1 || Distance > 2 || Ground == EPSTileType::Empty || Ground == EPSTileType::Water) return false;
	if (FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y)) == 2)
	{
		const FIntPoint Between = From + FIntPoint(Delta.X / 2, Delta.Y / 2);
		if (Grid->GetGroundTile(Between) == EPSTileType::Water || Grid->GetWorldObjectType(Between) != EPSWorldObjectType::None) return false;
	}
	// Diagonal corners must be clear too.
	if (Delta.X != 0 && Delta.Y != 0)
		for (const FIntPoint Side : {From + FIntPoint(Delta.X, 0), From + FIntPoint(0, Delta.Y)})
			if (Grid->GetGroundTile(Side) == EPSTileType::Water || Grid->GetWorldObjectType(Side) != EPSWorldObjectType::None) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GatheringReach), false);
	Query.AddIgnoredActor(Interactor ? Interactor : UGameplayStatics::GetPlayerPawn(this, 0));
	if (const auto* TreeActor = TreeActors.FindRef(Target).Get()) Query.AddIgnoredActor(TreeActor);
	if (GetWorld()->LineTraceTestByChannel(Grid->CellToWorldCenter(From) + FVector(0, 0, 50),
		Grid->CellToWorldCenter(Target) + FVector(0, 0, 50), ECC_Pawn, Query)) return false;
	return true;
}

bool UPSGatheringWorldComponent::CollectDrop(const FGuid Id, UPSInventoryComponent* Inventory)
{
	if (bCollecting || !IsValid(Inventory)) return false;
	const int32 Index = Drops.IndexOfByPredicate([Id](const FPSGatheringDrop& Drop) { return Drop.Id == Id; });
	if (Index == INDEX_NONE) return false;
	TGuardValue<bool> Guard(bCollecting, true);
	const FPSItemStack Item = Drops[Index].Item;
	if (!Inventory->AddItemVariant(Item.ItemType, Item.ItemId, Item.Quantity, Item.CropId, Item.CurrentDurability)) return false;
	Drops.RemoveAt(Index);
	DropActors.Remove(Id);
	Persist();
	return true;
}

void UPSGatheringWorldComponent::RefreshActors()
{
	auto* Grid = Cast<APSGridWorld>(GetOwner());
	const APawn* Pawn = GetWorld() ? UGameplayStatics::GetPlayerPawn(this, 0) : nullptr;
	if (!Grid || !Pawn || !bResourcesInitialized) return;
	const FIntPoint PlayerCell = Grid->WorldToCell(Pawn->GetActorLocation());
	const auto Visible = [PlayerCell](const FIntPoint Cell) { return FMath::Abs(Cell.X - PlayerCell.X) <= 80 && FMath::Abs(Cell.Y - PlayerCell.Y) <= 80; };
	for (auto It = TreeActors.CreateIterator(); It; ++It)
		if (!IsValid(It.Value()) || !Visible(It.Key())) { if (IsValid(It.Value())) It.Value()->Destroy(); It.RemoveCurrent(); }
	for (auto It = DropActors.CreateIterator(); It; ++It)
	{
		const auto* Drop = Drops.FindByPredicate([Id = It.Key()](const FPSGatheringDrop& D) { return D.Id == Id; });
		if (!IsValid(It.Value()) || !Drop || !Visible(Drop->Cell)) { if (IsValid(It.Value())) It.Value()->Destroy(); It.RemoveCurrent(); }
	}
	for (const auto& Tree : Trees)
	{
		if (!Visible(Tree.Cell) || TreeActors.Contains(Tree.Cell)) continue;
		auto* Actor = GetWorld()->SpawnActor<APSGatheringTreeActor>(Grid->CellToWorldCenter(Tree.Cell), FRotator::ZeroRotator);
		if (Actor) { Actor->Configure(Tree, Grid->GetCellSize()); TreeActors.Add(Tree.Cell, Actor); }
	}
	// FinishSpawning can trigger immediate overlaps. Iterate a copy to avoid invalidated arrays.
	const TArray<FPSGatheringDrop> Snapshot = Drops;
	for (const auto& Drop : Snapshot)
	{
		if (!Visible(Drop.Cell) || DropActors.Contains(Drop.Id) || GetWorld()->GetTimeSeconds() < Drop.RevealAtWorldSeconds) continue;
		const FVector Offset = Drop.Item.ItemType == EPSItemType::TreeSeed ? FVector(25, 0, 18) : FVector(-25, 0, 18);
		const FTransform Transform(FRotator::ZeroRotator, Grid->CellToWorldCenter(Drop.Cell) + Offset);
		auto* Actor = GetWorld()->SpawnActorDeferred<APSWorldItemActor>(APSWorldItemActor::StaticClass(), Transform);
		if (!Actor) continue;
		Actor->InitializeGatheringItem(Drop.Item, this, Drop.Id);
		DropActors.Add(Drop.Id, Actor);
		Actor->FinishSpawning(Transform);
	}
}

void UPSGatheringWorldComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshActors();
}

void UPSGatheringWorldComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearActors();
	Super::EndPlay(Reason);
}
