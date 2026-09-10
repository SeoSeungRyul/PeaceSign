#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "InputCoreTypes.h"
#include "InputMappingContext.h"

namespace
{
	bool HasMapping(const UInputMappingContext* Context, const FName ActionName, const FKey Key)
	{
		if (!Context)
		{
			return false;
		}

		return Context->GetMappings().ContainsByPredicate(
			[ActionName, Key](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action && Mapping.Action->GetFName() == ActionName && Mapping.Key == Key;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPSGameplayInputMappingTest,
	"PeaceSign.Input.GameplayMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSGameplayInputMappingTest::RunTest(const FString& Parameters)
{
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(
		nullptr,
		TEXT("/Game/Inputs/IMC_Gameplay.IMC_Gameplay"));
	TestNotNull(TEXT("IMC_Gameplay loads"), Context);
	if (!Context)
	{
		return false;
	}

	TestTrue(TEXT("Move W mapping"), HasMapping(Context, TEXT("IA_Move"), EKeys::W));
	TestTrue(TEXT("Move A mapping"), HasMapping(Context, TEXT("IA_Move"), EKeys::A));
	TestTrue(TEXT("Move S mapping"), HasMapping(Context, TEXT("IA_Move"), EKeys::S));
	TestTrue(TEXT("Move D mapping"), HasMapping(Context, TEXT("IA_Move"), EKeys::D));
	TestTrue(TEXT("Run mapping"), HasMapping(Context, TEXT("IA_Run"), EKeys::LeftShift));
	TestTrue(TEXT("Roll mapping"), HasMapping(Context, TEXT("IA_Roll"), EKeys::SpaceBar));
	TestTrue(TEXT("Interact mapping"), HasMapping(Context, TEXT("IA_Interact"), EKeys::F));
	TestTrue(TEXT("Special attack uses right mouse for hoe tilling"), HasMapping(Context, TEXT("IA_SpecialAttack"), EKeys::RightMouseButton));
	TestTrue(TEXT("Inventory mapping"), HasMapping(Context, TEXT("IA_Inventory"), EKeys::I));
	TestTrue(TEXT("Quest mapping"), HasMapping(Context, TEXT("IA_Quest"), EKeys::Q));
	TestTrue(TEXT("Ability mapping"), HasMapping(Context, TEXT("IA_Ability"), EKeys::P));
	TestTrue(TEXT("Craft mapping"), HasMapping(Context, TEXT("IA_Craft"), EKeys::B));

	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		if (!Mapping.Action || !Mapping.Key.IsValid())
		{
			AddWarning(FString::Printf(
				TEXT("IMC_Gameplay contains an empty mapping: Action=%s Key=%s"),
				*GetNameSafe(Mapping.Action),
				*Mapping.Key.ToString()));
		}
	}

	return true;
}

#endif
