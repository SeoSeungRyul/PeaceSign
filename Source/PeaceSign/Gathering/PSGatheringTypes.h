#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "../Inventory/PSItemTypes.h"
#include "PSGatheringTypes.generated.h"

UENUM(BlueprintType)
enum class EPSTreeSpecies : uint8 { Oak, Maple, Cherry, Palm, Birch, Cactus };

/** Runtime species defaults and drop-group mapping; not an importable DataTable row. */
USTRUCT(BlueprintType)
struct FPSTreeDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EPSTreeSpecies Species = EPSTreeSpecies::Oak;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxHealth = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LogId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SeedId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LogQuantity = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SeedQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CutDropID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ShakeDropID;
};

USTRUCT(BlueprintType)
struct FPSAxeDefinition : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Tier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Damage = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StaminaCost = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ExtraLogChance = 0;
	// Rare byproduct is intentionally disabled until its item and probability are authored.
};

USTRUCT(BlueprintType)
struct FPSTreeRecord
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame) FIntPoint Cell = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame) EPSTreeSpecies Species = EPSTreeSpecies::Oak;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame) int32 Health = 60;
	UPROPERTY(BlueprintReadOnly, SaveGame) int64 NextShakeHalfHour = -1;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 RewardSequence = 0;
	// Health 0 is a persistent stump. Regrowth is the next implementation stage.
};

USTRUCT()
struct FPSGatheringDrop
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FGuid Id;
	UPROPERTY(SaveGame) FIntPoint Cell = FIntPoint::ZeroValue;
	UPROPERTY(SaveGame) FPSItemStack Item;
	// Presentation delay only. Reloaded terminal tree state reveals its pending rewards immediately.
	UPROPERTY(Transient) double RevealAtWorldSeconds = 0;
};

namespace PSGathering
{
	inline constexpr int64 ShakeCooldown = 7 * 48;
	PEACESIGN_API FName AxeId(int32 Tier);
	PEACESIGN_API FPSAxeDefinition GetAxe(FName ItemId);
	PEACESIGN_API FPSTreeDefinition GetTree(EPSTreeSpecies Species);
}
