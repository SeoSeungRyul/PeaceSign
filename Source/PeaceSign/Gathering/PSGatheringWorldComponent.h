#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSGatheringTypes.h"
#include "PSGatheringWorldComponent.generated.h"

class APSGridWorld;
class APSGatheringTreeActor;
class APSWorldItemActor;
class UPSInventoryComponent;

UCLASS(ClassGroup=(Gathering), meta=(BlueprintSpawnableComponent))
class PEACESIGN_API UPSGatheringWorldComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPSGatheringWorldComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	// A development area only. No automatic biome generation or NPC unlock is implied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Development") bool bEnableTestArea = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Development") bool bDevelopmentUnlocked = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Development") FIntPoint TestAreaOrigin = FIntPoint(-20, 6);
	/** Initial HP for newly generated trees. Saved trees keep their remaining HP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Trees", meta=(ClampMin="1")) int32 TreeMaxHealth = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Data") TObjectPtr<UDataTable> AxeTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Data") TObjectPtr<UDataTable> ItemTable;
	/** FPSIconDefinition rows referenced by ItemTable.IconID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Data") TObjectPtr<UDataTable> ItemIconTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Data") TObjectPtr<UDataTable> TreeCutDropTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gathering|Data") TObjectPtr<UDataTable> TreeShakeDropTable;
	UFUNCTION(BlueprintPure, Category="Gathering") bool HasTree(FIntPoint Cell) const;
	UFUNCTION(BlueprintPure, Category="Gathering") FPSTreeRecord GetTree(FIntPoint Cell) const;
	UFUNCTION(BlueprintCallable, Category="Gathering") void SetUnlocked(bool bUnlocked);
	bool IsUnlocked() const { return bUnlocked || bDevelopmentUnlocked; }
	FPSTreeDefinition Definition(EPSTreeSpecies Species) const;
	FPSAxeDefinition AxeDefinition(FName ItemId) const;
	void InitializeResources();
	void Restore(const TArray<FPSTreeRecord>& InTrees, const TArray<FPSGatheringDrop>& InDrops, bool bInitialized, bool bSavedUnlocked);
	void ResetResources();
	bool Strike(FIntPoint Cell, const FPSAxeDefinition& Axe);
	bool Shake(FIntPoint Cell, bool* bOutHadReward = nullptr);
	bool ValidateRewardData(FIntPoint Cell, bool bChopping, FString& Error) const;
	bool CollectDrop(FGuid Id, UPSInventoryComponent* Inventory);
	bool CanReach(FIntPoint From, FIntPoint Target, const AActor* Interactor = nullptr) const;
	int64 Now() const;
	void RefreshActors();
	const TArray<FPSTreeRecord>& GetRecords() const { return Trees; }
	const TArray<FPSGatheringDrop>& GetDrops() const { return Drops; }
	bool IsInitialized() const { return bResourcesInitialized; }
	bool GetSavedUnlocked() const { return bUnlocked; }
private:
	friend class FPSGatheringTest;
	FPSTreeRecord* FindTree(FIntPoint Cell);
	void AddDrop(FIntPoint Cell, EPSItemType Type, FName ItemId, int32 Quantity, float RevealDelay = 0);
	void ClearActors();
	void Persist();
	bool UsesDropTables() const { return ItemTable || TreeCutDropTable || TreeShakeDropTable; }
	UPROPERTY() TArray<FPSTreeRecord> Trees;
	UPROPERTY() TArray<FPSGatheringDrop> Drops;
	UPROPERTY(Transient) TMap<FIntPoint, TObjectPtr<APSGatheringTreeActor>> TreeActors;
	UPROPERTY(Transient) TMap<FGuid, TObjectPtr<APSWorldItemActor>> DropActors;
	bool bResourcesInitialized = false;
	bool bUnlocked = false;
	bool bCollecting = false;
};
