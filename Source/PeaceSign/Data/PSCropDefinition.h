#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PSCropDefinition.generated.h"

USTRUCT(BlueprintType)
struct PEACESIGN_API FPSCropDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Season = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 GRtime = 1;
	// INDEX_NONE means a price has not been set.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Price = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Bonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 LV = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName IconID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SeedItemID;
};
