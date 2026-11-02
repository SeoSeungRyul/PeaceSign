#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PSFishingTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EPSFishingNoticeType : uint8
{
	General,
	RodRequired,
	WaterRequired
};

UENUM(BlueprintType)
enum class EPSFishingState : uint8
{
	Idle,
	WaitingForBite,
	BiteWindow,
	Minigame,
	Success,
	Failure,
	CastWindup,
	CastTiming,
	Retrieving
};

UENUM(BlueprintType)
enum class EPSFishingCastGrade : uint8
{
	Green,
	Yellow,
	Red
};

/** Editable cast distance and difficulty distribution, one row per grade. */
USTRUCT(BlueprintType)
struct FPSFishingCastDefinition : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="20"))
	int32 DistanceInCells = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<float> DifficultyWeights = {55, 30, 13, 2};
	bool IsUsable() const
	{
		if (DistanceInCells < 1 || DistanceInCells > 20 || DifficultyWeights.Num() != 4) return false;
		double Total = 0;
		for (const float Weight : DifficultyWeights)
		{
			if (!FMath::IsFinite(Weight) || Weight < 0) return false;
			Total += Weight;
		}
		return Total > 0;
	}
};

UENUM(BlueprintType)
enum class EPSFishingDirection : uint8
{
	Up,
	Left,
	Down,
	Right
};

UENUM(BlueprintType)
enum class EPSFishingLocation : uint8
{
	Sea = 0,
	River = 1,
	Lake = 2
};

/** Left-to-right widths of the horizontal casting gauge. Invalid sets use the design defaults. */
USTRUCT(BlueprintType)
struct FPSFishingCastBands
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float GreenLeft = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float YellowLeft = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float Red = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float YellowRight = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float GreenRight = 15;
	FVector4f GetEdges() const
	{
		const float Widths[] = {GreenLeft, YellowLeft, Red, YellowRight, GreenRight};
		double Total = 0;
		for (float Width : Widths)
		{
			if (!FMath::IsFinite(Width) || Width <= 0) return FVector4f(.2f, .35f, .65f, .85f);
			Total += Width;
		}
		FVector4f Edges;
		double Cumulative = 0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Cumulative += Widths[Index];
			Edges[Index] = Cumulative / Total;
		}
		return Edges;
	}
};

USTRUCT(BlueprintType)
struct FPSFishJournalRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 TimesCaught = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 LargestSizeCm = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	bool bMastered = false;

	bool IsDiscovered() const { return TimesCaught > 0; }
};

USTRUCT(BlueprintType)
struct FPSFishDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name = NSLOCTEXT("Fishing", "DefaultFish", "물고기");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="4")) int32 Difficulty = 1;
	/** 0=봄, 1=여름, 2=가을, 3=겨울. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="3")) int32 Season = 0;
	/** Fishing location IDs. CSV array syntax: "(0,1)". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<int32> Location;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MinSize = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MaxSize = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DescriptionPlus;
	/** Row ID in the Icon DataTable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName IconID = NAME_None;
	/** Optional CSV column. Missing values use equal catch weight 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", DataTableImportOptional)) float Weight = 1.0f;

	bool IsUsable() const
	{
		return !Name.IsEmpty() && Difficulty >= 1 && Difficulty <= 4
			&& Season >= 0 && Season <= 3 && MinSize > 0 && MaxSize >= MinSize;
	}
};

/** Row format for the Icon CSV referenced by FPSFishDefinition::IconID. */
USTRUCT(BlueprintType)
struct FPSIconDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Image;
};

namespace PSFishing
{
	inline EPSFishingCastGrade GetCastGrade(const float Position, const FVector4f& Edges)
	{
		const float X = FMath::Clamp(FMath::IsFinite(Position) ? Position : 0.f, 0.f, 1.f);
		if (X < Edges.X || X > Edges.W) return EPSFishingCastGrade::Green;
		if (X < Edges.Y || X > Edges.Z) return EPSFishingCastGrade::Yellow;
		return EPSFishingCastGrade::Red;
	}
	inline float GetCastPosition(const float Elapsed, const float TravelSeconds)
	{
		const float Phase = FMath::Fmod(FMath::Max(0.0f, Elapsed) / FMath::Max(0.01f, TravelSeconds), 2.0f);
		return Phase <= 1.0f ? Phase : 2.0f - Phase;
	}
	/** Strength is zero at both ends and one at the centre. X/Y/Z/W are the four visible boundaries. */
	inline FVector4f GetCastBandEdges(const float GreenEnd, const float YellowEnd)
	{
		const float Green = FMath::Clamp(FMath::IsFinite(GreenEnd) ? GreenEnd : .38f, .01f, .98f);
		const float Yellow = FMath::Clamp(FMath::IsFinite(YellowEnd) ? YellowEnd : .83f, Green + .01f, .99f);
		return FVector4f(Green * .5f, Yellow * .5f, 1 - Yellow * .5f, 1 - Green * .5f);
	}
	inline EPSFishingCastGrade GetCastGrade(const float Position, const float GreenEnd, const float YellowEnd)
	{
		const FVector4f Edges = GetCastBandEdges(GreenEnd, YellowEnd);
		const float X = FMath::Clamp(FMath::IsFinite(Position) ? Position : 0.f, 0.f, 1.f);
		// Boundaries belong to the stronger band on both sides of the centre.
		if (X < Edges.X || X > Edges.W) return EPSFishingCastGrade::Green;
		if (X < Edges.Y || X > Edges.Z) return EPSFishingCastGrade::Yellow;
		return EPSFishingCastGrade::Red;
	}
	inline FName GetCastRowName(const EPSFishingCastGrade Grade)
	{
		return Grade == EPSFishingCastGrade::Red ? FName(TEXT("Red"))
			: Grade == EPSFishingCastGrade::Yellow ? FName(TEXT("Yellow")) : FName(TEXT("Green"));
	}
	inline FPSFishingCastDefinition GetDefaultCastDefinition(const EPSFishingCastGrade Grade)
	{
		FPSFishingCastDefinition Result;
		if (Grade == EPSFishingCastGrade::Yellow) { Result.DistanceInCells = 4; Result.DifficultyWeights = {50, 30, 15, 5}; }
		if (Grade == EPSFishingCastGrade::Red) { Result.DistanceInCells = 6; Result.DifficultyWeights = {45, 30, 17, 8}; }
		return Result;
	}
	inline FIntPoint GetInputCountRange(const int32 Difficulty)
	{
		switch (FMath::Clamp(Difficulty, 1, 4))
		{
		case 1: return FIntPoint(5, 10);
		case 2: return FIntPoint(11, 20);
		case 3: return FIntPoint(21, 30);
		default: return FIntPoint(31, 40);
		}
	}
}
