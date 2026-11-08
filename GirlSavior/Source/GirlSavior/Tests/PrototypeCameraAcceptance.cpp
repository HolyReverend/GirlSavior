#if WITH_EDITOR
#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Camera/GirlSaviorCameraRoomBounds.h"
#include "Camera/GirlSaviorPrototypeWarningFixture.h"
#include "Camera/GirlSaviorPrototypeForegroundOccluder.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"

namespace GirlSaviorCameraAcceptance
{
static void Run(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->WorldType != EWorldType::PIE) return;
	APlayerController* PC = World->GetFirstPlayerController();
	AGirlSaviorCharacter* Hero = PC ? Cast<AGirlSaviorCharacter>(PC->GetPawn()) : nullptr;
	auto* Camera = PC ? Cast<AGirlSaviorPlayerCameraManager>(PC->PlayerCameraManager) : nullptr;
	ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
	FSceneViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->GetGameViewport() : nullptr;
	if (!Hero || !Camera || !Viewport) return;
	const int32 Width = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 960;
	const int32 Height = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 720;
	if (Width < 320 || Height < 240 || Width > 1920 || Height > 1080) return;
	const FVector OriginalLocation = Hero->GetActorLocation();
	const EMovementMode OriginalMode = Hero->GetCharacterMovement()->MovementMode;
	Hero->GetCharacterMovement()->StopMovementImmediately();
	Hero->GetCharacterMovement()->SetMovementMode(MOVE_None);
	Viewport->SetFixedViewportSize(Width, Height);
	int32 Failures = 0, Checks = 0;
	FString Lines;
	auto Check = [&](bool bPass, const TCHAR* Label)
	{
		++Checks;
		if (!bPass) ++Failures;
		Lines += FString::Printf(TEXT("%s %s\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Label);
	};
	auto Position = [&](const FVector& Point, bool bSnap)
	{
		Hero->SetActorLocation(Point, false, nullptr, ETeleportType::TeleportPhysics);
		if (bSnap) Camera->RequestFollowSnap();
	};
	auto Evaluate = [&](float DT)
	{
		// A normal viewport draw clears this flag; probes deliberately evaluate without a draw.
		Camera->bGameCameraCutThisFrame = false;
		Camera->UpdateCamera(DT);
	};
	auto Anchor = [&]()
	{
		const FMinimalViewInfo& POV = Camera->GetCameraCacheView();
		return POV.Location + POV.Rotation.Vector() * 1600.;
	};
	auto Inset = [&]()
	{
		FSceneViewProjectionData Data;
		if (!Player->GetProjectionData(Viewport, Data)) return false;
		const FVector4 Clip = Data.ComputeViewProjectionMatrix().TransformFVector4(FVector4(Hero->GetActorLocation(), 1.));
		if (!FMath::IsFinite(Clip.W) || Clip.W <= 0) return false;
		const double X = .5 + .5 * Clip.X / Clip.W, Y = .5 - .5 * Clip.Y / Clip.W;
		return X >= .2 - 1.e-6 && X <= .8 + 1.e-6 && Y >= .2 - 1.e-6 && Y <= .8 + 1.e-6;
	};
	Check(Viewport->GetSizeXY() == FIntPoint(Width, Height), TEXT("actual viewport size"));
	for (int32 FPS : {60, 30})
	{
		Position(FVector(0, -300, 90), true); Evaluate(0);
		Position(FVector(100, -300, 90), false);
		double PreviousError = 100.;
		for (int32 Frame = 0; Frame < FPS / 5; ++Frame)
		{
			Evaluate(1.f / FPS);
			const double Error = FVector::DistXY(Anchor(), Hero->GetActorLocation());
			Check(Error <= PreviousError + .001 && Anchor().X <= 100.001, TEXT("follow has no overshoot"));
			PreviousError = Error;
		}
		Check(PreviousError <= 5.01 && PreviousError >= 4.9, TEXT("95 percent settled in 0.20 seconds"));
	}
	// Probe large instantaneous displacement without snap: the margin overrides room preference and lag.
	for (const FVector Point : {FVector(-1100,-1000,90), FVector(2300,400,90), FVector(1400,-400,90), FVector(-1100,1000,90)})
	{
		Position(Point, false); Evaluate(1.f/60);
		Check(Inset(), TEXT("hero inside 20 percent inset after large displacement"));
		const FMinimalViewInfo& POV = Camera->GetCameraCacheView();
		Check(FMath::IsNearlyEqual(POV.FOV,45.f) && POV.Rotation.Equals(FRotator(-35,45,0),.001), TEXT("fixed lens and heading"));
	}
		// Free doorway approach and exit must not inherit either room's static-anchor interval.
	Position(FVector(900,100,90), true); Evaluate(0);
	for (int32 Direction : {1, -1})
	{
		double PreviousX = Anchor().X;
		for (int32 Step = 1; Step <= 9; ++Step)
		{
			const double X = Direction > 0 ? 900. + Step*100. : 1800. - Step*100.;
			FHitResult RouteHit;
			Hero->SetActorLocation(FVector(X,100,90),true,&RouteHit);
			Check(!RouteHit.bBlockingHit && FMath::Abs(Hero->GetActorLocation().X-X)<1., TEXT("doorway route physically clear"));
			for (int32 Frame=0; Frame<21; ++Frame) Evaluate(1.f/60);
			Check((Anchor().X-PreviousX)*Direction>1. && Inset(), TEXT("free doorway camera advances without room clamp hold"));
			PreviousX=Anchor().X;
		}
	}
	// The solid partition outside doorway width retains normal room-edge framing.
	Position(FVector(1000,400,90),true); Evaluate(0);
	const FVector WallAnchor=Anchor();
	Position(FVector(1100,400,90),true); Evaluate(0);
	Check(Anchor().Equals(WallAnchor,.001), TEXT("exposed boundary remainder retains wall inset"));

	Position(FVector(2000,200,90),true); Evaluate(0);
	Check(Anchor().Equals(FVector(2000,200,90),.001), TEXT("shared room region follows inside external inset"));
	Position(FVector(2300,0,90),true); Evaluate(0);
	Check(Inset() && Camera->bGameCameraCutThisFrame, TEXT("explicit same pawn snap updates first view and cut"));
	const FVector Destination = Anchor();
	Camera->RequestFollowSnap(); Evaluate(0);
	Check(Anchor().Equals(Destination,.001), TEXT("snap has no stale follow history"));
	// Regression: the engine bypasses UpdateViewTargetInternal for ACameraActor.
	ACameraActor* Cinematic = World->SpawnActor<ACameraActor>();
	PC->SetViewTarget(Cinematic); Evaluate(0);
	Position(FVector(0,-300,90),false);
	PC->SetViewTarget(Hero); Evaluate(0);
	const FVector Returned = Anchor();
	Check(Camera->bGameCameraCutThisFrame, TEXT("return from camera actor cuts immediately"));
	Camera->RequestFollowSnap(); Evaluate(0);
	Check(Returned.Equals(Anchor(),.001), TEXT("camera actor return resolves destination without stale history"));
	Cinematic->Destroy();

	AGirlSaviorPrototypeForegroundOccluder* Occluder = nullptr;
	AGirlSaviorPrototypeWarningFixture* Warning = nullptr;
	for (TActorIterator<AGirlSaviorPrototypeForegroundOccluder> It(World); It; ++It) { Occluder = *It; break; }
	for (TActorIterator<AGirlSaviorPrototypeWarningFixture> It(World); It; ++It) { Warning = *It; break; }
	Check(Occluder && Warning, TEXT("occlusion and warning fixtures present"));
	if (Occluder && Warning)
	{
		auto Fade = [&]() { const auto& Data = Occluder->Upper->GetCustomPrimitiveData().Data; return Data.Num() ? Data[0] : 0.f; };
		Warning->SetActorHiddenInGame(true);
		Position(FVector(1600,50,90),true); Evaluate(0);
		Check(FMath::IsNearlyEqual(Fade(),.8f,.001f), TEXT("hero overlap hit fades upper immediately on cut"));
		Position(FVector(600,-600,90),true); Evaluate(0);
		Check(Fade() == 0.f, TEXT("cut restores old occluder"));
		Occluder->Upper->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Position(FVector(1600,50,90),true); Evaluate(0);
		Occluder->Upper->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Evaluate(.075f);
		Check(FMath::IsNearlyEqual(Fade(),.4f,.001f), TEXT("normal fade is halfway at 0.075 seconds"));
		Evaluate(.075f);
		Check(FMath::IsNearlyEqual(Fade(),.8f,.001f), TEXT("normal fade reaches minimum coverage"));
		Check(Occluder->GameplayBlocker->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
			&& Occluder->Boundary->IsVisible(), TEXT("fade preserves blocker and visible boundary"));
		Position(FVector(1700,-300,90),true); Evaluate(0);
		const float HeroOnlyFade = Fade();
		Warning->SetActorHiddenInGame(false);
		for(int32 Index=0; Index<9; ++Index) Evaluate(1.f/60);
		Check(HeroOnlyFade == 0.f && FMath::IsNearlyEqual(Fade(),.8f,.001f), TEXT("active warning independently fades foreground"));
		Warning->SetActorHiddenInGame(true);
		for(int32 Index=0; Index<9; ++Index) Evaluate(1.f/60);
		Check(Fade() <= .001f, TEXT("clear sample restores within 0.15 seconds"));
		Position(FVector(1600,50,90),true); Evaluate(0);
		Check(Fade() > .79f, TEXT("view exit begins while obstructing"));
		PC->SetViewTarget(PC); Evaluate(0);
		Check(Fade() == 0.f, TEXT("view exit restores primitive data"));
		PC->SetViewTarget(Hero);
		Position(FVector(1410,250,90),true);
		FHitResult Contact;
		Hero->SetActorLocation(FVector(1410,-140,90),true,&Contact);
		Check(Contact.bBlockingHit && Contact.GetActor()==Occluder, TEXT("capsule sweep physically hits foreground blocker"));
	}
	Position(OriginalLocation,true); Evaluate(0);
	Hero->GetCharacterMovement()->SetMovementMode(OriginalMode);
	Viewport->SetFixedViewportSize(0,0);
	const FString Folder=FPaths::ProjectSavedDir()/TEXT("Stage2");
	IFileManager::Get().MakeDirectory(*Folder,true);
	Lines += FString::Printf(TEXT("checks=%d failures=%d viewport=%dx%d\n"), Checks,Failures,Width,Height);
	FFileHelper::SaveStringToFile(Lines,*(Folder/FString::Printf(TEXT("Camera%dx%d.txt"),Width,Height)));
	UE_LOG(LogTemp,Display,TEXT("Stage2 camera acceptance checks=%d failures=%d viewport=%dx%d"),Checks,Failures,Width,Height);
}
static FAutoConsoleCommandWithWorldAndArgs SnapCommand(TEXT("GirlSavior.CameraSnap"),
	TEXT("Editor PIE: reposition the prototype pawn to X Y at floor height and request a follow cut."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
	{
		if (!World || World->WorldType!=EWorldType::PIE || Args.Num()!=2) return;
		APlayerController* PC=World->GetFirstPlayerController();
		AGirlSaviorCharacter* Hero=PC ? Cast<AGirlSaviorCharacter>(PC->GetPawn()) : nullptr;
		auto* Camera=PC ? Cast<AGirlSaviorPlayerCameraManager>(PC->PlayerCameraManager) : nullptr;
		double X=0., Y=0.;
		if (!Hero || !Camera || !LexTryParseString(X,*Args[0]) || !LexTryParseString(Y,*Args[1])
			|| !FMath::IsFinite(X) || !FMath::IsFinite(Y)) return;
		Hero->GetCharacterMovement()->StopMovementImmediately();
		Hero->SetActorLocation(FVector(X,Y,90.),false,nullptr,ETeleportType::TeleportPhysics);
		Camera->RequestFollowSnap();
	}));
static FAutoConsoleCommandWithWorldAndArgs Command(TEXT("GirlSavior.TestCamera"),
	TEXT("Probe real PIE camera at width height; saved Stage2 evidence, then restore pawn and viewport."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Run));
}
#endif
