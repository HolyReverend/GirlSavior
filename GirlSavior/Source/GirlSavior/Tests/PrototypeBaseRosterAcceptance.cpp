#if WITH_EDITOR

#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Enemies/GirlSaviorEnemyAttackComponent.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
using EPhase = EGirlSaviorEnemyAttackPhase;

// Use the saved roster and wait for real gameplay ticks through every selected attack.
class FBaseRosterProbe
{
public:
    FBaseRosterProbe(UWorld& InWorld, FString InCase) : World(&InWorld), Case(MoveTemp(InCase))
    {
        for (TActorIterator<AGirlSaviorCombatEncounter> It(&InWorld); It; ++It)
        {
            Encounter = *It;
            break;
        }
        Hero = Encounter.IsValid() ? Encounter->GetHero() : nullptr;
        if (!Check(Hero.IsValid() && Encounter->GetRunState() == EGirlSaviorRunState::Waiting
            && Encounter->GetLiveEnemyCount() == 5, TEXT("Fresh CombatPrototype with all five registered fixtures")))
        {
            Finish();
            return;
        }
        for (AGirlSaviorEnemyCharacter* Enemy : Encounter->PlacedEnemies)
        {
            const FString Label = Enemy->GetActorLabel();
            if (Label == TEXT("CP2_Shooter_ShooterEnemy")) Shooter = Enemy;
            if (Label == TEXT("CP2_Charger_ChargerEnemy")) Charger = Enemy;
            if (Label == TEXT("CP2_Mixed_ShooterEnemy")) MixedShooter = Enemy;
            if (Label == TEXT("CP2_Mixed_ChargerEnemy")) MixedCharger = Enemy;
        }
        for (TActorIterator<AActor> It(&InWorld); It; ++It)
        {
            if (It->GetActorLabel() == TEXT("CP2_Shooter_WalkAroundCover"))
            {
                Cover = *It;
                break;
            }
        }
        if (!Check(Shooter.IsValid() && Charger.IsValid() && MixedShooter.IsValid() && MixedCharger.IsValid()
            && Cover.IsValid(), TEXT("Saved role and cover fixtures are available")))
        {
            Finish();
            return;
        }
        Place(*Hero.Get(), FVector(-650, 0, 90));
        Deadline = InWorld.GetTimeSeconds() + 10.0;
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FBaseRosterProbe::Tick);
        CleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FBaseRosterProbe::OnCleanup);
    }

    ~FBaseRosterProbe() { Stop(); }

private:
    void Stop()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
        if (Shot.IsValid())
        {
            Shot->OnCompleted.Remove(ShotHandle);
        }
    }

    void OnCleanup(UWorld* InWorld, bool, bool)
    {
        if (World.Get() == InWorld)
        {
            Stop();
        }
    }

    bool Check(bool bPass, const TCHAR* Label)
    {
        ++Checks;
        Failures += !bPass;
        Log += FString::Printf(TEXT("%s %s\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Label);
        return bPass;
    }

    void Finish()
    {
        Stop();
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("CP2");
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Summary = FString::Printf(TEXT("checks=%d failures=%d"), Checks, Failures);
        FFileHelper::SaveStringToFile(Log + Summary + TEXT("\n"), *(Directory / (Case + TEXT(".txt"))));
        UE_LOG(LogTemp, Display, TEXT("CP2 %s: %s"), *Case, *Summary);
    }

    void Place(ACharacter& Character, const FVector& Position) const
    {
        auto* Movement = Character.GetCharacterMovement();
        Movement->StopMovementImmediately();
        Character.SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Walking);
        Character.GetCapsuleComponent()->UpdateOverlaps();
        if (&Character == Hero.Get())
        {
            if (auto* Controller = Cast<APlayerController>(Character.GetController()))
            {
                CastChecked<AGirlSaviorPlayerCameraManager>(Controller->PlayerCameraManager)->RequestFollowSnap();
            }
        }
    }

    void Kill(AGirlSaviorEnemyCharacter& Target) const
    {
        FGirlSaviorDamageSpec Damage;
        Damage.Delivery = EGirlSaviorDamageDelivery::DirectProjectile;
        Damage.Physical = 100.f;
        Damage.SourceFaction = EGirlSaviorFaction::Hero;
        Damage.SourceId = TEXT("CP2.PIE.Fixture");
        Damage.Origin = Hero->GetActorLocation();
        Target.GetDamageReceiver()->ResolveDamage(Damage);
    }

    void SetUp()
    {
        if (IsMixedCase())
        {
            Place(*MixedShooter.Get(), FVector(0, 7350, 90));
            Place(*MixedCharger.Get(), FVector(900, 7900, 90));
            Place(*Hero.Get(), FVector(0, 6750, 90));
            Primary = MixedShooter;
        }
        else if (Case == TEXT("ShooterCover") || Case == TEXT("ShooterInterrupt") || Case == TEXT("ShooterAngle"))
        {
            if (Case != TEXT("ShooterAngle"))
            {
                Cover->SetActorLocation(FVector(740, 2070, 95), false, nullptr, ETeleportType::TeleportPhysics);
            }
            Place(*Shooter.Get(), FVector(240, 2480, 90));
            Place(*Hero.Get(), FVector(240, 1920, 90));
            Primary = Shooter;
        }
        else
        {
            Place(*Charger.Get(), FVector(460, 4780, 90));
            Place(*Hero.Get(), FVector(460, 4360, 90));
            Primary = Charger;
        }
        InitialBody = Primary->GetActorLocation();
        HealthBefore = Hero->GetDamageReceiver()->GetHealth();
    }

    void Tick(UWorld* InWorld, ELevelTick, float DeltaSeconds)
    {
        if (World.Get() != InWorld)
        {
            return;
        }
        if (!Encounter.IsValid() || !Hero.IsValid() || InWorld->GetTimeSeconds() > Deadline)
        {
            Log += FString::Printf(TEXT("Step %d, body %s, hero %s.\n"), Step,
                *GetNameSafe(Primary.Get()), Hero.IsValid() ? *Hero->GetActorLocation().ToCompactString() : TEXT("none"));
            Check(false, TEXT("Encounter actors stayed available and the case reached its final step"));
            Finish();
            return;
        }
        if (Step == 0)
        {
            if (Encounter->GetRunState() == EGirlSaviorRunState::Active)
            {
                SetUp();
                ++Step;
            }
            return;
        }
        const auto Cue = Primary.IsValid() ? Primary->GetAttackComponent()->GetAttackCue() : FGirlSaviorAttackCue{};
        if (Step == 1)
        {
            if (Cue.Phase == EPhase::Preparation && Cue.bLocked)
            {
                AttackId = Cue.AttackId;
                Check(AttackId > 0 && Encounter->HasDangerousAttack()
                    && FVector::Dist2D(Cue.FloorStart, Cue.FloorEnd) > 100.0,
                    TEXT("Visible locked warning owns a dangerous slot and has useful length"));
                if (Case == TEXT("ShooterAngle"))
                {
                    Check(FVector::Dist2D(InitialBody, Primary->GetActorLocation()) > 60.0,
                        TEXT("Shooter found a different firing position around authored cover"));
                    Finish();
                    return;
                }
                if (Case.EndsWith(TEXT("Interrupt")))
                {
                    Kill(*Primary.Get());
                    Check(!Primary->GetDamageReceiver()->IsAlive() && !Encounter->HasDangerousAttack()
                        && Encounter->GetLiveProjectileCount() == 0
                        && Primary->GetAttackComponent()->GetAttackCue().AttackId == 0,
                        TEXT("Death during windup clears the attached warning and frees its slot"));
                    Finish();
                    return;
                }
                if (Case == TEXT("ShooterCover"))
                {
                    Cover->SetActorLocation(FVector(240, 2070, 95), false, nullptr, ETeleportType::TeleportPhysics);
                }
                if (Case == TEXT("ChargeAlly"))
                {
                    Place(*MixedShooter.Get(), FVector(460, 4540, 90));
                }
                if (Case == TEXT("ChargeWall") || Case == TEXT("ChargeAlly"))
                {
                    Check(Cue.HalfWidth == Charger->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                        TEXT("Charge stripe width agrees with the swept capsule"));
                }
                ++Step;
            }
            return;
        }
        if (Case == TEXT("ChargeWall"))
        {
            // Sweep an explicit walk-speed sidestep. A hidden test viewport has no gameplay input focus.
            const float WalkSpeed = Hero->GetCharacterMovement()->MaxWalkSpeed;
            if (Hero->GetActorLocation().X < 620.0)
            {
                Hero->AddActorWorldOffset(FVector(WalkSpeed * DeltaSeconds, 0, 0), true);
            }
        }
        if (Case == TEXT("ChargeActiveInterrupt") && Cue.Phase == EPhase::Active
            && FVector::Dist2D(InitialBody, Primary->GetActorLocation()) > 20.0)
        {
            Kill(*Primary.Get());
            InterruptedAt = Primary->GetActorLocation();
            InterruptedTime = InWorld->GetTimeSeconds();
            ++Step;
        }
        if (Case == TEXT("ChargeActiveInterrupt"))
        {
            if (Step == 3 && InWorld->GetTimeSeconds() >= InterruptedTime + 0.35)
            {
                Check(!Encounter->HasDangerousAttack() && !Primary->GetDamageReceiver()->IsAlive()
                    && Primary->GetActorLocation().Equals(InterruptedAt, 0.5)
                    && Hero->GetDamageReceiver()->GetHealth() == HealthBefore,
                    TEXT("Killing a moving Charger stops travel and damage immediately"));
                Finish();
            }
            return;
        }
        if (Case == TEXT("ChargeWall") || Case == TEXT("ChargeAlly") || Case == TEXT("ChargeHit"))
        {
            if (Cue.Phase == EPhase::Recovery)
            {
                const FVector End = Primary->GetActorLocation();
                Log += FString::Printf(TEXT("Charge stopped at %s. Hero at %s.\n"),
                    *End.ToCompactString(), *Hero->GetActorLocation().ToCompactString());
                const bool bExpectedStop = Case == TEXT("ChargeAlly")
                    ? End.Y > 4585 && End.Y < 4650 : End.Y > 4145 && End.Y < 4200;
                if (Case == TEXT("ChargeHit"))
                {
                    Check(End.Y > 4420 && End.Y < 4445 && !Encounter->HasDangerousAttack(),
                        TEXT("The hero stops the charge at first body contact"));
                    Check(Hero->GetDamageReceiver()->GetHealth() == HealthBefore - 18.0,
                        TEXT("A real charge contact applies 18 damage once"));
                }
                else
                {
                    Check(bExpectedStop && !Encounter->HasDangerousAttack(),
                        TEXT("Charge stopped at the first intended blocker and released its slot"));
                    Check(Hero->GetDamageReceiver()->GetHealth() == HealthBefore
                        && (Case == TEXT("ChargeAlly") || Hero->GetActorLocation().X > 540),
                        TEXT("Side movement or an allied body prevented hero damage"));
                }
                Finish();
            }
            return;
        }
        if (Step == 2)
        {
            for (TActorIterator<AGirlSaviorTestProjectile> It(InWorld); It; ++It)
            {
                if (It->GetAttackId() == AttackId)
                {
                    Shot = *It;
                    ShotHandle = Shot->OnCompleted.AddLambda([this](AGirlSaviorTestProjectile*, EGirlSaviorProjectileCompletion Reason)
                    {
                        ProjectileCompletion = Reason;
                    });
                    Check(Encounter->HasDangerousAttack() && Cue.Phase == EPhase::Recovery,
                        TEXT("The launched projectile keeps the slot while the Shooter recovers"));
                    if (IsMixedCase())
                    {
                        Place(*MixedCharger.Get(), FVector(270, 7200, 90));
                        const int64 Distractor = Encounter->AllocateAttackId();
                        Encounter->ReleaseDangerousAttack(AttackId, *Primary->GetAttackComponent());
                        Check(!Encounter->TryAcquireDangerousAttack(Distractor, *MixedCharger.Get()),
                            TEXT("A stale owner cannot release the projectile's slot"));
                        Kill(*Primary.Get());
                        Check(!Primary->GetDamageReceiver()->IsAlive() && Shot.IsValid() && Encounter->HasDangerousAttack(),
                            TEXT("Killing the Shooter after launch preserves the projectile and slot"));
                    }
                    ++Step;
                    break;
                }
            }
            return;
        }
        if (IsMixedCase() && !ProjectileCompletion.IsSet())
        {
            if (MixedCharger->GetAttackComponent()->GetAttackPhase() != EPhase::Approach)
            {
                Check(false, TEXT("Charger cannot prepare during detached projectile flight"));
                Finish();
            }
            return;
        }
        if (ProjectileCompletion.IsSet())
        {
            if (IsMixedCase() && MixedCharger->GetAttackComponent()->GetAttackPhase() != EPhase::Preparation)
            {
                return;
            }
            Check(ProjectileCompletion.GetValue() == EGirlSaviorProjectileCompletion::Impact,
                TEXT("The shot resolved one physical impact"));
            const double ExpectedDamage = Case == TEXT("MixedTuning") ? 17.0 : IsMixedCase() ? 12.0 : 0.0;
            Check(Hero->GetDamageReceiver()->GetHealth() == HealthBefore - ExpectedDamage,
                TEXT("Posthumous shot hit for authored damage or closed cover prevented damage"));
            Check(Encounter->HasDangerousAttack() == IsMixedCase(),
                TEXT("Projectile completion frees the slot for a newly eligible Charger"));
            Finish();
        }
    }

    bool IsMixedCase() const { return Case.StartsWith(TEXT("Mixed")); }

    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AGirlSaviorCombatEncounter> Encounter;
    TWeakObjectPtr<AGirlSaviorCharacter> Hero;
    TWeakObjectPtr<AGirlSaviorEnemyCharacter> Shooter, Charger, MixedShooter, MixedCharger, Primary;
    TWeakObjectPtr<AActor> Cover;
    TWeakObjectPtr<AGirlSaviorTestProjectile> Shot;
    TOptional<EGirlSaviorProjectileCompletion> ProjectileCompletion;
    FDelegateHandle TickHandle, CleanupHandle, ShotHandle;
    FString Case, Log;
    FVector InitialBody = FVector::ZeroVector;
    FVector InterruptedAt = FVector::ZeroVector;
    int64 AttackId = 0;
    double Deadline = 0.0, HealthBefore = 0.0;
    double InterruptedTime = 0.0;
    int32 Step = 0, Checks = 0, Failures = 0;
};

TUniquePtr<FBaseRosterProbe> BaseRosterProbe;
FAutoConsoleCommandWithWorldAndArgs BaseRosterCommand(TEXT("GirlSavior.TestBaseRoster"),
    TEXT("Run a fresh PIE CP2 case: Mixed, MixedTuning, ShooterCover, ShooterAngle, ShooterInterrupt, ChargeWall, ChargeAlly, ChargeHit, ChargerInterrupt, ChargeActiveInterrupt."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (World && World->WorldType == EWorldType::PIE && Args.Num() == 1
            && (Args[0] == TEXT("Mixed") || Args[0] == TEXT("MixedTuning") || Args[0] == TEXT("ShooterCover") || Args[0] == TEXT("ShooterAngle")
                || Args[0] == TEXT("ShooterInterrupt") || Args[0] == TEXT("ChargeWall") || Args[0] == TEXT("ChargeAlly")
                || Args[0] == TEXT("ChargerInterrupt") || Args[0] == TEXT("ChargeHit") || Args[0] == TEXT("ChargeActiveInterrupt")))
        {
            BaseRosterProbe.Reset();
            BaseRosterProbe = MakeUnique<FBaseRosterProbe>(*World, Args[0]);
        }
    }));
}

#endif
