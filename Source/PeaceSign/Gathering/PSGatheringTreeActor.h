#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PSGatheringTypes.h"
#include "PSGatheringTreeActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UPaperSprite;
class UPaperSpriteComponent;
class UTexture2D;

UCLASS()
class PEACESIGN_API APSGatheringTreeActor : public AActor
{
	GENERATED_BODY()
public:
	APSGatheringTreeActor();
	virtual void Tick(float DeltaSeconds) override;
	void Configure(const FPSTreeRecord& Tree, float CellSize, UPaperSprite* StandingSprite = nullptr, float HeightInCells = 3.2f);
	void React(bool bFalling);
	bool IsFalling() const { return bFall; }
	static constexpr float FallDuration = 0.6f;
	UFUNCTION(BlueprintCallable, Category="Gathering|Editor")
	static UPaperSprite* CreateTreeSpriteAsset(UTexture2D* Texture, const FString& PackagePath, bool bStump);
private:
	UPROPERTY() TObjectPtr<USceneComponent> Pivot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Trunk;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Crown;
	UPROPERTY() TObjectPtr<UPaperSpriteComponent> TreeSprite;
	float AnimationTime = 0;
	bool bFall = false;
};
