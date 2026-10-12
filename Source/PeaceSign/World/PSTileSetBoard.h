#pragma once
#include "PSTileChunkActor.h"
#include "PSTileSetBoard.generated.h"

/** Editor-only, non-destructive board showing every supplied piece without auto-tiling. */
UCLASS()
class PEACESIGN_API APSTileSetBoard : public APSTileChunkActor
{
	GENERATED_BODY()
public:
	APSTileSetBoard();
	virtual void OnConstruction(const FTransform& Transform) override;
	UFUNCTION(CallInEditor, Category = "Tile Set Board", meta = (DisplayName = "Refresh Tile Set Board"))
	void RefreshBoard();
	UPROPERTY(EditAnywhere, Category = "Tile Set Board", meta = (ClampMin = "1"))
	float BoardCellSize = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Tile Set Board")
	bool bShowLabels = true;
private:
	void ClearLabels();
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextRenderComponent>> Labels;
};
