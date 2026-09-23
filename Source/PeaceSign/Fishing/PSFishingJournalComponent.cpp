#include "PSFishingJournalComponent.h"

#include "PSFishingJournalSaveGame.h"
#include "Kismet/GameplayStatics.h"

UPSFishingJournalComponent::UPSFishingJournalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPSFishingJournalComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadJournal();
}

void UPSFishingJournalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bAutoSave) SaveJournal();
	Super::EndPlay(EndPlayReason);
}

bool UPSFishingJournalComponent::RecordCatch(const FName FishId, const int32 SizeCm, const int32 Quantity)
{
	if (FishId.IsNone() || SizeCm <= 0 || Quantity <= 0) return false;
	FPSFishJournalRecord& Record = Records.FindOrAdd(FishId);
	Record.TimesCaught += Quantity;
	Record.LargestSizeCm = FMath::Max(Record.LargestSizeCm, SizeCm);
	if (bAutoSave) SaveJournal();
	OnJournalChanged.Broadcast(FishId);
	return true;
}

FPSFishJournalRecord UPSFishingJournalComponent::GetRecord(const FName FishId) const
{
	const FPSFishJournalRecord* Record = Records.Find(FishId);
	return Record ? *Record : FPSFishJournalRecord();
}

bool UPSFishingJournalComponent::HasDiscovered(const FName FishId) const
{
	return GetRecord(FishId).IsDiscovered();
}

void UPSFishingJournalComponent::ResetJournal()
{
	Records.Reset();
	if (bAutoSave) SaveJournal();
	OnJournalChanged.Broadcast(NAME_None);
}

bool UPSFishingJournalComponent::SaveJournal() const
{
	if (SaveSlotName.IsEmpty()) return false;
	UPSFishingJournalSaveGame* Save = Cast<UPSFishingJournalSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UPSFishingJournalSaveGame::StaticClass()));
	if (!Save) return false;
	Save->Records = Records;
	return UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
}

bool UPSFishingJournalComponent::LoadJournal()
{
	if (SaveSlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0)) return false;
	const UPSFishingJournalSaveGame* Save = Cast<UPSFishingJournalSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!Save) return false;
	Records = Save->Records;
	for (auto It = Records.CreateIterator(); It; ++It)
	{
		if (It.Key().IsNone() || It.Value().TimesCaught <= 0 || It.Value().LargestSizeCm <= 0) It.RemoveCurrent();
	}
	OnJournalChanged.Broadcast(NAME_None);
	return true;
}
