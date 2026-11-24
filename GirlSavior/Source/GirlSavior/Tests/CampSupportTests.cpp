#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Camp/GirlSaviorCampDefinition.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Support/GirlSaviorHeroineDefinition.h"
#include "Support/GirlSaviorSupportAbilityDefinition.h"
#include "Support/GirlSaviorSupportEffectDefinition.h"
#include "Support/GirlSaviorSupportRun.h"
#include "Templates/UnrealTemplate.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorCampSchemaTest, "GirlSavior.Camp.Schema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorCampFixtureTest, "GirlSavior.Camp.SavedFixture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorCampRejectionTest, "GirlSavior.Camp.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace GirlSavior::CampTests
{
// Load the packaged fixture through the same references the table will use.
UGirlSaviorCampDefinition* LoadCampFixture()
{
    return LoadObject<UGirlSaviorCampDefinition>(nullptr,
        TEXT("/Game/Data/DataAssets/Camp/DA_CampPrototype.DA_CampPrototype"));
}
}

// The Editor needs these reflected classes before it can load or author Camp assets.
bool FGirlSaviorCampSchemaTest::RunTest(const FString& Parameters)
{
    for (const TCHAR* ClassName : {
        TEXT("GirlSaviorCampDefinition"),
        TEXT("GirlSaviorHeroineDefinition"),
        TEXT("GirlSaviorSupportAbilityDefinition"),
        TEXT("GirlSaviorWeaponHitSlowEffectDefinition"),
        TEXT("GirlSaviorWeaponRateEffectDefinition"),
        TEXT("GirlSaviorRechargeBarrierEffectDefinition") })
    {
        const FString ClassPath = FString(TEXT("/Script/GirlSavior.")) + ClassName;
        TestNotNull(ClassPath, FindObject<UClass>(nullptr, *ClassPath));
    }
    return true;
}

// Read all authored values from disk, including the ownership and inline object references.
bool FGirlSaviorCampFixtureTest::RunTest(const FString& Parameters)
{
    using GirlSavior::CampTests::LoadCampFixture;
    const UGirlSaviorCampDefinition* Catalog = LoadCampFixture();
    if (!TestNotNull(TEXT("Saved catalog loads from its package"), Catalog))
    {
        return false;
    }

    FText Error;
    if (!TestTrue(TEXT("Camp content validates"), Catalog->ValidateCatalog(Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TestEqual(TEXT("Saved mission ID"), Catalog->MissionId, FName(TEXT("CombatPrototypeRoute")));
    TestEqual(TEXT("Saved Russian mission name"), Catalog->MissionName.ToString(), FString(TEXT("Боевой маршрут")));

    const FName HeroineIds[] = {TEXT("Liora"), TEXT("Ida"), TEXT("Saira")};
    const FName AbilityIds[] = {TEXT("StaticTrace"), TEXT("MechanismOverdrive"), TEXT("TravelersBarrier")};
    const UGirlSaviorSupportEffectDefinition* Effects[3] = {};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(AbilityIds); ++Index)
    {
        TestEqual(TEXT("Offer order"), Catalog->AvailableAbilityIds[Index], AbilityIds[Index]);
        const FGirlSaviorSupportOffer Offer = Catalog->FindOffer(AbilityIds[Index]);
        if (!TestNotNull(TEXT("Offer resolves an owner"), Offer.Heroine)
            || !TestNotNull(TEXT("Offer resolves a skill"), Offer.Ability))
        {
            return false;
        }

        TestEqual(TEXT("Heroine ID"), Offer.Heroine->HeroineId, HeroineIds[Index]);
        const FString HeroineName = FString::Printf(TEXT("DA_Heroine_%s"), *HeroineIds[Index].ToString());
        const FString AbilityName = FString::Printf(TEXT("DA_Support_%s"), *AbilityIds[Index].ToString());
        const FString IconName = FString::Printf(TEXT("T_Support_%s"), *AbilityIds[Index].ToString());
        TestEqual(TEXT("Heroine asset name"), Offer.Heroine->GetName(), HeroineName);
        TestEqual(TEXT("Ability asset name"), Offer.Ability->GetName(), AbilityName);
        TestEqual(TEXT("Icon texture asset name"), Offer.Ability->Icon->GetName(), IconName);
        TestEqual(TEXT("Heroine has one saved skill"), Offer.Heroine->Abilities.Num(), 1);
        TestTrue(TEXT("Heroine's saved reference is the offered skill"), Offer.Heroine->Abilities.Contains(Offer.Ability));
        if (!TestEqual(TEXT("Card has one saved inline mechanic"), Offer.Ability->Effects.Num(), 1))
        {
            return false;
        }
        Effects[Index] = Offer.Ability->Effects[0];
        TestTrue(TEXT("Mechanic is outered to the saved skill"), Effects[Index]->GetOuter() == Offer.Ability);

        FDataValidationContext HeroineContext;
        FDataValidationContext AbilityContext;
        TestTrue(TEXT("Editor accepts heroine"), Offer.Heroine->IsDataValid(HeroineContext) == EDataValidationResult::Valid);
        TestTrue(TEXT("Editor accepts ability"), Offer.Ability->IsDataValid(AbilityContext) == EDataValidationResult::Valid);
    }

    const auto* Slow = Cast<UGirlSaviorWeaponHitSlowEffectDefinition>(Effects[0]);
    const auto* Rate = Cast<UGirlSaviorWeaponRateEffectDefinition>(Effects[1]);
    const auto* Barrier = Cast<UGirlSaviorRechargeBarrierEffectDefinition>(Effects[2]);
    if (!TestNotNull(TEXT("Liora's typed slow"), Slow) || !TestNotNull(TEXT("Ida's typed rate"), Rate)
        || !TestNotNull(TEXT("Saira's typed barrier"), Barrier))
    {
        return false;
    }
    TestEqual(TEXT("Slow fraction"), Slow->SlowFraction, 0.10f);
    TestEqual(TEXT("Slow duration"), Slow->DurationSeconds, 1.5f);
    TestEqual(TEXT("Rate multiplier"), Rate->RateMultiplier, 1.06f);
    TestEqual(TEXT("Barrier capacity"), Barrier->HealthFraction, 0.08f);
    TestEqual(TEXT("Barrier recharge delay"), Barrier->RechargeDelaySeconds, 6.f);

    // Duplicate the freshly loaded skill to simulate a Details edit without touching the saved baseline.
    const auto* SavedAbility = Catalog->FindOffer(AbilityIds[0]).Ability;
    auto* EditedAbility = DuplicateObject(SavedAbility, GetTransientPackage());
    if (!TestEqual(TEXT("Duplicated ability has one inline effect"), EditedAbility->Effects.Num(), 1))
    {
        return false;
    }
    auto* EditedSlow = Cast<UGirlSaviorWeaponHitSlowEffectDefinition>(EditedAbility->Effects[0]);
    if (!TestNotNull(TEXT("Duplicated ability keeps its typed effect"), EditedSlow))
    {
        return false;
    }
    const FText BaselineDetail = SavedAbility->BuildDetailsText();
    auto* Run = NewObject<UGirlSaviorSupportRun>();
    const auto* OriginalRuntime = Cast<UGirlSaviorWeaponHitSlowRuntime>(EditedSlow->CreateRuntime(*Run, Error));
    EditedSlow->SlowFraction = 0.25f;
    const auto* EditedRuntime = Cast<UGirlSaviorWeaponHitSlowRuntime>(EditedSlow->CreateRuntime(*Run, Error));
    const FText EditedDetail = EditedAbility->BuildDetailsText();
    TestTrue(TEXT("Card detail changes without a Blueprint edit"), !EditedDetail.EqualTo(BaselineDetail));
    TestTrue(TEXT("Card displays the edited percentage"),
        EditedDetail.ToString().Contains(FText::AsPercent(0.25f).ToString()));
    TestTrue(TEXT("The saved baseline still renders the original detail"), BaselineDetail.EqualTo(SavedAbility->BuildDetailsText()));
    if (!TestNotNull(TEXT("Original runtime snapshot"), OriginalRuntime)
        || !TestNotNull(TEXT("Edited runtime snapshot"), EditedRuntime))
    {
        return false;
    }
    TestEqual(TEXT("Existing runtime retains original tuning"), OriginalRuntime->SlowFraction, 0.10f);
    TestEqual(TEXT("New runtime uses edited tuning"), EditedRuntime->SlowFraction, 0.25f);
    TestTrue(TEXT("Runtime belongs to the support run"), EditedRuntime->GetOuter() == Run);
    return true;
}

// Test early data rejection and the launch validator with real map packages on a transient catalog.
bool FGirlSaviorCampRejectionTest::RunTest(const FString& Parameters)
{
    using GirlSavior::CampTests::LoadCampFixture;
    const auto* SavedCatalog = LoadCampFixture();
    if (!TestNotNull(TEXT("Saved Camp fixture"), SavedCatalog))
    {
        return false;
    }

    FText Error;
    if (!TestTrue(TEXT("Baseline fixture validates before mutation"), SavedCatalog->ValidateCatalog(Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    auto* Catalog = DuplicateObject(SavedCatalog, GetTransientPackage());
    const auto OriginalRoster = Catalog->Roster;
    const auto OriginalOffers = Catalog->AvailableAbilityIds;
    const auto* SlowAbility = Catalog->FindOffer(TEXT("StaticTrace")).Ability;
    const auto* RateAbility = Catalog->FindOffer(TEXT("MechanismOverdrive")).Ability;
    if (!TestNotNull(TEXT("Slow offer"), SlowAbility) || !TestNotNull(TEXT("Rate offer"), RateAbility))
    {
        return false;
    }

    const auto* RateEffect = Cast<UGirlSaviorWeaponRateEffectDefinition>(RateAbility->Effects[0]);
    auto* EditedAbility = DuplicateObject(SlowAbility, GetTransientPackage());
    if (!TestTrue(TEXT("Duplicated slow ability keeps its effect list"), !EditedAbility->Effects.IsEmpty()))
    {
        return false;
    }
    auto* Slow = Cast<UGirlSaviorWeaponHitSlowEffectDefinition>(EditedAbility->Effects[0]);
    if (!TestNotNull(TEXT("Slow effect"), Slow) || !TestNotNull(TEXT("Rate effect"), RateEffect))
    {
        return false;
    }

    {
        TGuardValue<TObjectPtr<UGirlSaviorHeroineDefinition>> NullHeroine(Catalog->Roster[0], nullptr);
        TestFalse(TEXT("Reject a null roster entry"), Catalog->ValidateCatalog(Error));
    }
    Catalog->Roster[1] = Catalog->Roster[0];
    TestFalse(TEXT("Reject a repeated heroine ID"), Catalog->ValidateCatalog(Error));
    Catalog->Roster = OriginalRoster;

    Catalog->AvailableAbilityIds[1] = Catalog->AvailableAbilityIds[0];
    TestFalse(TEXT("Reject a repeated offer"), Catalog->ValidateCatalog(Error));
    Catalog->AvailableAbilityIds[1] = TEXT("Unowned");
    TestFalse(TEXT("Reject an unowned offer"), Catalog->ValidateCatalog(Error));
    Catalog->AvailableAbilityIds = OriginalOffers;

    auto* Heroine = DuplicateObject(Catalog->Roster[1].Get(), GetTransientPackage());
    Catalog->Roster[1] = Heroine;
    Heroine->Abilities.Add(DuplicateObject(SlowAbility, Heroine));
    TestFalse(TEXT("Reject an ability ID owned by multiple heroines"), Catalog->ValidateCatalog(Error));
    Heroine->Abilities.Pop();
    Heroine->Abilities.Add(nullptr);
    TestFalse(TEXT("Reject a null owned ability"), Catalog->ValidateCatalog(Error));
    Catalog->Roster = OriginalRoster;

    auto* Rate = DuplicateObject(RateEffect, GetTransientPackage());
    auto* Barrier = NewObject<UGirlSaviorRechargeBarrierEffectDefinition>();
    {
        TGuardValue<FName> NoId(EditedAbility->AbilityId, NAME_None);
        TestFalse(TEXT("Reject an empty ability ID"), EditedAbility->Validate(Error));
    }
    {
        TGuardValue<FText> BlankText(EditedAbility->DisplayName, FText::FromString(TEXT("  ")));
        TestFalse(TEXT("Reject whitespace display text"), EditedAbility->Validate(Error));
    }
    {
        TGuardValue<TObjectPtr<UTexture2D>> NoIcon(EditedAbility->Icon, nullptr);
        TestFalse(TEXT("Reject a missing offered icon"), EditedAbility->Validate(Error));
    }
    EditedAbility->Effects.Reset();
    TestFalse(TEXT("Reject an empty effect list"), EditedAbility->Validate(Error));
    EditedAbility->Effects.Add(nullptr);
    TestFalse(TEXT("Reject a null effect"), EditedAbility->Validate(Error));
    EditedAbility->Effects[0] = Slow;
    EditedAbility->Effects.Add(NewObject<UGirlSaviorWeaponHitSlowEffectDefinition>(EditedAbility));
    TestFalse(TEXT("Reject repeated mechanic keys within one card"), EditedAbility->Validate(Error));
    EditedAbility->Effects.Pop();

    EditedAbility->AbilityId = TEXT("OtherSlow");
    TestFalse(TEXT("Reject repeated mechanic keys across cards"),
        UGirlSaviorSupportAbilityDefinition::ValidatePair(*SlowAbility, *EditedAbility, Error));
    TestTrue(TEXT("Cross-card failure names the repeated mechanic"), Error.ToString().Contains(TEXT("WeaponHitSlow")));

    Slow->SlowFraction = 1.f;
    TestFalse(TEXT("Reject a full stop from slow tuning"), Slow->Validate(Error));
    Slow->SlowFraction = 0.1f;
    Slow->DurationSeconds = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Reject a nonfinite slow duration"), Slow->Validate(Error));
    TestNull(TEXT("Invalid tuning never creates a runtime"), Slow->CreateRuntime(*NewObject<UGirlSaviorSupportRun>(), Error));
    Rate->RateMultiplier = 0.99f;
    TestFalse(TEXT("Reject rate below one"), Rate->Validate(Error));
    Rate->RateMultiplier = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Reject nonfinite rate"), Rate->Validate(Error));
    Rate->RateMultiplier = 1.f;
    TestTrue(TEXT("Allow neutral rate"), Rate->Validate(Error));
    Barrier->HealthFraction = 1.f;
    Barrier->RechargeDelaySeconds = 0.f;
    TestFalse(TEXT("Reject a zero recharge delay"), Barrier->Validate(Error));
    Barrier->RechargeDelaySeconds = 6.f;
    TestTrue(TEXT("Allow barrier equal to maximum health"), Barrier->Validate(Error));
    Barrier->HealthFraction = 1.001f;
    TestFalse(TEXT("Reject barrier above maximum health"), Barrier->Validate(Error));

    Catalog->CampMap.Reset();
    Catalog->ExpeditionMap.Reset();
    TestFalse(TEXT("Full validation requires assigned maps"), Catalog->Validate(Error));
    Catalog->ExpeditionMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/TestMaps/CombatPrototypeRoute.CombatPrototypeRoute")));
    Catalog->CampMap = Catalog->ExpeditionMap;
    TestFalse(TEXT("Reject two references to the same map"), Catalog->Validate(Error));
    Catalog->CampMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/TestMaps/CampMissingForValidation.CampMissingForValidation")));
    TestFalse(TEXT("Reject a missing map package"), Catalog->Validate(Error));
    Catalog->CampMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(SavedCatalog));
    TestFalse(TEXT("Reject a Data Asset in place of a map"), Catalog->Validate(Error));
    Catalog->CampMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Demo.Demo")));
    TestTrue(TEXT("Accept distinct existing map packages on the test catalog"), Catalog->Validate(Error));
    FDataValidationContext Context;
    TestTrue(TEXT("Editor uses the same successful map and catalog checks"), Catalog->IsDataValid(Context) == EDataValidationResult::Valid);

    FGirlSaviorSupportOffer First;
    FGirlSaviorSupportOffer Second;
    TestTrue(TEXT("Resolve a pair in selection order"),
        Catalog->ResolveLaunchPair(TEXT("MechanismOverdrive"), TEXT("StaticTrace"), First, Second, Error));
    TestTrue(TEXT("Resolved selection preserves input order"), First.Ability == RateAbility && Second.Ability == SlowAbility);
    TestFalse(TEXT("Reject a duplicate selection"),
        Catalog->ResolveLaunchPair(TEXT("StaticTrace"), TEXT("StaticTrace"), First, Second, Error));
    TestFalse(TEXT("Reject an unknown selection"),
        Catalog->ResolveLaunchPair(TEXT("StaticTrace"), TEXT("Unowned"), First, Second, Error));
    TestTrue(TEXT("Failed resolution leaves both outputs intact"), First.Ability == RateAbility && Second.Ability == SlowAbility);
    return true;
}

#endif
