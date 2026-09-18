#include "PSItemTypes.h"

namespace
{
	FPSItemDefinition MakeDefinition(const TCHAR* Name, const TCHAR* Description, const int32 MaxStack,
		const TCHAR* Color, const int32 PlaceholderIcon)
	{
		FPSItemDefinition Result;
		Result.Name = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.MaxStack = MaxStack;
		Result.Color = FLinearColor(FColor::FromHex(Color));
		Result.PlaceholderIcon = PlaceholderIcon;
		return Result;
	}
}

const FPSItemDefinition& PSItems::GetDefinition(const EPSItemType ItemType)
{
	static const FPSItemDefinition Empty = MakeDefinition(TEXT("빈 슬롯"), TEXT("아이템을 이곳으로 끌어 놓으세요."), 0, TEXT("FFFFFF"), 0);
	static const FPSItemDefinition Hoe = MakeDefinition(TEXT("괭이"), TEXT("도구\n밭을 가꾸는 데 사용하는 기본 도구입니다."), 1, TEXT("BFD3C5"), 0);
	static const FPSItemDefinition Seed = MakeDefinition(TEXT("씨앗"), TEXT("농사 재료\n갈아 놓은 땅에 심을 작은 씨앗입니다."), 999, TEXT("A8C66C"), 1);
	static const FPSItemDefinition Crop = MakeDefinition(TEXT("수확한 작물"), TEXT("식량\n정성껏 가꾼 밭에서 거둔 작물입니다."), 999, TEXT("ECAC62"), 2);
	static const FPSItemDefinition Wood = MakeDefinition(TEXT("목재"), TEXT("제작 재료\n건축과 제작에 사용하는 나무입니다."), 999, TEXT("C58A58"), 3);
	static const FPSItemDefinition Stone = MakeDefinition(TEXT("돌"), TEXT("제작과 건축에 사용하는 돌입니다."), 999, TEXT("B4B9BF"), 4);
	static const FPSItemDefinition Fish = MakeDefinition(TEXT("물고기"), TEXT("식량\n물에서 낚아 올린 작은 물고기입니다."), 999, TEXT("7CC5CF"), 5);
	static const FPSItemDefinition FishingRod = MakeDefinition(TEXT("낚싯대"), TEXT("도구\n가까운 물에서 낚시할 때 사용합니다."), 1, TEXT("7CC5CF"), 5);
	switch (ItemType)
	{
	case EPSItemType::Hoe: return Hoe;
	case EPSItemType::TestSeed: return Seed;
	case EPSItemType::TestCrop: return Crop;
	case EPSItemType::Wood: return Wood;
	case EPSItemType::Stone: return Stone;
	case EPSItemType::Fish: return Fish;
	case EPSItemType::FishingRod: return FishingRod;
	case EPSItemType::None:
	default: return Empty;
	}
}

bool PSItems::IsValid(const EPSItemType ItemType)
{
	return ItemType > EPSItemType::None && ItemType <= EPSItemType::FishingRod;
}
