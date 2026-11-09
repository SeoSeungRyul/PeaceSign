#include "PSFishingBobber.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

APSFishingBobber::APSFishingBobber()
{
	FloatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Float"));
	SetRootComponent(FloatMesh);
	LineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FishingLine"));
	LineMesh->SetupAttachment(FloatMesh);
	CatchImage = CreateDefaultSubobject<UBillboardComponent>(TEXT("CatchImage"));
	CatchImage->SetupAttachment(FloatMesh);
	CatchImage->SetHiddenInGame(true);
	CatchImage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	FloatMesh->SetStaticMesh(Sphere.Object);
	LineMesh->SetStaticMesh(Cylinder.Object);
	FloatMesh->SetWorldScale3D(FVector(.12f, .12f, .24f));
	for (UStaticMeshComponent* Mesh : {FloatMesh.Get(), LineMesh.Get()})
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
	}
}

void APSFishingBobber::ShowCatch(UTexture2D* Texture)
{
	FloatMesh->SetVisibility(false);
	LineMesh->SetVisibility(false);
	CatchImage->SetHiddenInGame(true);
#if WITH_EDITOR
	// A first load can report the 32x32 compilation placeholder. Its size must
	// never determine the scale of the full-resolution sprite that arrives later.
	if (Texture && Texture->IsCompiling()) FTextureCompilingManager::Get().FinishCompilation({Texture});
#endif
	const int32 Width = Texture ? Texture->GetSizeX() : 0;
	const int32 Height = Texture ? Texture->GetSizeY() : 0;
	if (Width <= 0 || Height <= 0)
	{
		CatchImage->SetSprite(nullptr);
		return;
	}
	const int32 LongestSide = FMath::Max(Width, Height);
	const float DisplaySize = FMath::IsFinite(CatchImageWorldSize) ? FMath::Max(1.f, CatchImageWorldSize) : 64.f;
	CatchImage->SetWorldScale3D(FVector(DisplaySize / LongestSide));
	// SetSprite rebuilds the scene proxy, which snapshots its scale. Configure
	// the transform first and make the component visible only after that rebuild.
	CatchImage->SetSprite(Texture);
	CatchImage->SetHiddenInGame(false);
}

void APSFishingBobber::UpdateVisual(const FVector& RodTip, const FVector& Landing, const bool bBiting,
	const float WaterElapsed, const float RetrieveAlpha, const float FlightAlpha, const FVector& ArcUp)
{
	const float T = FMath::Clamp(FlightAlpha, 0.f, 1.f);
	const FVector Up = ArcUp.GetSafeNormal();
	// In the top-down orthographic camera, world Z is invisible. Bend towards
	// screen-up instead, and keep both ends faster even when casting screen-up/down.
	const float Height = FMath::Max(FMath::Max(0.f, CastArcHeight), .6f * FMath::Abs(FVector::DotProduct(Landing - RodTip, Up)));
	FVector FloatPosition = FMath::Lerp(RodTip, Landing, T)
		+ Up * (4 * Height * T * (1 - T));
	if (T >= 1.f)
	{
		const float Phase = WaterElapsed * (2 * PI / FMath::Max(.1f, FloatPeriod));
		FloatPosition += Up * (FMath::Sin(Phase) * FMath::Max(0.f, FloatAmplitude));
	}
	FVector Position = FMath::Lerp(FloatPosition, RodTip, FMath::Clamp(RetrieveAlpha, 0.f, 1.f));
	if (bBiting) Position.Y += FMath::Sin(WaterElapsed * BiteFrequency * 2 * PI) * BiteAmplitude;
	SetActorLocation(Position);
	const FVector Delta = RodTip - Position;
	LineMesh->SetWorldLocation((Position + RodTip) * .5);
	LineMesh->SetWorldRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
	LineMesh->SetWorldScale3D(FVector(.008f, .008f, Delta.Size() / 100));
}
