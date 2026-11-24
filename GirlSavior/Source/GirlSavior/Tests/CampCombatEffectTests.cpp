#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorCombatPistolSettings.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorVitalsSettings.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Enemies/GirlSaviorEnemyDefinition.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Support/GirlSaviorSupportAbilityDefinition.h"
#include "Support/GirlSaviorSupportRun.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorCampSupportMechanicsTest, "GirlSavior.Camp.CombatMechanics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Verify each authored pair through the runtime factory and the real component seams.
bool FGirlSaviorCampSupportMechanicsTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Support test world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    const auto LoadAbility = [](const TCHAR* Name)
    {
        return LoadObject<UGirlSaviorSupportAbilityDefinition>(nullptr,
            *FString::Printf(TEXT("/Game/Data/DataAssets/Camp/%s.%s"), Name, Name));
    };
    const auto* Liora = LoadAbility(TEXT("DA_Support_StaticTrace"));
    const auto* Ida = LoadAbility(TEXT("DA_Support_MechanismOverdrive"));
    const auto* Saira = LoadAbility(TEXT("DA_Support_TravelersBarrier"));
    if (!TestNotNull(TEXT("Liora definition"), Liora) || !TestNotNull(TEXT("Ida definition"), Ida)
        || !TestNotNull(TEXT("Saira definition"), Saira))
    {
        return false;
    }

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = World->SpawnActor<AGirlSaviorEnemyCharacter>(FVector(200, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Support target"), Enemy))
    {
        return false;
    }
    Enemy->Definition = NewObject<UGirlSaviorEnemyDefinition>(Enemy);
    Enemy->ClearSupportWalkSpeedMultiplier();
    Enemy->GetDamageReceiver()->InitializeVitals(FGirlSaviorVitals{}, EGirlSaviorFaction::Enemy);
    const float BaseWalk = Enemy->Definition->WalkSpeed;

    const UGirlSaviorSupportAbilityDefinition* Pairs[][2] = {{Liora, Ida}, {Liora, Saira}, {Ida, Saira}};
    for (const auto& Pair : Pairs)
    {
        auto* Hero = World->SpawnActor<AGirlSaviorCharacter>(FVector(0, 0, 100), FRotator::ZeroRotator, Spawn);
        if (!TestNotNull(TEXT("Fresh support hero"), Hero))
        {
            return false;
        }
        auto* Fire = Hero->GetFireComponent();
        Fire->CombatPistol = NewObject<UGirlSaviorCombatPistolSettings>(Hero);
        auto* Receiver = Hero->GetDamageReceiver();
        FGirlSaviorVitals Vitals;
        Vitals.InitialBarrier = Pair[0] == Liora ? 0.f : 4.f;
        TestTrue(TEXT("Initialize fresh hero with prototype vitals"), Receiver->InitializeVitals(Vitals, EGirlSaviorFaction::Hero));
        auto* Run = NewObject<UGirlSaviorSupportRun>();
        FText Error;
        if (!TestTrue(TEXT("Configure and attach the authored pair"), Run->Configure(Pair, Error) && Run->Attach(*Hero, Error)))
        {
            AddError(Error.ToString());
            return false;
        }
        ON_SCOPE_EXIT { Run->Detach(); };

        const bool bLiora = Pair[0] == Liora;
        const bool bIda = Pair[0] == Ida || Pair[1] == Ida;
        const bool bSaira = Pair[1] == Saira;
        const double InitialTotal = Vitals.InitialBarrier + (bSaira ? 8.0 : 0.0);
        TestEqual(TEXT("Ida modifies only the selected pair's cadence"), Fire->GetEffectiveShotsPerSecond(),
            bIda ? 4.24f : 4.f);
        TestTrue(TEXT("Saira adds only the selected pair's barrier capacity"),
            FMath::IsNearlyEqual(Receiver->GetBarrier(), InitialTotal, 1.e-5)
            && FMath::IsNearlyEqual(Receiver->GetMaxBarrier(), InitialTotal, 1.e-5));
        TestFalse(TEXT("A nonfinite effective firing rate is rejected"),
            Fire->SetSupportRateMultiplier(std::numeric_limits<float>::max()));

        FGirlSaviorDamageResult PistolHit;
        PistolHit.Spec.SourceFaction = EGirlSaviorFaction::Hero;
        PistolHit.Spec.SourceId = TEXT("Hero.Pistol");
        PistolHit.Outcome = EGirlSaviorDamageOutcome::Applied;
        Run->NotifyHeroWeaponDamage(*Enemy, PistolHit);
        TestEqual(TEXT("A zero-loss result leaves the enemy speed alone"), Enemy->GetCharacterMovement()->MaxWalkSpeed, BaseWalk);
        PistolHit.HealthLost = 1.0;
        PistolHit.bKilled = true;
        Run->NotifyHeroWeaponDamage(*Enemy, PistolHit);
        TestEqual(TEXT("A lethal result leaves the enemy speed alone"), Enemy->GetCharacterMovement()->MaxWalkSpeed, BaseWalk);
        PistolHit.bKilled = false;
        Run->NotifyHeroWeaponDamage(*Enemy, PistolHit);
        Run->NotifyHeroWeaponDamage(*Enemy, PistolHit);
        TestEqual(TEXT("Liora applies the selected pair's slow from base on repeated hits"),
            Enemy->GetCharacterMovement()->MaxWalkSpeed, bLiora ? BaseWalk * 0.9f : BaseWalk);
        Run->ForgetEnemy(*Enemy);
        TestEqual(TEXT("Retirement clears the named movement modifier"), Enemy->GetCharacterMovement()->MaxWalkSpeed, BaseWalk);

        FGirlSaviorDamageSpec Attack;
        Attack.SourceFaction = EGirlSaviorFaction::Enemy;
        Attack.Physical = 3.f;
        const auto Hit = Receiver->ResolveDamage(Attack);
        if (bSaira)
        {
            TestTrue(TEXT("The initial hit is absorbed without spending health"),
                Hit.HealthLost == 0.0 && FMath::IsNearlyEqual(Hit.BarrierLost, 3.0, 1.e-5));
            Receiver->RefillSupportBarrier();
            TestTrue(TEXT("A refill restores the support charge"), FMath::IsNearlyEqual(Receiver->GetBarrier(), InitialTotal, 1.e-5));
            Run->CloseDispatch();
            Run->NotifyHeroWeaponDamage(*Enemy, PistolHit);
            TestEqual(TEXT("A terminal run cannot slow a new victim"), Enemy->GetCharacterMovement()->MaxWalkSpeed, BaseWalk);
        }
        Run->Detach();
        TestEqual(TEXT("Detach restores the baseline rate"), Fire->GetEffectiveShotsPerSecond(), 4.f);
        TestEqual(TEXT("Support absorbs before the baseline barrier"), Receiver->GetBarrier(), double(Vitals.InitialBarrier));

        if (bSaira)
        {
            if (!TestTrue(TEXT("Rebind the same pair before the lethal barrier fixture"), Run->Attach(*Hero, Error)))
            {
                return false;
            }
            bool bSawCommittedPresentation = false;
            Receiver->OnDied.AddLambda([Run](const FGirlSaviorDamageResult&)
            {
                Run->CloseDispatch();
            });
            Receiver->OnDamagePresentation.AddLambda([&](AActor&, const FGirlSaviorDamageResult& Result)
            {
                Run->Detach();
                bSawCommittedPresentation = !Run->IsDispatchOpen() && Result.bKilled
                    && FMath::IsNearlyEqual(Result.BarrierBefore, InitialTotal, 1.e-5)
                    && Result.BarrierAfter == 0.0 && Result.HealthAfter == 0.0
                    && FMath::IsNearlyEqual(Receiver->GetMaxBarrier(), InitialTotal, 1.e-5);
            });
            Attack.Physical = 1000.f;
            Receiver->ResolveDamage(Attack);
            Receiver->OnDamagePresentation.Clear();
            TestTrue(TEXT("Lethal presentation sees the combined pool before deferred detach"), bSawCommittedPresentation);
            TestTrue(TEXT("Detach requested inside lethal publication clears the support pool after it publishes"),
                FMath::IsNearlyEqual(Receiver->GetMaxBarrier(), double(Vitals.InitialBarrier)) && Receiver->GetBarrier() == 0.0);
        }
    }

    return true;
}

#endif
