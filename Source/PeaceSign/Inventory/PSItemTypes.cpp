#include "PSItemTypes.h"
#include "../Data/PSGameDataTables.h"
#include "../Gathering/PSGatheringTypes.h"

namespace
{
	FPSItemDefinition MakeDefinition(const TCHAR* Name, const TCHAR* Description, const int32 MaxStack,
		const TCHAR* Color, const int32 PlaceholderIcon, const int32 MaxDurability = 0,
		const float FishingTimeBonusRatio = 0.0f,
		const bool bInfiniteDurability = false, const int32 MiningPower = 0)
	{
		FPSItemDefinition Result;
		Result.Name = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.MaxStack = MaxStack;
		Result.Color = FLinearColor(FColor::FromHex(Color));
		Result.PlaceholderIcon = PlaceholderIcon;
		Result.MaxDurability = MaxDurability;
		Result.FishingTimeBonusRatio = FishingTimeBonusRatio;
		Result.bInfiniteDurability = bInfiniteDurability;
		Result.MiningPower = MiningPower;
		return Result;
	}

	FPSItemDefinition MakePickaxeDefinition(const TCHAR* Name, const TCHAR* Color, const int32 MiningPower)
	{
		return MakeDefinition(Name, TEXT("도구\n돌 블록과 광물을 채굴하는 곡괭이입니다."),
			1, Color, 6, 100, 0.0f, false, MiningPower);
	}

	FPSItemDefinition MakeOreDefinition(const TCHAR* Name, const TCHAR* Color)
	{
		return MakeDefinition(Name, TEXT("제작 재료\n제련하여 주괴로 만드는 광석입니다."), 999, Color, 4);
	}

	FPSItemDefinition MakeUpgradedHoeDefinition(const TCHAR* Name, const TCHAR* Description, const TCHAR* Color, const int32 Depth)
	{
		FPSItemDefinition Result = MakeDefinition(Name, Description, 1, Color, 0, 100);
		Result.ChargedHoeDepth = Depth;
		return Result;
	}
}

const FPSItemDefinition& PSItems::GetDefinition(const EPSItemType ItemType, const FName ItemId)
{
	static const FPSItemDefinition Empty = MakeDefinition(TEXT("빈 슬롯"), TEXT("아이템을 이곳으로 끌어 놓으세요."), 0, TEXT("FFFFFF"), 0);
	static const FPSItemDefinition Hoe = MakeDefinition(TEXT("괭이"), TEXT("도구\n밭을 가꾸는 데 사용하는 기본 도구입니다."), 1, TEXT("BFD3C5"), 0, 100);
	static const FPSItemDefinition UpgradedHoe = MakeUpgradedHoeDefinition(TEXT("1차 강화 괭이"), TEXT("임시 도구\n짧게 우클릭하면 앞 1칸, 0.5초 이상 누르고 떼면 전방 가로 3칸을 갑니다."), TEXT("E5AF68"), 1);
	static const FPSItemDefinition SecondUpgradedHoe = MakeUpgradedHoeDefinition(TEXT("2차 강화 괭이"), TEXT("임시 도구\n짧게 우클릭하면 앞 1칸, 0.5초 이상 누르고 떼면 전방 3×3칸을 갑니다."), TEXT("B99BFF"), 3);
	static const FPSItemDefinition Shovel = MakeDefinition(TEXT("삽"), TEXT("도구\n우클릭으로 풀과 흙을 서로 바꿉니다."), 1, TEXT("B9C4CE"), 7, 100);
	static const FPSItemDefinition Seed = MakeDefinition(TEXT("씨앗"), TEXT("농사 재료\n갈아 놓은 땅에 심을 작은 씨앗입니다."), 999, TEXT("A8C66C"), 1);
	static const FPSItemDefinition Crop = MakeDefinition(TEXT("수확한 작물"), TEXT("식량\n정성껏 가꾼 밭에서 거둔 작물입니다."), 999, TEXT("ECAC62"), 2);
	static const FPSItemDefinition Wood = MakeDefinition(TEXT("목재"), TEXT("제작 재료\n건축과 제작에 사용하는 나무입니다."), 999, TEXT("C58A58"), 3);
	static const FPSItemDefinition Stone = MakeDefinition(TEXT("돌"), TEXT("제작과 건축에 사용하는 돌입니다."), 999, TEXT("B4B9BF"), 4);
	static const FPSItemDefinition Fish = MakeDefinition(TEXT("물고기"), TEXT("식량\n물에서 낚아 올린 작은 물고기입니다."), 999, TEXT("7CC5CF"), 5);
	static const FPSItemDefinition StonePickaxe = MakePickaxeDefinition(TEXT("돌 곡괭이"), TEXT("AAB0B7"), 10);
	static const FPSItemDefinition CopperPickaxe = MakePickaxeDefinition(TEXT("구리 곡괭이"), TEXT("B87333"), 15);
	static const FPSItemDefinition IronPickaxe = MakePickaxeDefinition(TEXT("철 곡괭이"), TEXT("666C73"), 25);
	static const FPSItemDefinition SilverPickaxe = MakePickaxeDefinition(TEXT("은 곡괭이"), TEXT("D9DEE5"), 40);
	static const FPSItemDefinition GoldPickaxe = MakePickaxeDefinition(TEXT("금 곡괭이"), TEXT("E3B341"), 65);
	static const FPSItemDefinition TitaniumPickaxe = MakePickaxeDefinition(TEXT("티타늄 곡괭이"), TEXT("9EB5C5"), 105);
	static const FPSItemDefinition LumistonePickaxe = MakePickaxeDefinition(TEXT("루미석 곡괭이"), TEXT("73E4CC"), 170);
	static const FPSItemDefinition AsteriumPickaxe = MakePickaxeDefinition(TEXT("아스테륨 곡괭이"), TEXT("C6A7FF"), 275);
	static const FPSItemDefinition CopperOre = MakeOreDefinition(TEXT("구리 광석"), TEXT("B87333"));
	static const FPSItemDefinition IronOre = MakeOreDefinition(TEXT("철 광석"), TEXT("666C73"));
	static const FPSItemDefinition SilverOre = MakeOreDefinition(TEXT("은 광석"), TEXT("D9DEE5"));
	static const FPSItemDefinition GoldOre = MakeOreDefinition(TEXT("금 광석"), TEXT("E3B341"));
	static const FPSItemDefinition TitaniumOre = MakeOreDefinition(TEXT("티타늄 광석"), TEXT("9EB5C5"));
	static const FPSItemDefinition LumistoneOre = MakeOreDefinition(TEXT("루미석 광석"), TEXT("73E4CC"));
	static const FPSItemDefinition AsteriumOre = MakeOreDefinition(TEXT("아스테륨 광석"), TEXT("C6A7FF"));
	// Durability values are tuning defaults until the item data sheet supplies final values.
	static const FPSItemDefinition WoodenRod = MakeDefinition(TEXT("나무 낚싯대"), TEXT("도구\n낚시가 가능한 기본 낚싯대입니다."), 1, TEXT("B88951"), 5, 100);
	static const FPSItemDefinition PlasticRod = MakeDefinition(TEXT("플라스틱 낚싯대"), TEXT("도구\n미니게임 제한시간 +20%."), 1, TEXT("E8D8B5"), 5, 100, 0.2f);
	static const FPSItemDefinition AluminumRod = MakeDefinition(TEXT("알루미늄 낚싯대"), TEXT("도구\n미니게임 제한시간 +30%."), 1, TEXT("BEC7CF"), 5, 100, 0.3f);
	static const FPSItemDefinition FiberglassRod = MakeDefinition(TEXT("유리섬유 낚싯대"), TEXT("도구\n미니게임 제한시간 +40%."), 1, TEXT("7CC5CF"), 5, 100, 0.4f);
	static const FPSItemDefinition CarbonRod = MakeDefinition(TEXT("카본 낚싯대"), TEXT("도구\n미니게임 제한시간 +50%."), 1, TEXT("555D67"), 5, 100, 0.5f);
	static const FPSItemDefinition AsteriumRod = MakeDefinition(TEXT("아스테륨 낚싯대"), TEXT("도구\n미니게임 제한시간 +60%, 내구도 무한."), 1, TEXT("B99BFF"), 5, 0, 0.6f, true);
	static const FPSItemDefinition Bait = MakeDefinition(TEXT("미끼"), TEXT("소모품\n입질까지 최대 대기시간을 5% 줄입니다."), 999, TEXT("E6A86E"), 1);
	static const FPSItemDefinition Bobber = MakeDefinition(TEXT("찌"), TEXT("낚시 도구\n입질이 오면 미니게임을 자동으로 시작합니다."), 1, TEXT("F3D45A"), 5, 100);
	switch (ItemType)
	{
	case EPSItemType::Axe:
	{
		static const TArray<FPSItemDefinition> Axes = []
		{
			TArray<FPSItemDefinition> Result;
			for (int32 Tier = 1; Tier <= 9; ++Tier)
				Result.Add(MakeDefinition(*FString::Printf(TEXT("%d단계 도끼"), Tier),
					TEXT("도구\n나무를 우클릭하여 벌목합니다."), 1, TEXT("AAB9CE"), 9, 100));
			return Result;
		}();
		return Axes[PSGathering::GetAxe(ItemId).Tier - 1];
	}
	case EPSItemType::TreeSeed:
	{
		static const TArray<FPSItemDefinition> Seeds = []
		{
			TArray<FPSItemDefinition> Result;
			for (const TCHAR* TreeName : {TEXT("참나무"), TEXT("단풍나무"), TEXT("벚나무"), TEXT("야자수"), TEXT("자작나무"), TEXT("선인장")})
				Result.Add(MakeDefinition(*FString::Printf(TEXT("%s 씨앗"), TreeName), TEXT("채집 재료\n나무에서 얻은 씨앗입니다."), 999, TEXT("A5B862"), 1));
			return Result;
		}();
		const FName Canonical = UPSGameDataLibrary::CanonicalItemID(ItemId);
		for (int32 Index = 0; Index < 6; ++Index)
			if (Canonical == FName(*FString::Printf(TEXT("03020%d"), Index + 1))) return Seeds[Index];
		return Seeds[0];
	}
	case EPSItemType::GatheredFruit:
	{
		static const FPSItemDefinition Apple = MakeDefinition(TEXT("사과"), TEXT("채집물\n나무를 흔들어 얻은 사과입니다."), 999, TEXT("DF7050"), 2);
		static const FPSItemDefinition Chestnut = MakeDefinition(TEXT("밤"), TEXT("채집물\n나무를 흔들어 얻은 밤입니다."), 999, TEXT("A7784A"), 1);
		return ItemId == TEXT("030208") ? Chestnut : Apple;
	}
	case EPSItemType::WateringCan:
	{
		static const FPSItemDefinition Can = MakeDefinition(TEXT("물뿌리개"), TEXT("도구\n우클릭으로 밭에 물을 줍니다. 다음 날 오전 6시에 마릅니다."), 1, TEXT("65BCE8"), 8);
		return Can;
	}
	case EPSItemType::Hoe:
		if (ItemId == PSItemIds::SecondUpgradedHoe) return SecondUpgradedHoe;
		return ItemId == PSItemIds::UpgradedHoe ? UpgradedHoe : Hoe;
	case EPSItemType::Shovel: return Shovel;
	case EPSItemType::TestSeed: return Seed;
	case EPSItemType::TestCrop: return Crop;
	case EPSItemType::Wood:
	{
		static const TArray<FPSItemDefinition> Logs = []
		{
			TArray<FPSItemDefinition> Result;
			for (const TCHAR* LogName : {TEXT("참나무 원목"), TEXT("단풍나무 원목"), TEXT("벚나무 원목"), TEXT("야자수 원목"), TEXT("자작나무 원목"), TEXT("선인장")})
				Result.Add(MakeDefinition(LogName, TEXT("제작 재료\n벌목으로 얻은 재료입니다."), 999, TEXT("C58A58"), 3));
			return Result;
		}();
		const FName Canonical = UPSGameDataLibrary::CanonicalItemID(ItemId);
		for (int32 Index = 0; Index < 6; ++Index)
			if (Canonical == FName(*FString::Printf(TEXT("03010%d"), Index + 1))) return Logs[Index];
		return Wood;
	}
	case EPSItemType::Stone: return Stone;
	case EPSItemType::Fish: return Fish;
	case EPSItemType::FishingRod:
		if (ItemId == PSItemIds::PlasticFishingRod) return PlasticRod;
		if (ItemId == PSItemIds::AluminumFishingRod) return AluminumRod;
		if (ItemId == PSItemIds::FiberglassFishingRod) return FiberglassRod;
		if (ItemId == PSItemIds::CarbonFishingRod) return CarbonRod;
		if (ItemId == PSItemIds::AsteriumFishingRod) return AsteriumRod;
		return WoodenRod;
	case EPSItemType::FishingBait: return Bait;
	case EPSItemType::FishingBobber: return Bobber;
	case EPSItemType::Pickaxe:
		if (ItemId == PSItemIds::CopperPickaxe) return CopperPickaxe;
		if (ItemId == PSItemIds::IronPickaxe) return IronPickaxe;
		if (ItemId == PSItemIds::SilverPickaxe) return SilverPickaxe;
		if (ItemId == PSItemIds::GoldPickaxe) return GoldPickaxe;
		if (ItemId == PSItemIds::TitaniumPickaxe) return TitaniumPickaxe;
		if (ItemId == PSItemIds::LumistonePickaxe) return LumistonePickaxe;
		if (ItemId == PSItemIds::AsteriumPickaxe) return AsteriumPickaxe;
		return StonePickaxe;
	case EPSItemType::Ore:
		if (ItemId == PSItemIds::IronOre) return IronOre;
		if (ItemId == PSItemIds::SilverOre) return SilverOre;
		if (ItemId == PSItemIds::GoldOre) return GoldOre;
		if (ItemId == PSItemIds::TitaniumOre) return TitaniumOre;
		if (ItemId == PSItemIds::LumistoneOre) return LumistoneOre;
		if (ItemId == PSItemIds::AsteriumOre) return AsteriumOre;
		return CopperOre;
	case EPSItemType::None:
	default: return Empty;
	}
}

const FPSItemDefinition& PSItems::GetDefinition(const EPSItemType ItemType)
{
	return GetDefinition(ItemType, GetDefaultItemId(ItemType));
}

const FPSItemDefinition& PSItems::GetDefinition(const FPSItemStack& Stack)
{
	return GetDefinition(Stack.ItemType, Stack.ItemId);
}

FName PSItems::GetDefaultItemId(const EPSItemType ItemType)
{
	if (ItemType == EPSItemType::Axe) return PSGathering::AxeId(1);
	if (ItemType == EPSItemType::TreeSeed) return TEXT("TreeSeed.Oak");
	if (ItemType == EPSItemType::FishingRod) return PSItemIds::WoodenFishingRod;
	if (ItemType == EPSItemType::Pickaxe) return PSItemIds::StonePickaxe;
	if (ItemType == EPSItemType::Ore) return PSItemIds::CopperOre;
	return NAME_None;
}

int32 PSItems::GetMiningPower(const FPSItemStack* EquippedStack)
{
	if (!EquippedStack || EquippedStack->IsEmpty() || EquippedStack->ItemType != EPSItemType::Pickaxe)
		return DefaultMiningPower;
	return FMath::Max(0, GetDefinition(*EquippedStack).MiningPower);
}

bool PSItems::UsesDurability(const FPSItemStack& Stack)
{
	const FPSItemDefinition& Definition = GetDefinition(Stack);
	return Definition.MaxDurability > 0 && !Definition.bInfiniteDurability;
}

bool PSItems::IsValid(const EPSItemType ItemType)
{
	return ItemType > EPSItemType::None && ItemType <= EPSItemType::GatheredFruit;
}

bool PSItems::CanChargeHoe(const FPSItemStack& Stack)
{
	return !Stack.IsEmpty() && Stack.ItemType == EPSItemType::Hoe && GetDefinition(Stack).ChargedHoeDepth > 0
		&& Stack.CurrentDurability > 0;
}
