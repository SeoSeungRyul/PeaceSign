#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PSPlayerSkillTypes.h"
#include "PSPlayerSkillComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPSPlayerSkillLevelUp, EPSPlayerSkillField, Field, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPSPlayerSkillsChanged, EPSPlayerSkillField, Field);

UCLASS(ClassGroup=(Skills), meta=(BlueprintSpawnableComponent))
class PEACESIGN_API UPSPlayerSkillComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPSPlayerSkillComponent();
	static constexpr int32 ExperiencePerLevel = 100;
	static constexpr int32 MaximumLevel = 20;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="Skills") FPSPlayerSkillState GetSkillState(EPSPlayerSkillField Field) const;
	UFUNCTION(BlueprintPure, Category="Skills") int32 GetSkillLevel(EPSPlayerSkillField Field) const;
	UFUNCTION(BlueprintPure, Category="Skills") int32 GetSkillRank(EPSPlayerSkillField Field, FName SkillId) const;
	UFUNCTION(BlueprintCallable, Category="Skills") bool AddExperience(EPSPlayerSkillField Field, int32 Amount = 2);
	UFUNCTION(BlueprintCallable, Category="Skills") bool LearnSkill(EPSPlayerSkillField Field, FName SkillId,
		int32 MaxRank = 1, int32 PointCost = 1, FName Prerequisite = NAME_None, int32 PrerequisiteRank = 1);
	UFUNCTION(BlueprintCallable, Category="Skills") void ResetSkills();
	UFUNCTION(BlueprintCallable, Category="Skills|Save") bool SaveSkills() const;
	UFUNCTION(BlueprintCallable, Category="Skills|Save") bool LoadSkills();

	UPROPERTY(BlueprintAssignable, Category="Skills") FPSPlayerSkillLevelUp OnSkillLevelUp;
	UPROPERTY(BlueprintAssignable, Category="Skills") FPSPlayerSkillsChanged OnSkillsChanged;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skills|Save") FString SaveSlotName = TEXT("PeaceSignSkills");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skills|Save") bool bAutoSave = true;

private:
	friend class FPSPlayerSkillTest;
	void InitializeFields();
	UPROPERTY(SaveGame)
	TMap<EPSPlayerSkillField, FPSPlayerSkillState> Skills;
};
