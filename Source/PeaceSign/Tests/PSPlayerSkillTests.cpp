#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Skills/PSPlayerSkillComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSPlayerSkillTest, "PeaceSign.Skills.ProgressionAndSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSPlayerSkillTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UPSPlayerSkillComponent* Skills = NewObject<UPSPlayerSkillComponent>(World);
	Skills->bAutoSave = false;
	Skills->SaveSlotName = TEXT("SkillTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Skills->ResetSkills();
	TestTrue(TEXT("Skill XP can be granted"), Skills->AddExperience(EPSPlayerSkillField::Fishing, 198));
	FPSPlayerSkillState State = Skills->GetSkillState(EPSPlayerSkillField::Fishing);
	TestEqual(TEXT("XP levels once per hundred"), State.Level, 1);
	TestEqual(TEXT("Remaining XP carries to the next level"), State.Experience, 98);
	TestEqual(TEXT("Level up grants a field point"), State.UnspentPoints, 1);
	TestFalse(TEXT("Prerequisite blocks a dependent skill"), Skills->LearnSkill(
		EPSPlayerSkillField::Fishing, TEXT("Fishing.FastHook"), 1, 1, TEXT("Fishing.Novice"), 1));
	TestTrue(TEXT("Available point learns a skill"), Skills->LearnSkill(
		EPSPlayerSkillField::Fishing, TEXT("Fishing.Novice")));
	TestEqual(TEXT("Learned rank is stored"), Skills->GetSkillRank(EPSPlayerSkillField::Fishing, TEXT("Fishing.Novice")), 1);
	TestTrue(TEXT("Skill save succeeds"), Skills->SaveSkills());
	UPSPlayerSkillComponent* Loaded = NewObject<UPSPlayerSkillComponent>(World);
	Loaded->bAutoSave = false;
	Loaded->SaveSlotName = Skills->SaveSlotName;
	TestTrue(TEXT("Skill save reloads"), Loaded->LoadSkills());
	TestEqual(TEXT("Reload retains level"), Loaded->GetSkillLevel(EPSPlayerSkillField::Fishing), 1);
	TestEqual(TEXT("Reload retains learned rank"), Loaded->GetSkillRank(
		EPSPlayerSkillField::Fishing, TEXT("Fishing.Novice")), 1);
	UGameplayStatics::DeleteGameInSlot(Skills->SaveSlotName, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
