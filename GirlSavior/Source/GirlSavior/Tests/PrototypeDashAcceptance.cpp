#if WITH_EDITOR

#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Combat/GirlSaviorSafeArea.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
// Drive actual controller key edges and let regular actor ticks run each frame in Demo.
class FDashAcceptance
{
public:
    explicit FDashAcceptance(UWorld& InWorld)
        : World(&InWorld)
    {
        Controller = Cast<AGirlSaviorPlayerController>(InWorld.GetFirstPlayerController());
        Hero = Controller.IsValid() ? Cast<AGirlSaviorCharacter>(Controller->GetPawn()) : nullptr;
        if (!Hero.IsValid() || !Controller->CanUseGameplayInput() || Controller->IsInSafeArea())
        {
            Check(false, TEXT("PIE needs a focused foreground viewport and a hero outside the safe area"));
            Save();
            return;
        }
        Origin = Hero->GetActorLocation();
        Controller->FlushPressedKeys();
        Place(FVector(-450, -450, 90));
        TestStart = Hero->GetActorLocation();
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FDashAcceptance::Tick);
        CleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FDashAcceptance::Cleanup);
    }

    ~FDashAcceptance() { Stop(); }

private:
    enum class EPhase { Prepare, IdlePress, FirstPress, Watch, Repeat, FreshPress, Cooldown, SafeArea };

    UGirlSaviorCharacterMovementComponent& Movement() const
    {
        return *CastChecked<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement());
    }

    void Send(FKey Key, EInputEvent Event)
    {
        const ULocalPlayer* Player = Controller->GetLocalPlayer();
        FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
        Controller->InputKey(FInputKeyEventArgs(Viewport, INPUTDEVICEID_NONE, Key, Event, FPlatformTime::Cycles64()));
    }

    void Place(const FVector& Position)
    {
        Movement().StopMovementImmediately();
        Movement().SetMovementMode(MOVE_None);
        Hero->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
        Movement().SetMovementMode(MOVE_Walking);
        Hero->GetCapsuleComponent()->UpdateOverlaps();
        CastChecked<AGirlSaviorPlayerCameraManager>(Controller->PlayerCameraManager)->RequestFollowSnap();
    }

    void Check(bool bPass, const TCHAR* Label)
    {
        ++Checks;
        Failures += !bPass;
        Lines += FString::Printf(TEXT("%s %s\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Label);
    }

    void Save() const
    {
        const FString Folder = FPaths::ProjectSavedDir() / TEXT("CP0");
        IFileManager::Get().MakeDirectory(*Folder, true);
        const FString Path = Folder / TEXT("DashAcceptance.txt");
        const bool bSaved = FFileHelper::SaveStringToFile(
            Lines + FString::Printf(TEXT("checks=%d failures=%d\n"), Checks, Failures), *Path);
        UE_LOG(LogTemp, Display, TEXT("CP0 dash acceptance: %d checks, %d failures, saved=%d."), Checks, Failures, bSaved);
    }

    void Stop()
    {
        if (TickHandle.IsValid())
        {
            FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
            FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
            TickHandle.Reset();
            if (Hero.IsValid() && Controller.IsValid() && Controller->GetPawn() == Hero.Get())
            {
                Controller->FlushPressedKeys();
                Place(Origin);
            }
        }
    }

    void Cleanup(UWorld* InWorld, bool, bool)
    {
        if (InWorld == World.Get())
        {
            FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
            FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
            TickHandle.Reset();
        }
    }

    void Tick(UWorld* InWorld, ELevelTick, float)
    {
        if (InWorld != World.Get())
        {
            return;
        }
        if (!Hero.IsValid() || !Controller.IsValid() || !Controller->CanUseGameplayInput() || ++Frames > 1000)
        {
            Check(false, TEXT("PIE fixture lost viewport focus or exceeded its frame budget"));
            Save();
            Stop();
            return;
        }

        auto& Move = Movement();
        switch (Phase)
        {
        case EPhase::Prepare:
        {
            int32 Width = 0, Height = 0;
            Controller->GetViewportSize(Width, Height);
            Controller->SetMouseLocation(Width * 3 / 4, Height / 2);
            Send(EKeys::LeftShift, IE_Pressed);
            Phase = EPhase::IdlePress;
            break;
        }
        case EPhase::IdlePress:
            Check(!Move.IsDashing() && Move.GetDashCooldownRemaining() == 0.0, TEXT("Shift alone spends nothing"));
            Send(EKeys::LeftShift, IE_Released);
            Send(EKeys::W, IE_Pressed);
            Send(EKeys::LeftShift, IE_Pressed);
            Send(EKeys::LeftMouseButton, IE_Pressed);
            Phase = EPhase::FirstPress;
            break;
        case EPhase::FirstPress:
            Check(Move.IsDashing() && Move.IsDashProtected(), TEXT("Fresh W + Shift starts the dash and its protection"));
            for (TActorIterator<AGirlSaviorTestProjectile> Shot(InWorld); Shot; ++Shot)
            {
                bShotWhileDashing |= Shot->GetOwner() == Hero.Get() && Shot->GetGameTimeSinceCreation() < 0.1f;
            }
            Check(bShotWhileDashing && Hero->GetAimComponent()->IsCursorInsideGameView(),
                TEXT("The live cursor and LMB still aim and launch a projectile during the dash"));
            Send(EKeys::W, IE_Released);
            Phase = EPhase::Watch;
            break;
        case EPhase::Watch:
            if (Move.IsDashing())
            {
                bObservedProtectionExpiry |= !Move.IsDashProtected();
                break;
            }
            if (!bCheckedFirstDash)
            {
                const double Distance = FVector::Dist2D(TestStart, Hero->GetActorLocation());
                Lines += FString::Printf(TEXT("Straight displacement %.3f cm.\n"), Distance);
                Check(FMath::IsNearlyEqual(Distance, 300.0, 0.2), TEXT("Live capsule travels 300 cm after W is released"));
                Check(bObservedProtectionExpiry && !Move.IsDashProtected(), TEXT("Protection expires during the active dash"));
                Check(Move.MovementMode == MOVE_Walking, TEXT("Full dash returns to walking"));
                bCheckedFirstDash = true;
            }
            if (Move.GetDashCooldownRemaining() <= 0.0)
            {
                Send(EKeys::LeftMouseButton, IE_Released);
                Send(EKeys::W, IE_Pressed);
                Send(EKeys::LeftShift, IE_Repeat);
                Phase = EPhase::Repeat;
            }
            break;
        case EPhase::Repeat:
            Check(!Move.IsDashing() && Move.GetDashCooldownRemaining() == 0.0, TEXT("Held Shift and repeat do not rearm"));
            Send(EKeys::LeftShift, IE_Released);
            Send(EKeys::D, IE_Pressed);
            Send(EKeys::LeftShift, IE_Pressed);
            Phase = EPhase::FreshPress;
            break;
        case EPhase::FreshPress:
        {
            Check(Move.IsDashing() && Move.IsDashProtected(), TEXT("Fresh Shift with diagonal WASD rearms after cooldown"));
            const double Remaining = Move.GetDashCooldownRemaining();
            Controller->SetGameplayInputAvailable(false);
            Check(!Move.IsDashing() && !Move.IsDashProtected(), TEXT("Control loss cancels motion and protection immediately"));
            Check(Remaining > 0.0 && Move.GetDashCooldownRemaining() == Remaining, TEXT("Interruption preserves the ready time"));
            Controller->FlushPressedKeys();
            Controller->SetGameplayInputAvailable(true);
            Phase = EPhase::Cooldown;
            break;
        }
        case EPhase::Cooldown:
            if (Move.GetDashCooldownRemaining() <= 0.0)
            {
                TActorIterator<AGirlSaviorSafeArea> Area(InWorld);
                if (Area)
                {
                    FVector Position = Area->GetActorLocation();
                    Position.Z = 90.0;
                    Place(Position);
                }
                Check(Area && Controller->IsInSafeArea() && !Controller->IsFireAllowed(), TEXT("Authored safe area blocks combat input"));
                Send(EKeys::W, IE_Pressed);
                Send(EKeys::LeftShift, IE_Pressed);
                Phase = EPhase::SafeArea;
            }
            break;
        case EPhase::SafeArea:
            Check(!Move.IsDashing() && Move.GetDashCooldownRemaining() == 0.0, TEXT("Safe-area press cannot start or consume the dash"));
            Save();
            Stop();
            break;
        }
    }

    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AGirlSaviorPlayerController> Controller;
    TWeakObjectPtr<AGirlSaviorCharacter> Hero;
    FVector Origin;
    FVector TestStart;
    FDelegateHandle TickHandle, CleanupHandle;
    FString Lines;
    EPhase Phase = EPhase::Prepare;
    int32 Frames = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    bool bObservedProtectionExpiry = false;
    bool bCheckedFirstDash = false;
    bool bShotWhileDashing = false;
};

TUniquePtr<FDashAcceptance> DashAcceptance;
FAutoConsoleCommandWithWorld DashAcceptanceCommand(TEXT("GirlSavior.TestDash"),
    TEXT("Run the CP0 keyboard, motion, protection and safe-area probes in focused Demo PIE."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (World && World->WorldType == EWorldType::PIE)
        {
            DashAcceptance.Reset();
            DashAcceptance = MakeUnique<FDashAcceptance>(*World);
        }
    }));
}

#endif
