#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/GirlSaviorFireSchedule.h"
#include "Misc/AutomationTest.h"
#include "Weapons/GirlSaviorAssaultRifle.h"
#include "Weapons/GirlSaviorShotgun.h"

// Use the production hooks so a failed preparation can be retried without spawning actors.
class FGirlSaviorWeaponCombatTestAccess
{
public:
    static FGirlSaviorPreparedDischarge Prepare(const UGirlSaviorFirearm& Firearm, double Now, FVector AimDirection)
    {
        FGirlSaviorWeaponFireContext Context;
        Context.AimDirection = AimDirection;
        Context.Now = Now;
        FGirlSaviorPreparedDischarge Discharge;
        Firearm.BuildShotDirections(Context, Discharge);
        return Discharge;
    }

    static void Commit(UGirlSaviorFirearm& Firearm, const FGirlSaviorPreparedDischarge& Discharge, double Now)
    {
        Firearm.OnShotCommitted(Discharge, Now);
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorShotgunFanTest, "GirlSavior.Weapons.Combat.ShotgunFan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorShotgunFanTest::RunTest(const FString& Parameters)
{
    auto* Shotgun = NewObject<UGirlSaviorShotgun>();
    FGirlSaviorWeaponRow Row;
    Row.ProjectileCount = 5;
    Row.PelletStepDegrees = 6.f;
    FText Error;
    TestTrue(TEXT("Shotgun accepts five evenly spaced pellets"), Shotgun->ValidateBehavior(Row, Error));
    Shotgun->Initialize(TEXT("Shotgun"), Row);

    const FVector Aim = FVector::RightVector;
    const auto Discharge = FGirlSaviorWeaponCombatTestAccess::Prepare(*Shotgun, 0.0, Aim);
    const double ExpectedOffsets[] = {-12.0, -6.0, 0.0, 6.0, 12.0};
    if (!TestEqual(TEXT("Five pellets share one centered fan"), Discharge.Directions.Num(), 5))
    {
        return false;
    }
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const FVector Expected = Aim.RotateAngleAxis(ExpectedOffsets[Index], FVector::UpVector);
        TestTrue(FString::Printf(TEXT("Pellet %d has the correct unit direction"), Index),
            Discharge.Directions[Index].Equals(Expected, 1.e-6));
    }
    Row.ProjectileCount = 4;
    TestFalse(TEXT("An even pellet count cannot have a center pellet"), Shotgun->ValidateBehavior(Row, Error));
    Row.ProjectileCount = 5;
    Row.PelletStepDegrees = 0.f;
    TestFalse(TEXT("A fan needs a positive step"), Shotgun->ValidateBehavior(Row, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorRifleSpreadTest, "GirlSavior.Weapons.Combat.RifleSpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorRifleSpreadTest::RunTest(const FString& Parameters)
{
    auto* Rifle = NewObject<UGirlSaviorAssaultRifle>();
    FGirlSaviorWeaponRow Row;
    Row.TightShotCount = 3;
    Row.TightSpreadDegrees = 1.f;
    Row.SustainedSpreadDegrees = 4.f;
    Row.SpreadResetSeconds = 0.30f;
    FText Error;
    TestTrue(TEXT("Rifle accepts the authored spread series"), Rifle->ValidateBehavior(Row, Error));

    constexpr int32 Seed = 912;
    Rifle->Initialize(TEXT("AssaultRifle"), Row, Seed);
    FRandomStream ExpectedStream(Seed);
    const double ShotTimes[] = {0.0, 0.125, 0.25, 0.375, 0.375 + double(Row.SpreadResetSeconds)};
    const float ExpectedSpreads[] = {1.f, 1.f, 1.f, 4.f, 1.f};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ShotTimes); ++Index)
    {
        const double Now = ShotTimes[Index];
        const auto Rejected = FGirlSaviorWeaponCombatTestAccess::Prepare(*Rifle, Now, FVector::ForwardVector);
        const auto Accepted = FGirlSaviorWeaponCombatTestAccess::Prepare(*Rifle, Now, FVector::ForwardVector);
        if (!TestEqual(TEXT("Rifle prepares one projectile"), Accepted.Directions.Num(), 1)
            || !TestEqual(TEXT("The rejected attempt also prepared one projectile"), Rejected.Directions.Num(), 1))
        {
            return false;
        }

        TestTrue(TEXT("An uncommitted attempt leaves the random sample and burst state available"),
            Accepted.Directions[0].Equals(Rejected.Directions[0], 1.e-6));
        const float Spread = ExpectedSpreads[Index];
        const FVector Expected = FVector::ForwardVector.RotateAngleAxis(
            ExpectedStream.FRandRange(-Spread, Spread), FVector::UpVector);
        TestTrue(FString::Printf(TEXT("Shot %d uses the expected spread and stream position"), Index + 1),
            Accepted.Directions[0].Equals(Expected, 1.e-6));
        FGirlSaviorWeaponCombatTestAccess::Commit(*Rifle, Accepted, Now);
    }

    Row.SustainedSpreadDegrees = 0.5f;
    TestFalse(TEXT("Sustained spread cannot be narrower than tight spread"), Rifle->ValidateBehavior(Row, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWeaponSwitchScheduleTest, "GirlSavior.Weapons.Combat.SwitchSchedule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWeaponSwitchScheduleTest::RunTest(const FString& Parameters)
{
    FGirlSaviorFireSchedule Schedule;
    Schedule.Press(0.0);
    TestTrue(TEXT("First shot can launch"), Schedule.ConsumeDue(0.0));
    Schedule.Launched(0.0, 4.0);

    Schedule.Cancel();
    Schedule.Press(0.125);
    TestFalse(TEXT("Repressing after a switch cannot bypass the committed deadline"), Schedule.ConsumeDue(0.125));
    TestTrue(TEXT("Fresh held press can fire at the original deadline"), Schedule.ConsumeDue(0.25));

    Schedule.Launched(0.25, 8.0);
    Schedule.Release();
    Schedule.Press(0.375);
    Schedule.Release();
    Schedule.Cancel();
    TestFalse(TEXT("Switch cancels a ready tap before the same-frame tick"), Schedule.ConsumeDue(0.375));
    return true;
}

#endif
