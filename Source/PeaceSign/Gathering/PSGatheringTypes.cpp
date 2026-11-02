#include "PSGatheringTypes.h"

FName PSGathering::AxeId(const int32 Tier)
{
	return FName(*FString::Printf(TEXT("Axe.Tier%d"), FMath::Clamp(Tier, 1, 9)));
}

FPSAxeDefinition PSGathering::GetAxe(const FName ItemId)
{
	FPSAxeDefinition Result;
	for (int32 Tier = 1; Tier <= 9; ++Tier)
		if (ItemId == AxeId(Tier)) { Result.Tier = Tier; break; }
	const int32 Damage[] = {10, 12, 15, 15, 20, 20, 30, 30, 60};
	Result.Damage = Damage[Result.Tier - 1];
	Result.StaminaCost = Result.Tier >= 6 ? 5 : 10;
	Result.ExtraLogChance = Result.Tier >= 4 ? 0.2f : 0;
	return Result;
}

FPSTreeDefinition PSGathering::GetTree(const EPSTreeSpecies Species)
{
	FPSTreeDefinition Result;
	Result.Species = Species;
	const TCHAR* Names[] = {TEXT("참나무"), TEXT("단풍나무"), TEXT("벚나무"), TEXT("야자수"), TEXT("자작나무"), TEXT("선인장")};
	const int32 Index = FMath::Clamp(static_cast<int32>(Species), 0, 5);
	Result.Name = FText::FromString(Names[Index]);
	Result.LogId = FName(*FString::Printf(TEXT("03010%d"), Index + 1));
	Result.SeedId = FName(*FString::Printf(TEXT("03020%d"), Index + 1));
	Result.CutDropID = FName(*FString::Printf(TEXT("10010%d"), Index + 1));
	Result.ShakeDropID = FName(*FString::Printf(TEXT("10020%d"), Index + 1));
	return Result;
}
