#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSFishingTypes.h"
#include "PSFishingJournalComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPSFishingJournalChanged, FName, FishId);

UCLASS(ClassGroup=(Fishing), meta=(BlueprintSpawnableComponent))
class PEACESIGN_API UPSFishingJournalComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPSFishingJournalComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Fishing|Journal")
	bool RecordCatch(FName FishId, int32 SizeCm, int32 Quantity = 1);
	UFUNCTION(BlueprintPure, Category="Fishing|Journal")
	FPSFishJournalRecord GetRecord(FName FishId) const;
	UFUNCTION(BlueprintPure, Category="Fishing|Journal")
	bool HasDiscovered(FName FishId) const;
	UFUNCTION(BlueprintCallable, Category="Fishing|Journal")
	void ResetJournal();
	UFUNCTION(BlueprintCallable, Category="Fishing|Journal|Save")
	bool SaveJournal() const;
	UFUNCTION(BlueprintCallable, Category="Fishing|Journal|Save")
	bool LoadJournal();

	UPROPERTY(BlueprintAssignable, Category="Fishing|Journal")
	FPSFishingJournalChanged OnJournalChanged;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fishing|Journal|Save")
	FString SaveSlotName = TEXT("PeaceSignFishingJournal");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fishing|Journal|Save")
	bool bAutoSave = true;

private:
	UPROPERTY(SaveGame)
	TMap<FName, FPSFishJournalRecord> Records;
};
