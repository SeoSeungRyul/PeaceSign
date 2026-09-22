#include "PSPlayerSkillComponent.h"
#include "PSPlayerSkillSaveGame.h"
#include "Kismet/GameplayStatics.h"

UPSPlayerSkillComponent::UPSPlayerSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPSPlayerSkillComponent::InitializeFields()
{
	for (const EPSPlayerSkillField Field : {EPSPlayerSkillField::Fishing, EPSPlayerSkillField::Farming,
		EPSPlayerSkillField::Mining, EPSPlayerSkillField::Combat}) Skills.FindOrAdd(Field);
}

void UPSPlayerSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!LoadSkills())
	{
		ResetSkills();
		if (bAutoSave) SaveSkills();
	}
}

void UPSPlayerSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bAutoSave) SaveSkills();
	Super::EndPlay(EndPlayReason);
}

FPSPlayerSkillState UPSPlayerSkillComponent::GetSkillState(const EPSPlayerSkillField Field) const
{
	if (const FPSPlayerSkillState* State = Skills.Find(Field)) return *State;
	return FPSPlayerSkillState();
}

int32 UPSPlayerSkillComponent::GetSkillLevel(const EPSPlayerSkillField Field) const
{
	return GetSkillState(Field).Level;
}

int32 UPSPlayerSkillComponent::GetSkillRank(const EPSPlayerSkillField Field, const FName SkillId) const
{
	if (const FPSPlayerSkillState* State = Skills.Find(Field))
		if (const int32* Rank = State->LearnedSkills.Find(SkillId)) return *Rank;
	return 0;
}

bool UPSPlayerSkillComponent::AddExperience(const EPSPlayerSkillField Field, const int32 Amount)
{
	if (Amount <= 0) return false;
	InitializeFields();
	FPSPlayerSkillState& State = Skills.FindChecked(Field);
	if (State.Level >= MaximumLevel) return false;
	State.Experience += Amount;
	while (State.Level < MaximumLevel && State.Experience >= ExperiencePerLevel)
	{
		State.Experience -= ExperiencePerLevel;
		++State.Level;
		++State.UnspentPoints;
		OnSkillLevelUp.Broadcast(Field, State.Level);
	}
	if (State.Level >= MaximumLevel) State.Experience = 0;
	if (bAutoSave) SaveSkills();
	OnSkillsChanged.Broadcast(Field);
	return true;
}

bool UPSPlayerSkillComponent::LearnSkill(const EPSPlayerSkillField Field, const FName SkillId,
	const int32 MaxRank, const int32 PointCost, const FName Prerequisite, const int32 PrerequisiteRank)
{
	if (SkillId.IsNone() || MaxRank <= 0 || PointCost <= 0) return false;
	InitializeFields();
	FPSPlayerSkillState& State = Skills.FindChecked(Field);
	const int32 CurrentRank = State.LearnedSkills.FindRef(SkillId);
	if (CurrentRank >= MaxRank || State.UnspentPoints < PointCost) return false;
	if (!Prerequisite.IsNone() && State.LearnedSkills.FindRef(Prerequisite) < PrerequisiteRank) return false;
	State.UnspentPoints -= PointCost;
	State.LearnedSkills.Add(SkillId, CurrentRank + 1);
	if (bAutoSave) SaveSkills();
	OnSkillsChanged.Broadcast(Field);
	return true;
}

void UPSPlayerSkillComponent::ResetSkills()
{
	Skills.Reset();
	InitializeFields();
}

bool UPSPlayerSkillComponent::SaveSkills() const
{
	if (SaveSlotName.IsEmpty()) return false;
	UPSPlayerSkillSaveGame* Save = Cast<UPSPlayerSkillSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UPSPlayerSkillSaveGame::StaticClass()));
	if (!Save) return false;
	Save->Skills = Skills;
	return UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
}

bool UPSPlayerSkillComponent::LoadSkills()
{
	if (SaveSlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0)) return false;
	const UPSPlayerSkillSaveGame* Save = Cast<UPSPlayerSkillSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!Save) return false;
	Skills = Save->Skills;
	InitializeFields();
	for (TPair<EPSPlayerSkillField, FPSPlayerSkillState>& Pair : Skills)
	{
		Pair.Value.Level = FMath::Clamp(Pair.Value.Level, 0, MaximumLevel);
		Pair.Value.Experience = Pair.Value.Level == MaximumLevel ? 0 : FMath::Clamp(Pair.Value.Experience, 0, ExperiencePerLevel - 1);
		Pair.Value.UnspentPoints = FMath::Max(0, Pair.Value.UnspentPoints);
	}
	return true;
}
