#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/GirlSaviorTestProjectile.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWeaponDamageFalloffTest, "GirlSavior.Weapons.Combat.DamageFalloff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Cover both authored falloff intervals and the non-falloff launch paths.
bool FGirlSaviorWeaponDamageFalloffTest::RunTest(const FString& Parameters)
{
    FGirlSaviorProjectileLaunch Launch;
    Launch.Damage.Physical = 6.f;
    Launch.FullDamageDistance = 300.f;
    Launch.MaxDistance = 800.f;
    TestEqual(TEXT("Legacy launches preserve damage at the endpoint"), Launch.GetDamageMultiplierAtDistance(800.0), 1.f);

    Launch.bApplyDamageFalloff = true;
    TestEqual(TEXT("A shotgun pellet still deals full damage at 300 cm"),
        Launch.Damage.Physical * Launch.GetDamageMultiplierAtDistance(300.0), 6.f);
    TestEqual(TEXT("A shotgun pellet deals 3 damage at 550 cm"),
        Launch.Damage.Physical * Launch.GetDamageMultiplierAtDistance(550.0), 3.f);
    TestEqual(TEXT("A shotgun pellet deals no damage at 800 cm"),
        Launch.GetDamageMultiplierAtDistance(800.0), 0.f);
    TestEqual(TEXT("Damage stays zero beyond range"), Launch.GetDamageMultiplierAtDistance(801.0), 0.f);

    Launch.Damage.Physical = 5.5f;
    Launch.FullDamageDistance = 700.f;
    Launch.MaxDistance = 1200.f;
    TestEqual(TEXT("A rifle round deals 2.75 damage at 950 cm"),
        Launch.Damage.Physical * Launch.GetDamageMultiplierAtDistance(950.0), 2.75f);

    Launch.FullDamageDistance = Launch.MaxDistance = 1400.f;
    TestEqual(TEXT("Pistol contact at the hard cutoff preserves full damage"),
        Launch.GetDamageMultiplierAtDistance(1400.0), 1.f);
    return true;
}

#endif
