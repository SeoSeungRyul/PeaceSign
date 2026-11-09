#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "UnrealClient.h"
#include "Slate/SceneViewport.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SWindow.h"
#include "GameFramework/Pawn.h"
#include "../World/PSGridWorld.h"
#include "../World/PSTileChunkActor.h"
#include "WaterBodyCustomActor.h"

namespace
{
struct FLinkSmoke
{
	UWorld* World = nullptr; FTimerHandle Timer; int32 Stage = 0; float OldTime = -1;
	TArray<FColor> Before; int32 Width = 0, Height = 0;
	bool bWorldMode=false;
	FVector Probe=FVector(250,900,0);
	bool ReadFrame(TArray<FColor>& Pixels)
	{
		auto* Viewport=GEngine->GameViewport->GetGameViewport();
		auto Window=Viewport ? Viewport->FindWindow() : nullptr;
		FViewportRHIRef RHI=Window ? static_cast<FRHIViewport*>(FSlateApplication::Get().GetRenderer()->GetViewportResource(*Window)) : nullptr;
		if(!RHI.IsValid()) return false;
		ENQUEUE_RENDER_COMMAND(ReadLinkWaterBackBuffer)([RHI,&Pixels](FRHICommandListImmediate& Cmd)
		{
			FTextureRHIRef Buffer=RHIGetViewportBackBuffer(RHI);
			if(Buffer) Cmd.ReadSurfaceData(Buffer,FIntRect(0,0,Buffer->GetSizeX(),Buffer->GetSizeY()),Pixels,FReadSurfaceDataFlags(RCM_UNorm));
		});
		FlushRenderingCommands();
		return Pixels.Num()==Width*Height;
	}
	void Finish(bool Pass)
	{
		if(auto* Time=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.OverrideWavesTime"))) Time->Set(OldTime,ECVF_SetByCode);
		World->GetTimerManager().ClearTimer(Timer);
		UE_LOG(LogTemp,Display,TEXT("%s %s"),bWorldMode ? TEXT("WORLD_NATIVE_WATER_SMOKE") : TEXT("LINK_VIEWPORT_SMOKE"),Pass ? TEXT("PASS") : TEXT("FAIL"));
		FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);
	}
	void Step()
	{
		auto* View=GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
		auto* Time=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.OverrideWavesTime"));
		if(!View || !Time) { Finish(false); return; }
		if(Stage==0)
		{
			if(bWorldMode)
			{
				APSGridWorld* Grid=nullptr; for(TActorIterator<APSGridWorld> It(World);It;++It) { Grid=*It; break; }
				if(!Grid || !Grid->bUseNativeWater) { Finish(false); return; }
				FIntPoint Water=FIntPoint::ZeroValue,Land=FIntPoint::ZeroValue; int32 Best=MAX_int32;
				const FIntPoint Offsets[]={{1,0},{-1,0},{0,1},{0,-1}};
				for(int32 Y=-24;Y<=24;++Y) for(int32 X=-24;X<=24;++X)
					if(Grid->GetGroundTile(FIntPoint(X,Y))==EPSTileType::Water && X*X+Y*Y<Best)
						for(auto Offset:Offsets)
						{
							const auto Ground=Grid->GetGroundTile(FIntPoint(X,Y)+Offset);
							if(Ground==EPSTileType::Grass || Ground==EPSTileType::Dirt)
							{ Water=FIntPoint(X,Y); Land=Water+Offset; Best=X*X+Y*Y; break; }
						}
				if(Best==MAX_int32) { Finish(false); return; }
				if(auto* PC=World->GetFirstPlayerController()) if(PC->GetPawn())
				{
					PC->GetPawn()->SetActorLocation(Grid->CellToWorldCenter(Land)+FVector(0,0,80));
					PC->GetPawn()->SetActorHiddenInGame(true); // Keep the sampled water pixels free of the player sprite.
				}
				Grid->RefreshWaterRendering();
				Probe=Grid->WaterVisualLocation(Water);
				auto* Camera=World->SpawnActor<ACameraActor>();
				Camera->SetActorLocation(Probe+FVector(-400,0,500)); Camera->SetActorRotation(FRotator(-51.34,0,0));
				Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
				Camera->GetCameraComponent()->SetOrthoWidth(600);
				if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera);
				UE_LOG(LogTemp,Display,TEXT("WORLD_NATIVE_WATER target %s"),*Water.ToString());
			}
			else
			for(TActorIterator<ACameraActor> It(World);It;++It)
			{
				It->SetActorLocation(FVector(-400,575,800)); It->SetActorRotation(FRotator(-50,0,0));
				It->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
				It->GetCameraComponent()->SetOrthoWidth(1500);
				if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(*It);
				break;
			}
			OldTime=Time->GetFloat(); Time->Set(0.f,ECVF_SetByCode); ++Stage; return;
		}
		if(Stage==1) { ++Stage; return; }
		if(Stage==2)
		{
			Width=View->GetSizeXY().X; Height=View->GetSizeXY().Y;
			if(!ReadFrame(Before)) { Finish(false); return; }
			Time->Set(.7f,ECVF_SetByCode); ++Stage; return;
		}
		if(Stage==3) { ++Stage; return; }
		TArray<FColor> After;
		if(!ReadFrame(After) || Before.Num()!=After.Num()) { Finish(false); return; }
		FVector2D Center;
		auto* PC=World->GetFirstPlayerController();
		if(!PC || !PC->ProjectWorldLocationToScreen(Probe,Center)) { Finish(false); return; }
		int32 Changed=0,Visible=0;
		for(int32 Y=FMath::RoundToInt(Center.Y)-35; Y<=FMath::RoundToInt(Center.Y)+35; ++Y)
		for(int32 X=FMath::RoundToInt(Center.X)-35; X<=FMath::RoundToInt(Center.X)+35; ++X)
		{
			if(X<0 || Y<0 || X>=Width || Y>=Height) continue;
			const FColor A=Before[Y*Width+X], B=After[Y*Width+X];
			Visible += A.G>25 || A.B>25;
			Changed += FMath::Abs(int32(A.R)-B.R)+FMath::Abs(int32(A.G)-B.G)+FMath::Abs(int32(A.B)-B.B)>8;
		}
		const FString Dir=FPaths::ProjectSavedDir()/TEXT("TileDiagnostics"); IFileManager::Get().MakeDirectory(*Dir,true);
		FImageUtils::SaveImageByExtension(*(Dir/(bWorldMode ? TEXT("NativeWorld0.png") : TEXT("LinkViewport0.png"))),FImageView(Before.GetData(),Width,Height));
		FImageUtils::SaveImageByExtension(*(Dir/(bWorldMode ? TEXT("NativeWorld1.png") : TEXT("LinkViewport1.png"))),FImageView(After.GetData(),Width,Height));
		UE_LOG(LogTemp,Display,TEXT("LINK_VIEWPORT pixels %d changed %d center %s"),Visible,Changed,*Center.ToString());
		if(bWorldMode)
		{
			int32 Bodies=0; for(TActorIterator<AWaterBodyCustom> It(World);It;++It)
				if(Cast<APSTileChunkActor>(It->GetParentActor())) ++Bodies;
			UE_LOG(LogTemp,Display,TEXT("WORLD_NATIVE_WATER bodies %d"),Bodies);
			if(Bodies==0) { Finish(false); return; }
		}
		Finish(Visible>1000 && Changed>100);
	}
};
TSharedPtr<FLinkSmoke> LinkSmoke;
FAutoConsoleCommandWithWorld StartLinkSmoke(TEXT("PS.Water.LinkSmoke"),TEXT("Capture native waves in the isolated game viewport"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if(!World || !World->IsGameWorld()) return;
		LinkSmoke=MakeShared<FLinkSmoke>(); LinkSmoke->World=World;
		World->GetTimerManager().SetTimer(LinkSmoke->Timer,FTimerDelegate::CreateLambda([] { LinkSmoke->Step(); }),1.f,true,5.f);
	}));
FAutoConsoleCommandWithWorld StartWorldWaterSmoke(TEXT("PS.Water.WorldSmoke"),TEXT("Check native water in the existing world; use isolated UserDir"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if(!World || !World->IsGameWorld()) return;
		LinkSmoke=MakeShared<FLinkSmoke>(); LinkSmoke->World=World; LinkSmoke->bWorldMode=true;
		World->GetTimerManager().SetTimer(LinkSmoke->Timer,FTimerDelegate::CreateLambda([] { LinkSmoke->Step(); }),1.f,true,5.f);
	}));
}
#endif
