#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/GirlSaviorTestProjectile.h"
#include "Engine/World.h"
#include "Feedback/GirlSaviorCombatFeedbackPresenter.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Weapons/GirlSaviorFirearm.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWeaponFeedbackTest, "GirlSavior.Weapons.Feedback.GroupedCues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Count actual presenter effects without depending on map art, sound, or camera setup.
bool FGirlSaviorWeaponFeedbackTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Feedback test world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    auto* Presenter = World->SpawnActor<AGirlSaviorCombatFeedbackPresenter>();
    auto* First = World->SpawnActor<AActor>();
    auto* Second = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Presenter"), Presenter) || !TestNotNull(TEXT("First contact identity"), First)
        || !TestNotNull(TEXT("Second contact identity"), Second))
    {
        return false;
    }

    using ECue = EGirlSaviorFeedbackCue;
    constexpr int64 Group = 12;
    FGirlSaviorWeaponShotCue Shot;
    Shot.WeaponId = TEXT("Shotgun");
    Shot.ShotGroupId = Group;
    Shot.AimDirection = FVector::ForwardVector;
    FGirlSaviorProjectileImpact Impact;
    Impact.WeaponId = Shot.WeaponId;
    Impact.ShotGroupId = Group;
    Impact.BlockingActor = First;
    Impact.SurfaceType = SurfaceType1;
    for (int32 Pellet = 0; Pellet < 5; ++Pellet)
    {
        Presenter->RegisterShotGroup(Group, 1.5);
        Presenter->HandleProjectileImpact(Impact);
    }
    Presenter->HandleShot(Shot);
    TestEqual(TEXT("A volley emits one muzzle cue"), Presenter->GetCueCount(ECue::Muzzle), 1);
    TestEqual(TEXT("Five contacts on the same blocker and surface share one cue"), Presenter->GetCueCount(ECue::Geometry), 1);
    Impact.SurfaceType = SurfaceType2;
    Presenter->HandleProjectileImpact(Impact);
    Impact.BlockingActor = Second;
    Presenter->HandleProjectileImpact(Impact);
    TestEqual(TEXT("A different surface and blocker each emit a cue"), Presenter->GetCueCount(ECue::Geometry), 3);
    Impact.BlockingActor.Reset();
    Impact.SurfaceType = SurfaceType_Default;
    Impact.GeometryKeyPoint = FVector(4, 0, 0);
    Presenter->HandleProjectileImpact(Impact);
    Impact.GeometryKeyPoint = FVector(8, 0, 0);
    Presenter->HandleProjectileImpact(Impact);
    Impact.GeometryKeyPoint = FVector(32, 0, 0);
    Presenter->HandleProjectileImpact(Impact);
    TestEqual(TEXT("Synthetic geometry groups by contact cell"), Presenter->GetCueCount(ECue::Geometry), 5);
    Impact.bWardContact = true;
    Presenter->HandleProjectileImpact(Impact);
    Presenter->HandleProjectileImpact(Impact);
    TestEqual(TEXT("Ward contact has its own grouped cue"), Presenter->GetCueCount(ECue::Ward), 1);

    FGirlSaviorDamageResult Damage;
    Damage.Outcome = EGirlSaviorDamageOutcome::Applied;
    Damage.Spec.SourceFaction = EGirlSaviorFaction::Hero;
    Damage.Spec.WeaponId = Shot.WeaponId;
    Damage.Spec.ShotGroupId = Group;
    Damage.HealthLost = 1.0;
    for (int32 Pellet = 0; Pellet < 5; ++Pellet)
    {
        Presenter->HandleDamage(*First, Damage);
    }
    Presenter->HandleDamage(*Second, Damage);
    TestEqual(TEXT("Each target plays one health cue"), Presenter->GetCueCount(ECue::HealthHit), 2);
    Damage.bKilled = true;
    Presenter->HandleDamage(*First, Damage);
    TestEqual(TEXT("A later lethal cue survives the previous health cue"), Presenter->GetCueCount(ECue::EnemyDeath), 1);
    Damage.bKilled = false;
    Damage.HealthLost = 0.0;
    Damage.BarrierLost = 1.0;
    Presenter->HandleDamage(*First, Damage);
    Presenter->HandleDamage(*First, Damage);
    TestEqual(TEXT("Barrier-only contacts play one barrier cue"), Presenter->GetCueCount(ECue::BarrierSustain), 1);
    TestEqual(TEXT("Barrier-only contacts emit no additional health cue"), Presenter->GetCueCount(ECue::HealthHit), 2);
    Damage.bBarrierBroken = true;
    Presenter->HandleDamage(*First, Damage);
    TestEqual(TEXT("Breaking an already hit barrier in the same volley is not a second reaction"),
        Presenter->GetCueCount(ECue::BarrierBreak), 0);
    Damage.BarrierLost = 1.0;
    Damage.HealthLost = 1.0;
    Damage.bBarrierBroken = true;
    auto* Third = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Mixed damage contact identity"), Third))
    {
        return false;
    }
    Presenter->HandleDamage(*Third, Damage);
    TestEqual(TEXT("A single pellet may break barrier and deal health damage"), Presenter->GetCueCount(ECue::BarrierBreak), 1);
    TestEqual(TEXT("Mixed damage presents its health loss too"), Presenter->GetCueCount(ECue::HealthHit), 3);

    Presenter->PruneShotGroups(1.0);
    TestTrue(TEXT("Stopping pellets does not age out their contact group"), Presenter->ShotGroups.Contains(Group));
    Presenter->PruneShotGroups(2.0);
    TestFalse(TEXT("Shot groups expire after the projectile safety lifetime"), Presenter->ShotGroups.Contains(Group));
    return true;
}

#endif
