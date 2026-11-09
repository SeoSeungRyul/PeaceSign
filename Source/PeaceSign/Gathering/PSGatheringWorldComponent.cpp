#include "PSGatheringWorldComponent.h"
#include "../Data/PSGameDataTables.h"
#include "../Fishing/PSFishingTypes.h"
#include "PSGatheringTreeActor.h"
#include "../World/PSGridWorld.h"
#include "../Inventory/PSWorldItemActor.h"
#include "../Inventory/PSInventoryComponent.h"
#include "../PSPlayerCharacter.h"
#include "../PSPlayerStatsComponent.h"
#include "../Time/PSGameTimeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

namespace
{
	TOptional<EPSTreeSpecies> GetSeedSpecies(const FPSItemStack& Seed)
	{
		if (Seed.IsEmpty() || Seed.ItemType != EPSItemType::TreeSeed) return {};
		const FName ItemId = UPSGameDataLibrary::CanonicalItemID(Seed.ItemId);
		for (int32 Index = 0; Index <= static_cast<int32>(EPSTreeSpecies::Cactus); ++Index)
		{
			const auto Species = static_cast<EPSTreeSpecies>(Index);
			if (PSGathering::GetTree(Species).SeedId == ItemId) return Species;
		}
		return {};
	}
}

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

bool UPSGatheringWorldComponent::OccupiesTreeCell(const FIntPoint Cell) const
{
	const auto* Actor = TreeActors.FindRef(Cell).Get();
	return HasTree(Cell) || (IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->IsFalling());
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

UTexture2D* UPSGatheringWorldComponent::GetItemIcon(const FPSItemStack& Item) const
{
	FPSItemTableRow Data;
	if (!UPSGameDataLibrary::FindItem(ItemTable, Item.ItemId, Data) || Data.IconID.IsNone()
		|| !ItemIconTable || ItemIconTable->GetRowStruct() != FPSIconDefinition::StaticStruct()) return nullptr;
	const auto* Icon = ItemIconTable->FindRow<FPSIconDefinition>(Data.IconID, TEXT("Gathering item icon lookup"), false);
	return Icon ? Icon->Image.LoadSynchronous() : nullptr;
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
	EnsurePlacedTrees();
	const bool bMatured = MatureSaplings();
	if (bResourcesInitialized)
	{
		if (bMatured) Persist();
		RefreshActors();
		return;
	}
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

int32 UPSGatheringWorldComponent::EnsurePlacedTrees()
{
	auto* Grid = Cast<APSGridWorld>(GetOwner());
	if (!Grid) return 0;
	int32 Added = 0;
	for (const FPSTreeRecord& Authored : PlacedTrees)
	{
		const FName Id = Authored.PlacementId.IsNone()
			? FName(*FString::Printf(TEXT("Placed.%d.%d"), Authored.Cell.X, Authored.Cell.Y)) : Authored.PlacementId;
		if (RemovedPlacements.Contains(Id)
			|| Trees.ContainsByPredicate([Id](const FPSTreeRecord& Tree) { return Tree.PlacementId == Id; })) continue;
		bool bPlaced = false;
		for (int32 Radius = 0; Radius <= 4 && !bPlaced; ++Radius)
			for (int32 X = -Radius; X <= Radius && !bPlaced; ++X)
				for (int32 Y = -Radius; Y <= Radius && !bPlaced; ++Y)
				{
					if (FMath::Max(FMath::Abs(X), FMath::Abs(Y)) != Radius) continue;
					const FIntPoint Cell = Authored.Cell + FIntPoint(X,Y);
					if (!Grid->CanPlaceGatheringTree(Cell) || Trees.ContainsByPredicate([Cell](const FPSTreeRecord& Other)
						{ return FMath::Abs(Other.Cell.X-Cell.X)<3 && FMath::Abs(Other.Cell.Y-Cell.Y)<3; })) continue;
					bool bApproachable = false;
					for (const FIntPoint Delta : {FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
					{
						const FIntPoint Side = Cell + Delta;
						const auto Ground = Grid->GetGroundTile(Side);
						bApproachable |= (Ground == EPSTileType::Grass || Ground == EPSTileType::Dirt)
							&& Grid->GetWorldObjectType(Side) == EPSWorldObjectType::None;
					}
					if (!bApproachable) continue;
					FPSTreeRecord Tree; Tree.Cell = Cell; Tree.Species = Authored.Species;
					Tree.Health = Definition(Tree.Species).MaxHealth; Tree.PlacementId = Id;
					Trees.Add(Tree); ++Added; bPlaced = true;
				}
	}
	if (Added > 0) Persist();
	return Added;
}

void UPSGatheringWorldComponent::Restore(const TArray<FPSTreeRecord>& InTrees, const TArray<FPSGatheringDrop>& InDrops,
	const bool bInitialized, const bool bSavedUnlocked, const TSet<FName>& InRemovedPlacements)
{
	ClearActors();
	RemovedPlacements = InRemovedPlacements;
	Trees.Reset(InTrees.Num());
	for (const auto& Tree : InTrees)
	{
		if (Tree.Health <= 0)
		{
			// Older saves kept stumps as trees. Only their authored identity still matters.
			if (!Tree.PlacementId.IsNone()) RemovedPlacements.Add(Tree.PlacementId);
			continue;
		}
		auto& Restored = Trees.Add_GetRef(Tree);
		if (Restored.IsSapling()) Restored.Health = FMath::Min(Restored.Health, PSGathering::SaplingHealth);
	}
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
	RemovedPlacements.Reset();
	Drops.Reset();
	bResourcesInitialized = false;
	InitializeResources();
}

void UPSGatheringWorldComponent::AddDrop(const FIntPoint Cell, const EPSItemType Type, const FName ItemId, const int32 Quantity, const float RevealDelay)
{
	FPSItemStack Item;
	Item.ItemType = Type;
	Item.ItemId = ItemId;
	Item.Quantity = Quantity;
	AddDropStack(Cell, Item, RevealDelay);
}

void UPSGatheringWorldComponent::AddDropStack(const FIntPoint Cell, const FPSItemStack& Item, const float RevealDelay)
{
	if (Item.IsEmpty()) return;
	FPSGatheringDrop Drop;
	Drop.Id = FGuid::NewGuid();
	Drop.Cell = Cell;
	Drop.Item = Item;
	Drop.Item.ItemId = UPSGameDataLibrary::CanonicalItemID(Item.ItemId);
	Drop.RevealAtWorldSeconds = GetWorld()->GetTimeSeconds() + RevealDelay;
	Drops.Add(Drop);
}

bool UPSGatheringWorldComponent::ValidateRewardData(const FIntPoint Cell, const bool bChopping, FString& Error) const
{
	Error.Reset();
	if (bChopping && GetTree(Cell).IsSapling()) return true;
	if (!UsesDropTables()) return true; // Legacy worlds without the three tables retain prototype rewards.
	if (!UPSGameDataLibrary::ValidateTreeTables(ItemTable, TreeCutDropTable, TreeShakeDropTable, Error)) return false;
	const auto Data = Definition(GetTree(Cell).Species);
	TArray<FPSItemStack> Preview;
	return bChopping ? UPSGameDataLibrary::RollTreeCutDrop(ItemTable, TreeCutDropTable, Data.CutDropID, 0, Preview, Error)
		: UPSGameDataLibrary::RollTreeShakeDrop(ItemTable, TreeShakeDropTable, Data.ShakeDropID, 0, Preview, Error);
}

bool UPSGatheringWorldComponent::CanPlantSeed(const APawn* Pawn, const FIntPoint Cell, const FPSItemStack& Seed) const
{
	const auto* Grid = Cast<APSGridWorld>(GetOwner());
	if (!Grid || !IsValid(Pawn) || !IsUnlocked() || !GetSeedSpecies(Seed).IsSet()) return false;
	const auto Ground = Grid->GetGroundTile(Cell);
	return (Ground == EPSTileType::Grass || Ground == EPSTileType::Water)
		&& Grid->GetWorldObjectType(Cell) == EPSWorldObjectType::None && Grid->GetCropType(Cell) == EPSCropType::None
		&& CanReachCell(Grid->WorldToCell(Pawn->GetActorLocation()), Cell, Pawn);
}

bool UPSGatheringWorldComponent::PlantSeed(const APawn* Pawn, const FIntPoint Cell,
	UPSInventoryComponent* Inventory, const int32 HotbarSlot)
{
	if (!IsValid(Inventory)) return false;
	const FPSItemStack Seed = Inventory->GetHotbarSlot(HotbarSlot);
	if (!CanPlantSeed(Pawn, Cell, Seed)) return false;

	FPSTreeRecord Tree;
	Tree.Cell = Cell;
	Tree.Species = GetSeedSpecies(Seed).GetValue();
	Tree.Health = PSGathering::SaplingHealth;
	Tree.MaturesAtHalfHour = Now() + PSGathering::SaplingGrowthTime;
	Trees.Add(Tree);
	if (!Inventory->RemoveFromHotbarSlot(HotbarSlot, 1))
	{
		Trees.Pop();
		return false;
	}
	Persist();
	RefreshActors();
	return true;
}

bool UPSGatheringWorldComponent::MatureSaplings()
{
	const int64 Time = Now();
	bool bChanged = false;
	for (auto& Tree : Trees)
	{
		if (!Tree.IsSapling() || Time < Tree.MaturesAtHalfHour) continue;
		Tree.MaturesAtHalfHour = -1;
		Tree.Health = Definition(Tree.Species).MaxHealth;
		if (auto* Actor = TreeActors.FindRef(Tree.Cell).Get()) ConfigureTreeActor(Actor, Tree);
		bChanged = true;
	}
	return bChanged;
}

bool UPSGatheringWorldComponent::Strike(const FIntPoint Cell, const FPSAxeDefinition& Axe, const APawn* Interactor)
{
	const int32 Index = Trees.IndexOfByPredicate([Cell](const FPSTreeRecord& Tree) { return Tree.Cell == Cell; });
	if (!IsUnlocked() || Index == INDEX_NONE || Axe.Damage <= 0) return false;
	auto& Tree = Trees[Index];
	const int32 NextHealth = FMath::Max(0, Tree.Health - Axe.Damage);
	TArray<FPSItemStack> Rewards;
	if (NextHealth == 0 && !Tree.IsSapling())
	{
		const auto Data = Definition(Tree.Species);
		const int32 Seed = GetTypeHash(Cell) ^ CastChecked<APSGridWorld>(GetOwner())->GetGatheringSeed() ^ Tree.RewardSequence;
		FRandomStream Random(Seed ^ 0x17365AB);
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
	Tree.Health = NextHealth;
	if (auto* Actor = TreeActors.FindRef(Cell).Get()) Actor->React(NextHealth == 0);
	if (NextHealth == 0)
	{
		if (!Tree.PlacementId.IsNone()) RemovedPlacements.Add(Tree.PlacementId);
		Trees.RemoveAt(Index);
		if (!Rewards.IsEmpty())
		{
			const FIntPoint DropCell = GetDropCell(Cell, Interactor);
			for (const auto& Reward : Rewards)
				AddDrop(DropCell, Reward.ItemType, Reward.ItemId, Reward.Quantity, APSGatheringTreeActor::FallDuration);
		}
	}
	Persist(); // Pending rewards survive even if actor spawning fails.
	RefreshActors();
	return true;
}

bool UPSGatheringWorldComponent::Shake(const FIntPoint Cell, bool* bOutHadReward, const APawn* Interactor)
{
	if (bOutHadReward) *bOutHadReward = false;
	auto* Tree = FindTree(Cell);
	if (!IsUnlocked() || !Tree || Tree->IsSapling() || Tree->Health <= 0) return false;
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
	const FIntPoint DropCell = GetDropCell(Cell, Interactor);
	for (const auto& Reward : Rewards) AddDrop(DropCell, Reward.ItemType, Reward.ItemId, Reward.Quantity);
	if (bOutHadReward) *bOutHadReward = !Rewards.IsEmpty();
	Persist();
	if (const auto* Player = Cast<APSPlayerCharacter>(Interactor))
	{
		auto* Stats = Player->StatsComponent.Get();
		if (Stats && Rewards.ContainsByPredicate([](const FPSItemStack& Reward) { return Reward.ItemId == TEXT("030208"); }))
		{
			const float Damage = FMath::CeilToFloat(Stats->MaxHealth * 0.05f);
			Stats->ApplyDamage(FMath::Min(Damage, Stats->Health - 1.0f));
		}
	}
	RefreshActors();
	return true;
}

bool UPSGatheringWorldComponent::CanReach(const FIntPoint From, const FIntPoint Target, const AActor* Interactor) const
{
	return HasTree(Target) && CanReachCell(From, Target, Interactor);
}

bool UPSGatheringWorldComponent::CanReachCell(const FIntPoint From, const FIntPoint Target, const AActor* Interactor) const
{
	const auto* Grid = Cast<APSGridWorld>(GetOwner());
	if (!Grid) return false;
	const FIntPoint Delta = Target - From;
	const int32 Distance = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
	const auto Ground = Grid->GetGroundTile(From);
	if (Distance < 1 || Distance > 2 || Ground == EPSTileType::Empty || Ground == EPSTileType::Water
		|| Ground == EPSTileType::Stone) return false;
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
	const FVector Start = Grid->CellToWorldCenter(From) + FVector(0, 0, 50);
	FVector End = Grid->CellToWorldCenter(Target) + FVector(0, 0, 50);
	if (Grid->GetGroundTile(Target) == EPSTileType::Water)
	{
		// Stop just before the target's water collider. Any intervening blocker still counts.
		const float Cells = FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y));
		End -= (End - Start) * (0.51f / Cells);
	}
	if (GetWorld()->LineTraceTestByChannel(Start, End, ECC_Pawn, Query)) return false;
	return true;
}

FIntPoint UPSGatheringWorldComponent::GetDropCell(const FIntPoint TreeCell, const APawn* Interactor) const
{
	const auto* Grid = CastChecked<APSGridWorld>(GetOwner());
	if (Grid->GetGroundTile(TreeCell) != EPSTileType::Water) return TreeCell;
	if (Interactor)
	{
		const FIntPoint Shore = Grid->WorldToCell(Interactor->GetActorLocation());
		if (CanReachCell(Shore, TreeCell, Interactor)) return Shore;
	}
	// Direct callers without a pawn can still fell or shake a water tree.
	for (const FIntPoint Direction : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
	{
		const FIntPoint Shore = TreeCell + Direction;
		if (Grid->GetWorldObjectType(Shore) == EPSWorldObjectType::None && CanReachCell(Shore, TreeCell, Interactor)) return Shore;
	}
	return TreeCell;
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

void UPSGatheringWorldComponent::DropItem(const FIntPoint Cell, const EPSItemType Type, const FName ItemId, const int32 Quantity)
{
	AddDrop(Cell, Type, ItemId, Quantity);
	Persist();
	RefreshActors();
}

void UPSGatheringWorldComponent::DropItemStack(const FIntPoint Cell, const FPSItemStack& Item)
{
	AddDropStack(Cell, Item);
	Persist();
	RefreshActors();
}

void UPSGatheringWorldComponent::ConfigureTreeActor(APSGatheringTreeActor* Actor, const FPSTreeRecord& Tree) const
{
	Actor->Configure(Tree, CastChecked<APSGridWorld>(GetOwner())->GetCellSize(),
		TreeSprites.FindRef(Tree.Species), SpriteHeightInCells);
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
		if (Actor) { ConfigureTreeActor(Actor, Tree); TreeActors.Add(Tree.Cell, Actor); }
	}
	// FinishSpawning can trigger immediate overlaps. Iterate a copy to avoid invalidated arrays.
	const TArray<FPSGatheringDrop> Snapshot = Drops;
	for (const auto& Drop : Snapshot)
	{
		if (!Visible(Drop.Cell) || DropActors.Contains(Drop.Id) || GetWorld()->GetTimeSeconds() < Drop.RevealAtWorldSeconds) continue;
		const float Side = Drop.Item.ItemType == EPSItemType::TreeSeed ? 0.29f : -0.29f;
		const float CellSize = Grid->GetCellSize();
		const FVector Offset = FVector(-0.28f, Side, 0.18f) * CellSize;
		const FTransform Transform(FRotator::ZeroRotator, Grid->CellToWorldCenter(Drop.Cell) + Offset);
		auto* Actor = GetWorld()->SpawnActorDeferred<APSWorldItemActor>(APSWorldItemActor::StaticClass(), Transform);
		if (!Actor) continue;
		const bool bPlayHop = Drop.RevealAtWorldSeconds > 0 && GetWorld()->GetTimeSeconds() < Drop.RevealAtWorldSeconds + 0.5;
		Actor->InitializeGatheringItem(Drop.Item, this, Drop.Id, GetItemIcon(Drop.Item), CellSize, bPlayHop);
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
