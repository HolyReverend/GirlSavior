#if WITH_EDITOR
#include "Characters/GirlSaviorAimComponent.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include <limits>

// Friend access is confined to deterministic editor probes; no physical input is synthesized.
struct FGirlSaviorAimAcceptance
{
 static void Run(const TArray<FString>& Args, UWorld* World)
 {
  if (!World || World->WorldType != EWorldType::PIE) return;
  auto* PC = Cast<AGirlSaviorPlayerController>(World->GetFirstPlayerController());
  auto* Hero = PC ? Cast<AGirlSaviorCharacter>(PC->GetPawn()) : nullptr;
  auto* Aim = Hero ? Hero->FindComponentByClass<UGirlSaviorAimComponent>() : nullptr;
  ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
  FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
  if (!PC || !Hero || !Aim || !Viewport) return;
  const int32 Width = Args.Num() ? FCString::Atoi(*Args[0]) : 960;
  const int32 Height = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 720;
  FString Lines = TEXT("Deterministic math/lifecycle probes; physical mouse, focus, pause route remains manual.\n");
  int32 Checks = 0, Failures = 0;
  auto Check = [&](bool Pass, const TCHAR* Label)
  {
   ++Checks; Failures += !Pass;
   Lines += FString::Printf(TEXT("%03d %s %s\n"), Checks, Pass ? TEXT("PASS") : TEXT("FAIL"), Label);
  };
  const FTransform SavedTransform = Hero->GetActorTransform();
  const FVector SavedDirection = Aim->AimDirection;
  const bool SavedHold = Aim->bHolding, SavedCursor = Aim->bCursorInsideGameView;
  const float SavedEnter = Aim->HoldEnterFraction, SavedExit = Aim->HoldExitFraction;
  const bool SavedAvailable = PC->GetGameplayInputAvailable();
  AActor* SavedTarget = PC->GetViewTarget();
    Check(Width >= 320 && Height >= 240 && Width <= 1920 && Height <= 1080, TEXT("requested probe rectangle is valid"));
  Lines += FString::Printf(TEXT("live viewport=%dx%d; synthetic probe rectangle=%dx%d uses live final camera matrix\n"),
   Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Width, Height);
  Check(Aim->PrimaryComponentTick.TickGroup == TG_PostUpdateWork && !Aim->PrimaryComponentTick.bRunOnAnyThread,
   TEXT("post-camera game-thread aim tick"));
  Aim->HoldEnterFraction = .01f; Aim->HoldExitFraction = .015f;
  FSceneViewProjectionData Data;
  const bool Projected = Player->GetProjectionData(Viewport, Data);
  Check(Projected, TEXT("owning local player projection available"));
  if (Projected)
  {
   const FMatrix Matrix = Data.ComputeViewProjectionMatrix();
   const FVector Body = Hero->GetCapsuleComponent()->GetComponentLocation();
   for (int32 Offset : {0, 37})
   {
    const FIntRect OriginalRect = Data.GetConstrainedViewRect();
    const FIntRect Rect(OriginalRect.Min + FIntPoint(Offset, Offset), OriginalRect.Min + FIntPoint(Offset + Width, Offset + Height));
    FVector2D Center;
    Check(FSceneView::ProjectWorldToScreen(Body, Rect, Matrix, Center), TEXT("body projects with viewport origin"));
    const double Inner = .01 * Rect.Height(), Outer = .015 * Rect.Height();
    Lines += FString::Printf(TEXT("rect origin=%d,%d height=%d inner=%.6f outer=%.6f pixels\n"),
     Rect.Min.X, Rect.Min.Y, Rect.Height(), Inner, Outer);
    auto Sample = [&](FVector2D Cursor) { Aim->ResolveViewSample(Body, Cursor, Rect, Matrix); };
    Aim->AimDirection = FVector::ForwardVector; Aim->bHolding = false;
    Sample(Center);
    Check(Aim->bHolding && Aim->AimDirection == FVector::ForwardVector, TEXT("center enters hold and retains facing"));
    Aim->bHolding = false;
    Sample(Center + FVector2D(double(Aim->HoldEnterFraction) * Rect.Height(), 0));
    Check(Aim->bHolding, TEXT("exact configured inner radius enters hold inclusively"));
    Sample(Center + FVector2D(Inner * .999, 0));
    Check(Aim->bHolding, TEXT("inside inner radius holds"));
    Sample(Center + FVector2D((Inner + Outer) * .5, 0));
    Check(Aim->bHolding, TEXT("band retains preceding hold"));
    Sample(Center + FVector2D(Outer * .999, 0));
    Check(Aim->bHolding, TEXT("inside outer radius holds"));
    Sample(Center + FVector2D(double(Aim->HoldExitFraction) * Rect.Height(), 0));
    Check(Aim->bHolding, TEXT("exact configured outer radius retains hold inclusively"));
    Sample(Center + FVector2D(Outer * 1.01, 0));
    Check(!Aim->bHolding, TEXT("beyond outer radius resumes tracking"));
    Sample(Center + FVector2D((Inner + Outer) * .5, 0));
    Check(!Aim->bHolding, TEXT("band retains preceding tracking"));
    for (const FVector Delta : {FVector(180, 70, 0), FVector(-100, 210, 0)})
    {
     FVector2D Cursor;
     FSceneView::ProjectWorldToScreen(Body + Delta, Rect, Matrix, Cursor);
     Hero->SetActorRotation(FRotator(7, 20, -4));
     Sample(Cursor);
      FVector2D ResolvedScreen;
     FSceneView::ProjectWorldToScreen(Body + Aim->AimDirection * Delta.Size(), Rect, Matrix, ResolvedScreen);
     Check(FVector2D::Distance(ResolvedScreen, Cursor) <= 2., TEXT("independent world target within two pixels after UE integer deprojection"));
     Check(FMath::IsNearlyEqual(Hero->GetActorRotation().Pitch, 7., 1.e-4)
      && FMath::IsNearlyEqual(Hero->GetActorRotation().Roll, -4., 1.e-4), TEXT("yaw preserves pitch and roll"));
    }
    const FVector HeldDirection = Aim->AimDirection;
    Aim->bHolding = true;
    for (FMatrix Bad : {FMatrix::Identity})
    {
     // A singular matrix must not become a plausible identity inverse.
     Bad.M[0][0] = 0.; Bad.M[0][1] = 0.; Bad.M[0][2] = 0.; Bad.M[0][3] = 0.;
     Aim->ResolveViewSample(Body, Center, Rect, Bad);
     Check(Aim->bHolding && Aim->AimDirection == HeldDirection && Aim->IsCursorInsideGameView(),
      TEXT("singular geometry retains mode/facing but in-view sample remains true"));
    }
    FMatrix Nonfinite = Matrix; Nonfinite.M[0][0] = std::numeric_limits<double>::infinity();
    Aim->ResolveViewSample(Body, Center, Rect, Nonfinite);
    Check(Aim->bHolding && Aim->AimDirection == HeldDirection && Aim->IsCursorInsideGameView(), TEXT("nonfinite matrix rejected independently of cursor validity"));
    Sample(FVector2D(Rect.Max.X, Center.Y));
    Check(!Aim->IsCursorInsideGameView() && Aim->bHolding && Aim->AimDirection == HeldDirection, TEXT("exclusive maximum rejects cursor and preserves state"));
    Sample(FVector2D(Rect.Min.X - 1, Center.Y));
    Check(!Aim->IsCursorInsideGameView(), TEXT("cursor before minimum rejected"));
    Sample(FVector2D(Rect.Min.X, Rect.Min.Y));
    Check(Aim->IsCursorInsideGameView(), TEXT("exact minimum is inside independently of geometry"));
    const FVector BeforeInvalid = Aim->AimDirection; const bool HoldBeforeInvalid = Aim->bHolding;
    Sample(FVector2D(std::numeric_limits<double>::quiet_NaN(), Center.Y));
    Check(!Aim->IsCursorInsideGameView() && Aim->AimDirection == BeforeInvalid && Aim->bHolding == HoldBeforeInvalid, TEXT("NaN cursor rejected without changing facing or mode"));
    Aim->ResolveViewSample(Body, Center, FIntRect(Rect.Min, Rect.Min), Matrix);
    Check(!Aim->IsCursorInsideGameView() && Aim->AimDirection == BeforeInvalid && Aim->bHolding == HoldBeforeInvalid, TEXT("empty rectangle rejected without changing facing or mode"));
    const FVector ElevatedBody = Body + FVector(0, 0, 130);
    const FVector Target = ElevatedBody + FVector(140, -170, 0);
    FVector2D ElevatedCursor;
    FSceneView::ProjectWorldToScreen(Target, Rect, Matrix, ElevatedCursor);
    Aim->bHolding = false;
    Aim->ResolveViewSample(ElevatedBody, ElevatedCursor, Rect, Matrix);
    FVector2D ElevatedResolved;
    FSceneView::ProjectWorldToScreen(ElevatedBody + Aim->AimDirection * (Target - ElevatedBody).Size(), Rect, Matrix, ElevatedResolved);
    Check(FVector2D::Distance(ElevatedResolved, ElevatedCursor) <= 2., TEXT("plane follows changed body height within two-pixel deprojection tolerance"));
    const FVector FirstDirection = Aim->AimDirection;
    const FVector ShiftedBody = ElevatedBody + FVector(40, 0, 0);
    Aim->ResolveViewSample(ShiftedBody, ElevatedCursor, Rect, Matrix);
    FVector2D ShiftedResolved;
    FSceneView::ProjectWorldToScreen(ShiftedBody + Aim->AimDirection * (Target - ShiftedBody).Size(), Rect, Matrix, ShiftedResolved);
    Check(FVector2D::Distance(ShiftedResolved, ElevatedCursor) <= 2.
     && !Aim->AimDirection.Equals(FirstDirection, 1.e-3), TEXT("stationary screen cursor recomputes after body moves"));
   }
   FSceneViewProjectionData Horizontal = Data;
   Horizontal.ViewOrigin = Body + FVector(-500, 0, 100);
   Horizontal.ViewRotationMatrix = FMatrix(
    FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
   const FIntRect Rect(0, 0, Width, Height);
   const FVector Previous = Aim->AimDirection;
   Aim->bHolding = true;
   Aim->ResolveViewSample(Body, FVector2D(Width / 2, Height / 2), Rect, Horizontal.ComputeViewProjectionMatrix());
   Check(Aim->bHolding && Aim->AimDirection == Previous && Aim->IsCursorInsideGameView(), TEXT("horizontal ray preserves hold and facing"));
   Aim->ResolveViewSample(Body, FVector2D(Width / 2, Height / 4), Rect, Horizontal.ComputeViewProjectionMatrix());
   Check(Aim->bHolding && Aim->AimDirection == Previous && Aim->IsCursorInsideGameView(), TEXT("plane intersection behind ray preserves hold and facing"));
  }
  Aim->bCursorInsideGameView = true; Aim->bHolding = true;
  const FVector BeforeHandoff = Aim->AimDirection;
  PC->FlushPressedKeys();
  Check(!Aim->IsCursorInsideGameView() && Aim->bHolding && Aim->AimDirection == BeforeHandoff, TEXT("flush clears sample only"));
  Aim->bCursorInsideGameView = true;
  PC->SetGameplayInputAvailable(false);
  Check(!PC->CanUseGameplayInput() && !Aim->IsCursorInsideGameView() && Aim->bHolding
   && Aim->AimDirection == BeforeHandoff, TEXT("explicit suspension gates input and retains facing/hold"));
  PC->SetGameplayInputAvailable(true);
  const bool GateBeforeMoveLock = PC->CanUseGameplayInput();
  Lines += GateBeforeMoveLock ? TEXT("Shared input gate baseline available.\n") : TEXT("NotRun: positive shared-gate baseline (live focus/foreground unavailable); negative gate probes remain valid.\n");
  PC->SetIgnoreMoveInput(true);
  Check(PC->CanUseGameplayInput() == GateBeforeMoveLock, TEXT("movement-specific lock does not alter shared gate"));
  PC->SetIgnoreMoveInput(false);
  ACameraActor* OtherView = World->SpawnActor<ACameraActor>();
  if (OtherView)
  {
   PC->SetViewTarget(OtherView);
   Check(!PC->CanUseGameplayInput(), TEXT("different camera view target gates gameplay"));
   PC->SetViewTarget(SavedTarget); OtherView->Destroy();
  }
  else Check(false, TEXT("temporary camera spawn"));
  for (double Yaw : {63., -117.})
  {
   const FTransform SpawnTransform(FRotator(0, Yaw, 0), FVector(0, 0, 10000));
   auto* Spawned = World->SpawnActorDeferred<AGirlSaviorCharacter>(AGirlSaviorCharacter::StaticClass(),
    SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if (Spawned)
   {
    Spawned->FinishSpawning(SpawnTransform);
    auto* Seed = Spawned->FindComponentByClass<UGirlSaviorAimComponent>();
    Check(Seed && Seed->GetAimDirection().Equals(FRotator(0, Yaw, 0).Vector(), 1.e-6)
     && !Seed->IsHoldingAim() && !Seed->IsCursorInsideGameView(), TEXT("each new pawn seeds its own spawn yaw"));
    Spawned->Destroy();
   }
   else Check(false, TEXT("deferred hero spawn"));
  }
  Aim->HoldEnterFraction = -.1f; Aim->HoldExitFraction = .8f; Aim->ValidateTuning();
  Check(Aim->HoldEnterFraction == .01f && Aim->HoldExitFraction == .015f, TEXT("invalid tuning falls back to prototype defaults"));
  Aim->bCursorInsideGameView = true;
  const FVector BeforeUnpossess = Aim->AimDirection;
  const bool HoldBeforeUnpossess = Aim->bHolding;
  PC->UnPossess();
  Check(!Aim->IsCursorInsideGameView() && Aim->bHolding == HoldBeforeUnpossess && Aim->AimDirection == BeforeUnpossess,
   TEXT("unpossess invalidates sample before releasing pawn and preserves facing/hold"));
  PC->Possess(Hero);
  Check(Aim->bHolding == HoldBeforeUnpossess && Aim->AimDirection == BeforeUnpossess,
   TEXT("same pawn repossession does not reseed facing or hold"));
  PC->SetViewTarget(SavedTarget);
  PC->SetGameplayInputAvailable(SavedAvailable);
  Hero->SetActorTransform(SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);
  Aim->AimDirection = SavedDirection; Aim->bHolding = SavedHold; Aim->bCursorInsideGameView = SavedCursor;
  Aim->HoldEnterFraction = SavedEnter; Aim->HoldExitFraction = SavedExit;
  Lines += FString::Printf(TEXT("TOTAL checks=%d failures=%d\nNotRun: physical input/focus/pause route; camera-motion sequence.\n"), Checks, Failures);
  const FString Directory = FPaths::ProjectSavedDir() / TEXT("Stage3");
  IFileManager::Get().MakeDirectory(*Directory, true);
  const FString Path = Directory / FString::Printf(TEXT("Aim%dx%d.txt"), Width, Height);
  const bool Saved = FFileHelper::SaveStringToFile(Lines, *Path);
  UE_LOG(LogTemp, Display, TEXT("Aim acceptance: %d checks, %d failures; report saved=%d: %s"), Checks, Failures, Saved, *Path);
 }
};
static FAutoConsoleCommandWithWorldAndArgs AimAcceptanceCommand(TEXT("GirlSavior.TestAim"),
 TEXT("Run deterministic Stage 3 aiming probes in PIE at the current viewport width and height."),
 FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FGirlSaviorAimAcceptance::Run));
#endif
