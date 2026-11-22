#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/GirlSaviorTestProjectile.h"
#include "Enemies/GirlSaviorEnemyDefinition.h"
#include "Misc/AutomationTest.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorBaseRosterTuningTest, "GirlSavior.Combat.BaseRoster.Tuning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorBaseRosterTuningTest::RunTest(const FString& Parameters)
{
    auto* Definition = NewObject<UGirlSaviorEnemyDefinition>();
    Definition->Role = EGirlSaviorEnemyRole::Shooter;
    TestFalse(TEXT("Shooter requires a projectile class"), Definition->IsValidForSpawn());
    Definition->Shooter.ProjectileClass = AGirlSaviorTestProjectile::StaticClass();
    TestTrue(TEXT("Shooter admits complete usable tuning"), Definition->IsValidForSpawn());
    Definition->Shooter.ProjectileRadius = Definition->CapsuleRadius;
    TestFalse(TEXT("Shooter rejects a projectile that cannot leave its own capsule"), Definition->IsValidForSpawn());
    Definition->Shooter.ProjectileRadius = 12.f;
    Definition->Shooter.ProjectileSpeed = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Shooter rejects nonfinite speed"), Definition->IsValidForSpawn());
    Definition->Shooter.ProjectileSpeed = 1100.f;
    Definition->Shooter.PreferredDistance = Definition->Shooter.Range + 1.f;
    TestFalse(TEXT("Shooter rejects a preferred position outside its range"), Definition->IsValidForSpawn());

    Definition->Role = EGirlSaviorEnemyRole::Charger;
    TestTrue(TEXT("Only the active role's attack data is required"), Definition->IsValidForSpawn());
    Definition->Charger.MaxRange = Definition->Charger.MinRange - 1.f;
    TestFalse(TEXT("Charger rejects an inverted admission window"), Definition->IsValidForSpawn());
    Definition->Charger.MaxRange = 750.f;
    Definition->Charger.MaxTravel = 200.f;
    TestFalse(TEXT("Charger rejects unreachable admission distance"), Definition->IsValidForSpawn());
    Definition->Charger.MaxTravel = 750.f;
    Definition->Charger.LockedTail = Definition->Charger.Preparation;
    TestFalse(TEXT("Charger requires an unlocked portion of preparation"), Definition->IsValidForSpawn());
    return true;
}

#endif
