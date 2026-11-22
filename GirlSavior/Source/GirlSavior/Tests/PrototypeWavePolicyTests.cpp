#if WITH_DEV_AUTOMATION_TESTS

#include "Encounters/GirlSaviorWavePolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWavePolicyTest, "GirlSavior.Combat.Waves.Policy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWavePolicyTest::RunTest(const FString& Parameters)
{
    using ERole = EGirlSaviorEnemyRole;
    using namespace GirlSaviorWavePolicy;
    const auto* Settings = NewObject<UGirlSaviorWaveSettings>();
    auto* Profile = NewObject<UGirlSaviorWaveProfile>();
    Profile->TargetCount = 6;
    Profile->RefillThreshold = 4;
    Profile->Interval = 5.f;

    const FGirlSaviorWavePopulation Live{4, 1, 1};
    const TArray Group = {ERole::Pursuer, ERole::Shooter};
    TestEqual(TEXT("At threshold, room for two"),
        GetBudget(*Settings, *Profile, Live.Total), 2);
    TestTrue(TEXT("A pending group counts once when committing"), CanAddGroup(*Settings, *Profile, Live, Group));
    TestFalse(TEXT("An extra live spawn invalidates the existing group"),
        CanAddGroup(*Settings, *Profile, {5, 1, 1}, Group));
    TestFalse(TEXT("A second Charger cannot reserve"), CanAddGroup(*Settings, *Profile, Live, {ERole::Charger}));
    TestFalse(TEXT("A third Shooter cannot reserve"), CanAddGroup(*Settings, *Profile, {4, 2, 0}, Group));
    const TArray Recipe = {ERole::Shooter, ERole::Charger, ERole::Pursuer};
    const auto Roles = SelectRepeatRoles(*Settings, Recipe, 2, Live);
    TestTrue(TEXT("A shortened group keeps a Pursuer and skips occupied roles"),
        Roles == TArray{ERole::Shooter, ERole::Pursuer});
    TestTrue(TEXT("One free place preserves the required Pursuer"),
        SelectRepeatRoles(*Settings, Recipe, 1, Live) == TArray{ERole::Pursuer});

    TestTrue(TEXT("First successful group is eligible without a timeline"),
        TimingAllows(*Settings, *Profile, {}, 0.1, false));
    TestFalse(TEXT("Empty frame cannot bypass the minimum gap"),
        TimingAllows(*Settings, *Profile, 20.0, 22.99, true));
    TestTrue(TEXT("Empty frame may accelerate beyond the minimum gap"),
        TimingAllows(*Settings, *Profile, 20.0, 23.0, true));
    TestFalse(TEXT("Visible or imminent enemy uses the ordinary interval"),
        TimingAllows(*Settings, *Profile, 20.0, 24.99, false));
    TestTrue(TEXT("Ordinary deadline admits the next check"),
        TimingAllows(*Settings, *Profile, 20.0, 25.0, false));

    Profile->TargetCount = 2;
    Profile->RefillThreshold = 1;
    Profile->Interval = 10.f;
    Profile->bAccelerateEmptyFrame = false;
    TestFalse(TEXT("Changing to Quiet invalidates the prior reservation"),
        CanAddGroup(*Settings, *Profile, Live, Group));
    TestFalse(TEXT("Quiet ignores an empty frame at the minimum gap"),
        TimingAllows(*Settings, *Profile, 20.0, 23.0, true));
    TestTrue(TEXT("Quiet uses its own ordinary deadline"), TimingAllows(*Settings, *Profile, 20.0, 30.0, true));
    return true;
}

#endif
