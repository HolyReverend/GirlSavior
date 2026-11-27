#if WITH_EDITOR

#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Enemies/GirlSaviorEnemyAttackComponent.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
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
#include "Weapons/GirlSaviorWeaponDefinition.h"

namespace
{
// Exercise the placed actors and normal controller/fire path in a fresh encounter.
class FFirstFightProbe
{
public:
    FFirstFightProbe(UWorld& InWorld, bool bDefeatCase)
        : World(&InWorld), bDefeat(bDefeatCase)
    {
        TActorIterator<AGirlSaviorCombatEncounter> EncounterIt(&InWorld);
        Encounter = EncounterIt ? *EncounterIt : nullptr;
        Hero = Encounter.IsValid() ? Encounter->GetHero() : nullptr;
        Controller = Hero.IsValid() ? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
        Enemy = Encounter.IsValid() && !Encounter->PlacedEnemies.IsEmpty() ? Encounter->PlacedEnemies[0].Get() : nullptr;
        const bool bReady = Hero.IsValid() && Controller.IsValid() && Controller->CanUseGameplayInput()
            && Enemy.IsValid() && Encounter->GetRunState() == EGirlSaviorRunState::Waiting;
        Check(bReady, TEXT("Fresh focused CombatPrototype PIE with a placed Pursuer"));
        if (!bReady)
        {
            if (Controller.IsValid())
            {
                const auto* Player = Controller->GetLocalPlayer();
                const FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
                Lines += FString::Printf(TEXT("Input available=%d local=%d viewTargetIsHero=%d focus=%d foreground=%d.\n"),
                    Controller->GetGameplayInputAvailable(), Controller->IsLocalController(),
                    Controller->GetViewTarget() == Hero.Get(), Viewport && Viewport->HasFocus(),
                    Viewport && Viewport->IsForegroundWindow());
            }
            Finish();
            return;
        }

        Check(Controller->IsInSafeArea() && !Controller->IsFireAllowed()
            && !Movement().TryStartDash(FVector::ForwardVector), TEXT("Spawn pocket denies firing and dash"));
        Check(Hit(EGirlSaviorDamageDelivery::DirectMelee, 10).Outcome == EGirlSaviorDamageOutcome::BlockedBySafeArea
            && Receiver().GetHealth() == 100.0, TEXT("Incoming damage leaves health intact in the safe pocket"));
        InitialEnemyLocation = Enemy->GetActorLocation();
        InitialLiveEnemies = Encounter->GetLiveEnemyCount();
        Deadline = InWorld.GetTimeSeconds() + 18.0;
        Send(EKeys::W, IE_Pressed);
        Send(EKeys::A, IE_Pressed);
        PostTick = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FFirstFightProbe::Tick);
        Cleanup = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FFirstFightProbe::OnCleanup);
    }

    ~FFirstFightProbe() { Stop(); }

private:
    enum class EPhase { Enter, Rearm, Expiry, LockedWarning, WaitForMiss, Firing, WaitForExit, WaitForDefeat };

    UGirlSaviorCharacterMovementComponent& Movement() const
    {
        return *CastChecked<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement());
    }

    UGirlSaviorDamageReceiverComponent& Receiver() const { return *Hero->GetDamageReceiver(); }

    FGirlSaviorDamageResult Hit(EGirlSaviorDamageDelivery Delivery, float Amount)
    {
        FGirlSaviorDamageSpec Spec;
        Spec.Delivery = Delivery;
        Spec.Physical = Amount;
        Spec.SourceId = TEXT("CP1.PIE.Fixture");
        Spec.Contact = Hero->GetActorLocation();
        Spec.Origin = Spec.Contact - FVector(0, 60, 0);
        Spec.bHasContact = true;
        return Receiver().ResolveDamage(Spec);
    }

    void Send(const FKey& Key, EInputEvent Event)
    {
        const auto* Player = Controller->GetLocalPlayer();
        FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
        Controller->InputKey(FInputKeyEventArgs(Viewport, INPUTDEVICEID_NONE, Key, Event, FPlatformTime::Cycles64()));
    }

    // Reposition between isolated cases while preserving the current possession and real map geometry.
    void PlaceHero(const FVector& Position)
    {
        Movement().StopMovementImmediately();
        Movement().SetMovementMode(MOVE_None);
        Hero->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
        Movement().SetMovementMode(MOVE_Walking);
        Hero->GetCapsuleComponent()->UpdateOverlaps();
        CastChecked<AGirlSaviorPlayerCameraManager>(Controller->PlayerCameraManager)->RequestFollowSnap();
    }

    void StartDuel()
    {
        const FVector Body = Enemy->GetActorLocation();
        PlaceHero(FVector(Body.X - 130.0, Body.Y, 90.0));
        if (bDefeat)
        {
            Check(Hit(EGirlSaviorDamageDelivery::Area, 90.f).HealthAfter == 10.0,
                TEXT("Fixture leaves the hero one real Pursuer hit from defeat"));
            Phase = EPhase::WaitForDefeat;
            return;
        }
        Check(FVector::Dist2D(Body, InitialEnemyLocation) > 20.0, TEXT("Pursuer approached the hero using the saved navmesh"));
        Phase = EPhase::LockedWarning;
    }

    // Drive the real combat projectile through saved geometry using explicit frame sizes.
    void CheckFlight(const FVector& Start, const FVector& Direction, int32 FramesPerSecond,
        EGirlSaviorProjectileCompletion ExpectedReason, double ExpectedDistance, const TCHAR* Label)
    {
        auto* Fire = Hero->GetFireComponent();
        const auto* Weapon = Fire->GetConfiguredWeaponDefinition();
        if (!Weapon)
        {
            Check(false, TEXT("The saved hero has a configured pistol row"));
            return;
        }
        PlaceHero(Start);
        FGirlSaviorProjectileLaunch Launch;
        Launch.Radius = Weapon->ProjectileRadius;
        Launch.Speed = Weapon->ProjectileSpeed;
        Launch.MaxDistance = Weapon->MaxDistance;
        Launch.Damage.Physical = Weapon->DamagePerProjectile;
        Launch.Damage.Cause = Weapon->DamageCause;
        Launch.Damage.Origin = Hero->GetActorLocation();
        Launch.Damage.SourceFaction = EGirlSaviorFaction::Hero;
        Launch.Damage.Instigator = Hero.Get();
        Launch.Damage.AttackId = Encounter->AllocateAttackId();
        auto* Shot = AGirlSaviorTestProjectile::SpawnCombatProjectile(
            *World.Get(), Weapon->ProjectileClass, Hero.Get(), Direction, Launch, *Encounter.Get());
        if (!IsValid(Shot))
        {
            Check(false, Label);
            return;
        }

        int32 Completions = 0;
        double Distance = 0.0;
        auto Reason = EGirlSaviorProjectileCompletion::Removed;
        const FDelegateHandle Handle = Shot->OnCompleted.AddLambda(
            [&](AGirlSaviorTestProjectile* Finished, EGirlSaviorProjectileCompletion InReason)
        {
            ++Completions;
            Distance = Finished->GetDistanceTraveled();
            Reason = InReason;
        });
        for (int32 Step = 0; Step < FramesPerSecond * 2 && Completions == 0 && IsValid(Shot); ++Step)
        {
            Shot->Tick(1.f / FramesPerSecond);
        }
        if (IsValid(Shot))
        {
            Shot->OnCompleted.Remove(Handle);
            Shot->Destroy();
        }
        Lines += FString::Printf(TEXT("Flight at %d Hz covered %.3f cm.\n"), FramesPerSecond, Distance);
        Check(Completions == 1 && Reason == ExpectedReason && FMath::IsNearlyEqual(Distance, ExpectedDistance, 1.5),
            Label);
    }

    // Place a box so the leading edge of the sphere meets its face exactly at the range limit.
    void CheckRangeEndpoint(double ExtraDistance, EGirlSaviorProjectileCompletion ExpectedReason, const TCHAR* Label)
    {
        const auto* Weapon = Hero->GetFireComponent()->GetConfiguredWeaponDefinition();
        if (!Weapon)
        {
            Check(false, TEXT("The range fixture requires the configured weapon"));
            return;
        }
        auto* Blocker = World->SpawnActor<AActor>();
        if (!Blocker)
        {
            Check(false, TEXT("Spawn an endpoint blocker"));
            return;
        }
        auto* Box = NewObject<UBoxComponent>(Blocker);
        const FVector Start(1000, -850, 90);
        constexpr double BoxHalfWidth = 10.0;
        Blocker->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(BoxHalfWidth));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldDynamic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->SetGenerateOverlapEvents(false);
        Box->SetWorldLocation(Start + FVector::RightVector
            * (Weapon->MaxDistance + Weapon->ProjectileRadius + BoxHalfWidth + ExtraDistance));
        Box->RegisterComponent();
        CheckFlight(Start, FVector::RightVector, 60, ExpectedReason, Weapon->MaxDistance, Label);
        Blocker->Destroy();
    }

    void Check(bool bPass, const TCHAR* Label)
    {
        ++Checks;
        Failures += !bPass;
        Lines += FString::Printf(TEXT("%s %s\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Label);
    }

    void Finish()
    {
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("CP1");
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString File = Directory / (bDefeat ? TEXT("Defeat.txt") : TEXT("FirstFight.txt"));
        const bool bSaved = FFileHelper::SaveStringToFile(
            Lines + FString::Printf(TEXT("checks=%d failures=%d\n"), Checks, Failures), *File);
        UE_LOG(LogTemp, Display, TEXT("CP1 first fight probe (%s): %d checks, %d failures, saved=%d."),
            bDefeat ? TEXT("defeat") : TEXT("duel"), Checks, Failures, bSaved);
        Stop();
    }

    void Stop()
    {
        if (PostTick.IsValid())
        {
            FWorldDelegates::OnWorldPostActorTick.Remove(PostTick);
            FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
            PostTick.Reset();
            if (Controller.IsValid())
            {
                Controller->FlushPressedKeys();
            }
        }
    }

    void OnCleanup(UWorld* InWorld, bool, bool)
    {
        if (InWorld == World.Get())
        {
            Stop();
        }
    }

    void Tick(UWorld* InWorld, ELevelTick, float)
    {
        if (World.Get() != InWorld)
        {
            return;
        }
        if (!Encounter.IsValid() || !Hero.IsValid()
            || (!Enemy.IsValid() && Phase != EPhase::WaitForDefeat && Phase != EPhase::WaitForExit) || !Controller.IsValid()
            || InWorld->GetTimeSeconds() > Deadline)
        {
            Lines += FString::Printf(TEXT("Last phase %d.\n"), int32(Phase));
            Check(false, TEXT("The probe lost an actor or reached the deadline"));
            Finish();
            return;
        }

        const auto Cue = Enemy.IsValid() ? Enemy->GetAttackComponent()->GetAttackCue() : FGirlSaviorAttackCue{};
        switch (Phase)
        {
        case EPhase::Enter:
            if (Encounter->GetRunState() == EGirlSaviorRunState::Active)
            {
                Send(EKeys::W, IE_Released);
                Send(EKeys::A, IE_Released);
                Check(Controller->IsFireAllowed(), TEXT("Walking through the opening activates combat"));
                if (bDefeat)
                {
                    StartDuel();
                    break;
                }
                PlaceHero(FVector(-220, -900, 90));
                const bool bStarted = Movement().TryStartDash(FVector::ForwardVector);
                Check(bStarted && Movement().IsDashProtected(), TEXT("The live hero starts a protected dash"));
                const double Health = Receiver().GetHealth();
                const double Barrier = Receiver().GetBarrier();
                for (const auto Delivery : {EGirlSaviorDamageDelivery::DirectProjectile,
                    EGirlSaviorDamageDelivery::DirectMelee, EGirlSaviorDamageDelivery::DirectCharge})
                {
                    Check(Hit(Delivery, 10.f).Outcome == EGirlSaviorDamageOutcome::BlockedByDash
                        && Receiver().GetHealth() == Health && Receiver().GetBarrier() == Barrier,
                        TEXT("A direct hit preserves both vitals during dash protection"));
                }
                Check(Hit(EGirlSaviorDamageDelivery::Area, 3.f).HealthLost == 3.0
                    && Hit(EGirlSaviorDamageDelivery::FloorHazard, 2.f).HealthLost == 2.0,
                    TEXT("Area and floor damage bypass dash protection"));
                Movement().CancelDash();
                Check(Hit(EGirlSaviorDamageDelivery::DirectMelee, 1.f).HealthLost == 1.0,
                    TEXT("An interrupted dash rejects no subsequent direct hit"));
                Phase = EPhase::Rearm;
            }
            break;
        case EPhase::Rearm:
            if (Movement().GetDashCooldownRemaining() == 0.0)
            {
                Check(Movement().TryStartDash(FVector::ForwardVector), TEXT("Dash rearms on the original cooldown"));
                Phase = EPhase::Expiry;
            }
            break;
        case EPhase::Expiry:
            if (!Movement().IsDashProtected())
            {
                Check(Movement().IsDashing() && Hit(EGirlSaviorDamageDelivery::DirectMelee, 1.f).HealthLost == 1.0,
                    TEXT("Direct damage applies after protection expires during active movement"));
                Movement().CancelDash();
                StartDuel();
            }
            break;
        case EPhase::LockedWarning:
            if (Cue.Phase == EGirlSaviorEnemyAttackPhase::Preparation && Cue.bLocked)
            {
                Check(Cue.FloorPolygon.Num() == 18, TEXT("Pursuer displays the locked, sampled sector"));
                HealthBeforeSwing = Receiver().GetHealth();
                BeforeSidestep = Hero->GetActorLocation();
                Send(EKeys::D, IE_Pressed);
                Phase = EPhase::WaitForMiss;
            }
            break;
        case EPhase::WaitForMiss:
            if (Cue.Phase == EGirlSaviorEnemyAttackPhase::Recovery)
            {
                Send(EKeys::D, IE_Released);
                Check(Receiver().GetHealth() == HealthBeforeSwing
                    && FVector::Dist2D(BeforeSidestep, Hero->GetActorLocation()) > 50.0,
                    TEXT("Walking out after lock avoids the real swing"));
                FVector2D Pixel;
                Check(Controller->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), Pixel),
                    TEXT("The live target projects into the gameplay viewport"));
                Controller->SetMouseLocation(FMath::RoundToInt(Pixel.X), FMath::RoundToInt(Pixel.Y));
                Send(EKeys::LeftMouseButton, IE_Pressed);
                Phase = EPhase::Firing;
            }
            break;
        case EPhase::Firing:
            if (!Enemy->GetDamageReceiver()->IsAlive())
            {
                const auto& LethalShot = Enemy->GetDamageReceiver()->GetLastLethalResult().Spec;
                Check(LethalShot.SourceFaction == EGirlSaviorFaction::Hero
                    && Encounter->GetLiveEnemyCount() == InitialLiveEnemies - 1
                    && !Enemy->GetCapsuleComponent()->IsQueryCollisionEnabled(),
                    TEXT("Normal LMB shots kill the Pursuer, unregister it and clear body collision"));
                Check(LethalShot.WeaponId == TEXT("Pistol") && LethalShot.ShotGroupId > 0
                    && LethalShot.ShotGroupId == LethalShot.AttackId,
                    TEXT("The committed pistol damage retains its row and single-projectile discharge identifiers"));
                Send(EKeys::LeftMouseButton, IE_Released);
                const auto Range = EGirlSaviorProjectileCompletion::RangeExpired;
                const auto Impact = EGirlSaviorProjectileCompletion::Impact;
                CheckFlight(FVector(1000, -850, 90), FVector::RightVector, 60, Range, 1400,
                    TEXT("Clear flight expires at pistol range at 60 Hz"));
                CheckFlight(FVector(1000, -850, 90), FVector::RightVector, 15, Range, 1400,
                    TEXT("Clear flight expires at pistol range at 15 Hz"));
                CheckRangeEndpoint(0.0, Impact, TEXT("Contact at maximum range still impacts"));
                CheckRangeEndpoint(1.0, Range, TEXT("A blocker beyond range is not hit"));
                // The tall cover intersects the full sphere, including at capsule-center height.
                CheckFlight(FVector(-35, 360, 90), FVector::ForwardVector, 15, Impact, 64,
                    TEXT("A long sweep stops on the near face of the cover"));
                CheckFlight(FVector(-740, 0, 90), -FVector::ForwardVector, 15, Impact, 54,
                    TEXT("The open entrance still intercepts projectiles at the safe pocket"));
                PlaceHero(FVector(1050, 0, 90));
                Phase = EPhase::WaitForExit;
            }
            else
            {
                // Keep tracking the moving target through the normal cursor aim component.
                FVector2D Pixel;
                if (Controller->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), Pixel))
                {
                    Controller->SetMouseLocation(FMath::RoundToInt(Pixel.X), FMath::RoundToInt(Pixel.Y));
                }
            }
            break;
        case EPhase::WaitForExit:
            if (Encounter->IsOutcomeVisible())
            {
                Check(Encounter->GetRunState() == EGirlSaviorRunState::Succeeded
                    && !Controller->CanUseGameplayInput() && Encounter->GetLiveProjectileCount() == 0,
                    TEXT("The exit succeeds and closes input and projectiles"));
                Finish();
            }
            break;
        case EPhase::WaitForDefeat:
            if (Encounter->IsOutcomeVisible())
            {
                const auto& Lethal = Encounter->GetDefeatResult();
                Check(Encounter->GetRunState() == EGirlSaviorRunState::Defeated && Lethal.bKilled
                    && Lethal.Spec.SourceId == FName(TEXT("Enemy.Pursuer.Melee")) && Receiver().GetHealth() == 0.0,
                    TEXT("A live Pursuer swing commits defeat and the exact lethal cause"));
                Check(!Controller->CanUseGameplayInput() && Encounter->GetLiveEnemyCount() == 0
                    && Encounter->GetLiveProjectileCount() == 0 && !Enemy.IsValid()
                    && Hit(EGirlSaviorDamageDelivery::Area, 10.f).Outcome == EGirlSaviorDamageOutcome::Rejected,
                    TEXT("Defeat closes input, clears active threats and rejects later damage"));
                Finish(); // Leave the real outcome panel visible for a Retry click.
            }
            break;
        }
    }

    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AGirlSaviorCombatEncounter> Encounter;
    TWeakObjectPtr<AGirlSaviorCharacter> Hero;
    TWeakObjectPtr<AGirlSaviorPlayerController> Controller;
    TWeakObjectPtr<AGirlSaviorEnemyCharacter> Enemy;
    FDelegateHandle PostTick, Cleanup;
    EPhase Phase = EPhase::Enter;
    FString Lines;
    FVector InitialEnemyLocation = FVector::ZeroVector;
    FVector BeforeSidestep = FVector::ZeroVector;
    double Deadline = 0.0;
    double HealthBeforeSwing = 0.0;
    int32 Checks = 0;
    int32 InitialLiveEnemies = 0;
    int32 Failures = 0;
    bool bDefeat;
};

TUniquePtr<FFirstFightProbe> FirstFightProbe;
FAutoConsoleCommandWithWorldAndArgs FirstFightCommand(TEXT("GirlSavior.TestFirstFight"),
    TEXT("Verify CP1 in fresh focused CombatPrototype PIE. Optional argument Defeat checks terminal damage."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (World && World->WorldType == EWorldType::PIE)
        {
            FirstFightProbe.Reset();
            FirstFightProbe = MakeUnique<FFirstFightProbe>(*World, Args.Contains(TEXT("Defeat")));
        }
    }));
}

#endif
