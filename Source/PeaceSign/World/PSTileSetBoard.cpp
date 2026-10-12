#include "PSTileSetBoard.h"
#include "PSTileStamp.h"
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
	constexpr int32 Size = 32;
	FPSChunkData Board;
	Board.Cells.SetNum(Size * Size);
	for (FPSTileCell& Cell : Board.Cells) Cell.GroundType = EPSTileType::Empty;
	for (int32 Family = 0; Family < 2; ++Family)
		for (int32 Entry = 0; Entry < 12; ++Entry)
		{
			FPSTileStamp Stamp;
			Stamp.Set = Entry < 7 ? FMath::Min(Entry + 1, 6) : Entry;
			Stamp.bVertical = Entry == 6;
			Stamp.bWater = Family == 1;
			Stamp.Origin = FIntPoint((Entry % 3) * 5, Family * 16 + (Entry / 3) * 4 + 1);
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
	Rebuild(Board, Size, FMath::Max(1.0f, BoardCellSize));
	// Board has no gameplay purpose; disable generated water collision as well.
	SetActorEnableCollision(false);
}
