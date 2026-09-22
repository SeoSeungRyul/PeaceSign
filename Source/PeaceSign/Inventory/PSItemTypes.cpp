#include "PSItemTypes.h"

namespace
{
	FPSItemDefinition MakeDefinition(const TCHAR* Name, const TCHAR* Description, const int32 MaxStack,
		const TCHAR* Color, const int32 PlaceholderIcon, const int32 MaxDurability = 0,
		const float FishingTimeBonus = 0.0f, const float ExtraFishChance = 0.0f,
		const bool bInfiniteDurability = false)
	{
		FPSItemDefinition Result;
		Result.Name = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.MaxStack = MaxStack;
		Result.Color = FLinearColor(FColor::FromHex(Color));
		Result.PlaceholderIcon = PlaceholderIcon;
		Result.MaxDurability = MaxDurability;
		Result.FishingTimeBonus = FishingTimeBonus;
		Result.ExtraFishChance = ExtraFishChance;
		Result.bInfiniteDurability = bInfiniteDurability;
		return Result;
	}
}

const FPSItemDefinition& PSItems::GetDefinition(const EPSItemType ItemType, const FName ItemId)
{
	static const FPSItemDefinition Empty = MakeDefinition(TEXT("빈 슬롯"), TEXT("아이템을 이곳으로 끌어 놓으세요."), 0, TEXT("FFFFFF"), 0);
	static const FPSItemDefinition Hoe = MakeDefinition(TEXT("괭이"), TEXT("도구\n밭을 가꾸는 데 사용하는 기본 도구입니다."), 1, TEXT("BFD3C5"), 0, 100);
	static const FPSItemDefinition Seed = MakeDefinition(TEXT("씨앗"), TEXT("농사 재료\n갈아 놓은 땅에 심을 작은 씨앗입니다."), 999, TEXT("A8C66C"), 1);
	static const FPSItemDefinition Crop = MakeDefinition(TEXT("수확한 작물"), TEXT("식량\n정성껏 가꾼 밭에서 거둔 작물입니다."), 999, TEXT("ECAC62"), 2);
	static const FPSItemDefinition Wood = MakeDefinition(TEXT("목재"), TEXT("제작 재료\n건축과 제작에 사용하는 나무입니다."), 999, TEXT("C58A58"), 3);
	static const FPSItemDefinition Stone = MakeDefinition(TEXT("돌"), TEXT("제작과 건축에 사용하는 돌입니다."), 999, TEXT("B4B9BF"), 4);
	static const FPSItemDefinition Fish = MakeDefinition(TEXT("물고기"), TEXT("식량\n물에서 낚아 올린 작은 물고기입니다."), 999, TEXT("7CC5CF"), 5);
	// Durability values are tuning defaults until the item data sheet supplies final values.
	static const FPSItemDefinition WoodenRod = MakeDefinition(TEXT("나무 낚싯대"), TEXT("도구\n낚시가 가능한 기본 낚싯대입니다."), 1, TEXT("B88951"), 5, 100);
	static const FPSItemDefinition PlasticRod = MakeDefinition(TEXT("플라스틱 낚싯대"), TEXT("도구\n미니게임 제한시간이 2초 증가합니다."), 1, TEXT("E8D8B5"), 5, 100, 2.0f);
	static const FPSItemDefinition AluminumRod = MakeDefinition(TEXT("알루미늄 낚싯대"), TEXT("도구\n제한시간 +2초, 추가 물고기 확률 10%."), 1, TEXT("BEC7CF"), 5, 100, 2.0f, 0.10f);
	static const FPSItemDefinition FiberglassRod = MakeDefinition(TEXT("유리섬유 낚싯대"), TEXT("도구\n제한시간 +3초, 추가 물고기 확률 10%."), 1, TEXT("7CC5CF"), 5, 100, 3.0f, 0.10f);
	static const FPSItemDefinition CarbonRod = MakeDefinition(TEXT("카본 낚싯대"), TEXT("도구\n제한시간 +3초, 추가 물고기 확률 10%."), 1, TEXT("555D67"), 5, 100, 3.0f, 0.10f);
	static const FPSItemDefinition AsteriumRod = MakeDefinition(TEXT("아스테륨 낚싯대"), TEXT("도구\n제한시간 +4초, 추가 물고기 확률 10%, 내구도 무한."), 1, TEXT("B99BFF"), 5, 0, 4.0f, 0.10f, true);
	static const FPSItemDefinition Bait = MakeDefinition(TEXT("미끼"), TEXT("소모품\n입질 대기 시간을 2초로 줄입니다."), 999, TEXT("E6A86E"), 1);
	static const FPSItemDefinition Bobber = MakeDefinition(TEXT("찌"), TEXT("낚시 도구\n입질이 오면 미니게임을 자동으로 시작합니다."), 1, TEXT("F3D45A"), 5, 100);
	switch (ItemType)
	{
	case EPSItemType::Hoe: return Hoe;
	case EPSItemType::TestSeed: return Seed;
	case EPSItemType::TestCrop: return Crop;
	case EPSItemType::Wood: return Wood;
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
	return ItemType == EPSItemType::FishingRod ? PSItemIds::WoodenFishingRod : NAME_None;
}

bool PSItems::UsesDurability(const FPSItemStack& Stack)
{
	const FPSItemDefinition& Definition = GetDefinition(Stack);
	return Definition.MaxDurability > 0 && !Definition.bInfiniteDurability;
}

bool PSItems::IsValid(const EPSItemType ItemType)
{
	return ItemType > EPSItemType::None && ItemType <= EPSItemType::FishingBobber;
}
