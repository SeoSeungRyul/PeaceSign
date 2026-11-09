#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSBushWorldComponent.generated.h"

class APSGridWorld;
class APSPlayerCharacter;
class UPSInventoryComponent;
class UTexture2D;

USTRUCT()
struct FPSBushRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FIntPoint Cell = FIntPoint::ZeroValue;
	UPROPERTY(SaveGame) FName ItemId;
	UPROPERTY(SaveGame) int32 RemainingHarvests = 3;
	UPROPERTY(SaveGame) int32 HarvestSequence = 0;
	UPROPERTY(SaveGame) int64 ReadyAt = 0;
};

/** The grass-only prototype patch uses the same game clock and world save as trees. */
UCLASS(ClassGroup=(Gathering))
class PEACESIGN_API UPSBushWorldComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPSBushWorldComponent();
	virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UPROPERTY(EditAnywhere, Category="Bushes") bool bEnabled = false;
	UPROPERTY(EditAnywhere, Category="Bushes") FIntPoint PatchOrigin = FIntPoint(-10, -10);
	UPROPERTY(EditAnywhere, Category="Bushes") TObjectPtr<UTexture2D> Texture;
	void Initialize();
	void Restore(const TArray<FPSBushRecord>& Records, bool bWasInitialized);
	void Reset();
	bool HasBush(FIntPoint Cell) const;
	bool CanReach(const APSPlayerCharacter* Player, FIntPoint Cell) const;
	TOptional<FIntPoint> FindNearest(const APSPlayerCharacter* Player) const;
	const FPSBushRecord* Find(FIntPoint Cell) const;
	void SetHarvestShake(FIntPoint Cell, float Angle);
	bool Harvest(APSPlayerCharacter* Player, FIntPoint Cell, UPSInventoryComponent* Inventory);
	const TArray<FPSBushRecord>& GetRecords() const { return Bushes; }
	bool IsInitialized() const { return bInitialized; }
private:
	bool Place(FPSBushRecord& Bush, FRandomStream& Random);
	void RefreshActors();
	void ClearActors();
	UPROPERTY() TArray<FPSBushRecord> Bushes;
	UPROPERTY(Transient) TMap<FIntPoint, TObjectPtr<AActor>> Actors;
	bool bInitialized = false;
	bool bHarvesting = false;
	int64 NextSpawnCheck = -1;
};
