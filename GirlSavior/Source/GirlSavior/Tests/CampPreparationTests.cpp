#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Camp/GirlSaviorCampPreparation.h"
#include "Camp/GirlSaviorCampDefinition.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorCampDraftTest, "GirlSavior.Camp.Preparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Exercise the same native draft functions used by the expedition table.
bool FGirlSaviorCampDraftTest::RunTest(const FString& Parameters)
{
    const auto* Catalog = LoadObject<UGirlSaviorCampDefinition>(nullptr,
        TEXT("/Game/Data/DataAssets/Camp/DA_CampPrototype.DA_CampPrototype"));
    if (!TestNotNull(TEXT("Camp catalog loads"), Catalog))
    {
        return false;
    }

    FText Error;
    if (!TestTrue(TEXT("Both saved world references validate"), Catalog->Validate(Error)))
    {
        AddError(Error.ToString());
        return false;
    }

    const FName Slow(TEXT("StaticTrace"));
    const FName Rate(TEXT("MechanismOverdrive"));
    const FName Barrier(TEXT("TravelersBarrier"));
    FGirlSaviorCampPreparation Draft;
    Draft.Begin(*Catalog);
    TestEqual(TEXT("The only mission is preselected"), Draft.GetMissionId(), Catalog->MissionId);
    TestEqual(TEXT("A fresh draft is empty"), Draft.GetAbilityIds().Num(), 0);
    FGirlSaviorSupportOffer First;
    FGirlSaviorSupportOffer Second;
    TestFalse(TEXT("An empty draft cannot launch"), Draft.ResolveForLaunch(*Catalog, First, Second, Error));
    TestTrue(TEXT("An unknown card is refused"),
        Draft.ToggleAbility(*Catalog, TEXT("Unavailable")) == EGirlSaviorCampSelectionChange::Unavailable);
    TestTrue(TEXT("First card is selected"),
        Draft.ToggleAbility(*Catalog, Rate) == EGirlSaviorCampSelectionChange::Selected);
    TestFalse(TEXT("One card cannot launch"), Draft.ResolveForLaunch(*Catalog, First, Second, Error));
    Draft.ToggleAbility(*Catalog, Slow);
    TestTrue(TEXT("Two different cards can launch"), Draft.ResolveForLaunch(*Catalog, First, Second, Error));
    TestTrue(TEXT("A third card is rejected"),
        Draft.ToggleAbility(*Catalog, Barrier) == EGirlSaviorCampSelectionChange::Full);
    TestTrue(TEXT("Reselecting a picked card removes it"),
        Draft.ToggleAbility(*Catalog, Rate) == EGirlSaviorCampSelectionChange::Removed);
    Draft.ToggleAbility(*Catalog, Barrier);
    TestEqual(TEXT("Remaining card stays first"), Draft.GetAbilityIds()[0], Slow);
    TestEqual(TEXT("Replacement is appended"), Draft.GetAbilityIds()[1], Barrier);
    TestTrue(TEXT("Replacement creates a valid pair"), Draft.ResolveForLaunch(*Catalog, First, Second, Error));
    Draft.ToggleAbility(*Catalog, Slow);
    Draft.ToggleAbility(*Catalog, Rate);
    TestTrue(TEXT("The last distinct pair also validates"), Draft.ResolveForLaunch(*Catalog, First, Second, Error));

    const FName ValidSeed[] = {Barrier, Slow};
    Draft.Begin(*Catalog, ValidSeed);
    TestEqual(TEXT("A valid remembered pair seeds the draft"), Draft.GetAbilityIds().Num(), 2);
    const FName InvalidSeed[] = {Rate, Rate};
    Draft.Begin(*Catalog, InvalidSeed);
    TestTrue(TEXT("An invalid remembered pair is discarded atomically"), Draft.GetAbilityIds().IsEmpty());

    auto* ChangedCatalog = DuplicateObject(Catalog, GetTransientPackage());
    Draft.Begin(*ChangedCatalog, ValidSeed);
    ChangedCatalog->MissionId = TEXT("ChangedMission");
    TestFalse(TEXT("A mismatched mission cannot launch"),
        Draft.ResolveForLaunch(*ChangedCatalog, First, Second, Error));
    return true;
}

#endif
