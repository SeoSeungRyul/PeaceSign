#include "PSTileSetBoard.h"
#include "PSTileStamp.h"
#include "PSFarmAutoTile.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

APSTileSetBoard::APSTileSetBoard()
{
	bIsEditorOnlyActor = true;
}
void APSTileSetBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshBoard();
}
void APSTileSetBoard::ClearLabels()
{
	for (UTextRenderComponent* Label : Labels) if (Label) Label->DestroyComponent();
	Labels.Reset();
}
void APSTileSetBoard::RefreshBoard()
{
	ClearLabels();
	if (!GetWorld()) return;
	constexpr int32 Size = 60;
	FPSChunkData Board;
	Board.Cells.SetNum(Size * Size);
	for (FPSTileCell& Cell : Board.Cells) Cell.GroundType = EPSTileType::Empty;
	for (int32 Family = 0; Family < 2; ++Family)
		for (int32 Entry = 0; Entry < 13; ++Entry)
		{
			FPSTileStamp Stamp;
			Stamp.Set = Entry < 7 ? FMath::Min(Entry + 1, 6) : Entry;
			Stamp.bVertical = Entry == 6;
			Stamp.bWater = Family == 1;
			Stamp.Origin = FIntPoint((Entry % 3) * 5, Family * 20 + (Entry / 3) * 4 + 1);
			const FIntPoint Extent = PSTileStamp::GetSize(Stamp);
			for (int32 Y = 0; Y < Extent.Y; ++Y)
				for (int32 X = 0; X < Extent.X; ++X)
				{
					const FIntPoint Cell = Stamp.Origin + FIntPoint(X, Y);
					PSTileStamp::BuildCell(Stamp, Cell, Board.Cells[Cell.Y * Size + Cell.X]);
				}
			if (!bShowLabels) continue;
			UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
			Label->SetupAttachment(GetRootComponent());
			Label->SetRelativeLocation(FVector(Stamp.Origin.X * BoardCellSize,
				(Stamp.Origin.Y - 0.4f) * BoardCellSize, 5));
			Label->SetRelativeRotation(FRotator(90, 0, 180));
			Label->SetWorldSize(BoardCellSize * 0.25f);
			Label->SetTextRenderColor(FColor::White);
			Label->SetText(FText::FromString(FString::Printf(TEXT("%s Set%d%s"),
				Family == 0 ? TEXT("Dirt") : TEXT("Water"), Stamp.Set,
				Stamp.Set == 6 ? (Stamp.bVertical ? TEXT(" V") : TEXT(" H")) : TEXT(""))));
			Label->RegisterComponent();
			Labels.Add(Label);
		}
	// Farm pieces are independent from the saved Dirt/Water stamp numbering.
	for (int32 Entry = 0; Entry < 5; ++Entry)
	{
		const FIntPoint Origin((Entry % 3) * 5, 42 + (Entry / 3) * 5);
		const uint8 First[] = {1, 10, 13, 16, 20};
		const uint8 Count[] = {9, 3, 3, 4, 4};
		for (uint8 I = 0; I < Count[Entry]; ++I)
		{
			const FIntPoint Offset = Entry == 0 ? FIntPoint(I % 3, I / 3)
				: Entry == 1 ? FIntPoint(I, 0) : Entry == 2 ? FIntPoint(0, I)
				: I == 0 ? FIntPoint(1, 0) : I == 1 ? FIntPoint(0, 1) : I == 2 ? FIntPoint(2, 1) : FIntPoint(1, 2);
			FPSTileCell& Cell = Board.Cells[(Origin.Y + Offset.Y) * Size + Origin.X + Offset.X];
			Cell.GroundType = EPSTileType::TilledSoil;
			Cell.Variant = First[Entry] + I;
		}
		if (bShowLabels)
		{
			auto* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
			Label->SetupAttachment(GetRootComponent());
			Label->SetRelativeLocation(FVector(Origin.X * BoardCellSize, (Origin.Y - 0.4f) * BoardCellSize, 5));
			Label->SetRelativeRotation(FRotator(90, 0, 180));
			Label->SetWorldSize(BoardCellSize * 0.25f);
			Label->SetText(FText::FromString(Entry == 0 ? TEXT("Farm Set1") : Entry == 1 ? TEXT("Farm Set2 H")
				: Entry == 2 ? TEXT("Farm Set2 V") : Entry == 3 ? TEXT("Farm Set3") : TEXT("Farm Set4")));
			Label->RegisterComponent();
			Labels.Add(Label);
		}
	}
	for (int32 I = 0; I < 9; ++I)
	{
		FPSTileCell& Cell = Board.Cells[(42 + I / 3) * Size + 20 + I % 3];
		Cell.GroundType = EPSTileType::TilledSoil;
		Cell.Variant = I + 1;
		Cell.bWatered = true;
	}
	Rebuild(Board, Size, FMath::Max(1.0f, BoardCellSize));
	// Board has no gameplay purpose; disable generated water collision as well.
	SetActorEnableCollision(false);
}
