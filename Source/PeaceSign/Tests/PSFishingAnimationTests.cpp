#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../PSPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPSFishingAnimationTest, "PeaceSign.Fishing.CastPreparationAnimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPSFishingAnimationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	APSPlayerCharacter* Character = World->SpawnActor<APSPlayerCharacter>();
	const auto MakeClip = [World](const TArray<int32>& Runs)
	{
		UPaperFlipbook* Clip = NewObject<UPaperFlipbook>(World);
		{
			FScopedFlipbookMutator Mutator(Clip);
			Mutator.FramesPerSecond = 10;
			for (const int32 Run : Runs)
			{
				FPaperFlipbookKeyFrame& Frame = Mutator.KeyFrames.AddDefaulted_GetRef();
				Frame.Sprite = NewObject<UPaperSprite>(World);
				Frame.FrameRun = Run;
			}
		}
		return Clip;
	};
	UPaperFlipbook* Idle = MakeClip({10});
	UPaperFlipbook* CastAnimation = MakeClip({2, 3, 1, 1});
	Character->GetSprite()->SetFlipbook(Idle);
	Character->GetSprite()->SetLooping(true);
	Character->GetSprite()->SetPlaybackPosition(.1f, false);
	Character->GetSprite()->Stop();
	Character->FishingCastFlipbook = CastAnimation;
	const FRotator RotationBefore = Character->GetActorRotation();
	TestEqual(TEXT("Preparation duration follows the first two sprite frame runs"),
		Character->BeginFishingCastPreparation(FVector(0, 1, 0)), .5f);
	TestEqual(TEXT("Casting faces the selected world direction"), Character->GetFacingCellOffset(), FIntPoint(0, 1));
	TestEqual(TEXT("Sprite facing does not rotate the character plane"), Character->GetActorRotation(), RotationBefore);
	TestEqual(TEXT("Preparation uses the assigned casting clip"), Character->GetSprite()->GetFlipbook(), CastAnimation);
	TestFalse(TEXT("Automatic playback cannot run past the preparation frames"), Character->GetSprite()->IsPlaying());
	Character->UpdateFishingCastPreparation(.25f);
	TestEqual(TEXT("Preparation advances to the second sprite"), Character->GetSprite()->GetPlaybackPositionInFrames(), 2);
	Character->UpdateFishingCastPreparation(10.f);
	TestEqual(TEXT("Preparation holds the last frame of sprite two"), Character->GetSprite()->GetPlaybackPositionInFrames(), 4);
	Character->PlayFishingCastThrow();
	TestEqual(TEXT("Throw starts at sprite three"), Character->GetSprite()->GetPlaybackPositionInFrames(), 5);
	TestTrue(TEXT("Remaining throw animation plays after confirmation"), Character->GetSprite()->IsPlaying());
	Character->UpdateFishingCastPreparation(0);
	TestEqual(TEXT("Preparation updates cannot rewind a confirmed throw"), Character->GetSprite()->GetPlaybackPositionInFrames(), 5);
	Character->EndFishingCastAnimation();
	TestEqual(TEXT("Ending fishing restores the previous animation"), Character->GetSprite()->GetFlipbook(), Idle);
	TestTrue(TEXT("Ending fishing restores looping"), Character->GetSprite()->IsLooping());
	TestFalse(TEXT("Ending fishing restores the previous stopped state"), Character->GetSprite()->IsPlaying());
	TestTrue(TEXT("Ending fishing restores the playback position"), FMath::IsNearlyEqual(Character->GetSprite()->GetPlaybackPosition(), .1f));
	Character->BeginFishingCastPreparation(FVector(1, 0, 0));
	Character->UpdateFishingCastPreparation(.1f);
	Character->EndFishingCastAnimation();
	TestEqual(TEXT("Cancelling preparation also restores idle"), Character->GetSprite()->GetFlipbook(), Idle);
	Character->BeginFishingCastPreparation(FVector(1, 0, 0));
	UPaperFlipbook* Success = MakeClip({1, 1});
	Character->GetSprite()->SetFlipbook(Success);
	Character->EndFishingCastAnimation();
	TestEqual(TEXT("Ending casting preserves a Blueprint success animation"), Character->GetSprite()->GetFlipbook(), Success);
	Character->FishingCastFlipbook = nullptr;
	TestEqual(TEXT("An unassigned clip uses the controller fallback timer"), Character->BeginFishingCastPreparation(FVector(1, 0, 0)), 0.f);
	Character->FishingCastFlipbook = MakeClip({1});
	TestEqual(TEXT("A clip missing its second sprite uses the fallback"), Character->BeginFishingCastPreparation(FVector(1, 0, 0)), 0.f);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
