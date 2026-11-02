#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "../Inventory/PSItemTypes.h"
#include "PSGameDataTables.generated.h"

/** Item CSV: the first column is the item ID / DataTable row name. */
USTRUCT(BlueprintType)
struct FPSItemTableRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName IconID;
	/** -1 means that a price has not been authored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Sellp = -1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BuyP = -1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Store;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Isdiscard = true;
};

/** Row names must be unique; ID is the repeatable drop-table group ID. */
USTRUCT(BlueprintType)
struct FPSTreeCutDropRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) double DropRate = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 0;
};

USTRUCT(BlueprintType)
struct FPSTreeShakeDropRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) double DropRate = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 0;
};

UCLASS()
class PEACESIGN_API UPSGameDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="PeaceSign|Data")
	static bool FindItem(const UDataTable* ItemTable, FName ItemID, FPSItemTableRow& Item);
	/** Validates all configured groups, item references, counts and probability sums. */
	UFUNCTION(BlueprintCallable, Category="PeaceSign|Data")
	static bool ValidateTreeTables(const UDataTable* ItemTable, const UDataTable* TreeCutDropTable,
		const UDataTable* TreeShakeDropTable, FString& Error);
	/** One weighted selection per ItemID within ID (e.g. logs AND seeds). */
	UFUNCTION(BlueprintCallable, Category="PeaceSign|Data")
	static bool RollTreeCutDrop(const UDataTable* ItemTable, const UDataTable* DropTable,
		FName DropID, int32 RandomSeed, TArray<FPSItemStack>& Rewards, FString& Error);
	/** One selection across all rows within ID. True + empty rewards is a valid blank roll. */
	UFUNCTION(BlueprintCallable, Category="PeaceSign|Data")
	static bool RollTreeShakeDrop(const UDataTable* ItemTable, const UDataTable* DropTable,
		FName DropID, int32 RandomSeed, TArray<FPSItemStack>& Rewards, FString& Error);
	UFUNCTION(BlueprintPure, Category="PeaceSign|Data")
	static FName CanonicalItemID(FName ItemID);
	static EPSItemType GatheringItemType(FName ItemID);
};
