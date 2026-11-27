#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Camp/GirlSaviorCampPreparation.h"
#include "Camp/GirlSaviorCampDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Expeditions/GirlSaviorExpeditionSessionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Weapons/GirlSaviorWeaponProfileSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorExpeditionSessionTest, "GirlSavior.Camp.ExpeditionSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace GirlSavior::ExpeditionSessionTests
{
// Reuse the real catalog paths with isolated PIE package names. These worlds are never placed in an engine context.
UWorld* MakeSessionWorld(UGameInstance& Instance, const TSoftObjectPtr<UWorld>& Map)
{
    static int32 NextInstanceId = 30000;
    const FString PackageName = UWorld::ConvertToPIEPackageName(Map.ToSoftObjectPath().GetLongPackageName(), NextInstanceId++);
    UWorld* World = UWorld::CreateWorld(EWorldType::PIE, false, TEXT("ExpeditionSessionFixture"), CreatePackage(*PackageName));
    if (World)
    {
        World->SetGameInstance(&Instance);
    }
    return World;
}
}

// Drive failure and retry branches with synthetic worlds and the same dispatch gate the actual game uses.
bool FGirlSaviorExpeditionSessionTest::RunTest(const FString& Parameters)
{
    using namespace GirlSavior::ExpeditionSessionTests;

    const auto* Catalog = LoadObject<UGirlSaviorCampDefinition>(nullptr,
        TEXT("/Game/Data/DataAssets/Camp/DA_CampPrototype.DA_CampPrototype"));
    if (!TestNotNull(TEXT("Saved Camp catalog"), Catalog))
    {
        return false;
    }

    auto* Instance = NewObject<UGameInstance>();
    auto* Session = NewObject<UGirlSaviorExpeditionSessionSubsystem>(Instance);
    auto* Profile = NewObject<UGirlSaviorWeaponProfileSubsystem>(Instance);
    Profile->SetTestStorage(FGuid::NewGuid(), true, true);
    Session->SetWeaponProfileForTest(Profile);
    UWorld* Camp = MakeSessionWorld(*Instance, Catalog->CampMap);
    UWorld* Route = MakeSessionWorld(*Instance, Catalog->ExpeditionMap);
    UWorld* ForeignRoute = MakeSessionWorld(*NewObject<UGameInstance>(), Catalog->ExpeditionMap);
    ON_SCOPE_EXIT
    {
        for (UWorld* World : {Camp, Route, ForeignRoute})
        {
            if (World)
            {
                World->DestroyWorld(false);
            }
        }
    };
    if (!TestNotNull(TEXT("Camp test world"), Camp) || !TestNotNull(TEXT("Route test world"), Route)
        || !TestNotNull(TEXT("Foreign route test world"), ForeignRoute))
    {
        return false;
    }

    const FName Slow(TEXT("StaticTrace"));
    const FName Rate(TEXT("MechanismOverdrive"));
    FGirlSaviorCampPreparation Draft;
    Draft.Begin(*Catalog);
    Draft.ToggleAbility(*Catalog, Rate);
    Draft.ToggleAbility(*Catalog, Slow);

    FText Error;
    FGirlSaviorExpeditionLaunch Claim;
    FGirlSaviorWeaponLoadout Loadout;
    Loadout.Slot1 = NAME_None;
    Loadout.Slot2 = TEXT("AssaultRifle");
    Loadout.Slot3 = TEXT("Shotgun");
    if (!TestNotNull(TEXT("Camp weapon table"), Catalog->WeaponTable.Get())
        || !TestTrue(TEXT("Confirm a playtest set with slot one empty"),
            Profile->TryConfirmLoadout(*Catalog->WeaponTable, Loadout, Error)))
    {
        return false;
    }
    FString Options;
    auto CaptureDispatch = [&Options](UWorld&, const TSoftObjectPtr<UWorld>&, const FString& LaunchOptions)
    {
        Options = TEXT("?") + LaunchOptions;
        return true;
    };
    const auto RejectDispatch = [](UWorld&, const TSoftObjectPtr<UWorld>&, const FString&) { return false; };

    TestTrue(TEXT("A direct route without a token uses standalone setup"),
        Session->ClaimLaunch(*Route, FString(), Claim, Error) == EGirlSaviorExpeditionClaim::Standalone);
    TestTrue(TEXT("An untrusted token cannot grant a Camp return"),
        Session->ClaimLaunch(*Route, TEXT("?CampRunId=bad"), Claim, Error) == EGirlSaviorExpeditionClaim::Rejected
            && !Session->CanReturn(*Route) && Claim.Abilities.IsEmpty());

    Session->SetNextDispatchForTest(RejectDispatch);
    TestFalse(TEXT("A rejected dispatch can be retried from the origin"), Session->RequestLaunch(*Camp, *Catalog, Draft, Error));
    TestFalse(TEXT("A failed dispatch unlocks the Camp origin"), Session->IsLaunchPendingFrom(*Camp));

    const FString RejectedOptions[] = {
        FString(), TEXT("?CampRunId=bad"), TEXT("?CampRunId=7E02574F-25ED-4854-86AF-066DAEBCB29B")
    };
    for (const FString& RejectedOption : RejectedOptions)
    {
        Session->SetNextDispatchForTest(CaptureDispatch);
        if (!TestTrue(TEXT("Next request is possible after failure"), Session->RequestLaunch(*Camp, *Catalog, Draft, Error)))
        {
            return false;
        }
        TestTrue(TEXT("A missing, malformed or stale token enters recovery without granting effects"),
            Session->ClaimLaunch(*Route, RejectedOption, Claim, Error) == EGirlSaviorExpeditionClaim::Rejected
                && Claim.Abilities.IsEmpty() && Session->CanReturn(*Route));
        TestEqual(TEXT("Recovery does not remember the rejected pair"), Session->GetLastConfirmedAbilityIds().Num(), 0);
        Session->SetNextDispatchForTest(CaptureDispatch);
        TestTrue(TEXT("Trusted recovery can return to Camp"), Session->RequestReturn(*Route, Error));
        Session->NotifyCampReady(*Camp);
    }

    Session->SetNextDispatchForTest(CaptureDispatch);
    if (!TestTrue(TEXT("A valid pair begins one pending request"), Session->RequestLaunch(*Camp, *Catalog, Draft, Error)))
    {
        return false;
    }
    TestTrue(TEXT("A later profile write succeeds independently of the pending launch"),
        Profile->TryConfirmLoadout(*Catalog->WeaponTable, FGirlSaviorWeaponLoadout{}, Error));
    TestTrue(TEXT("The pending URL carries only the run token"),
        Options.StartsWith(TEXT("?CampRunId=")) && !Options.Contains(TEXT("AssaultRifle")));
    TestFalse(TEXT("A pending request rejects duplicate dispatch"), Session->RequestLaunch(*Camp, *Catalog, Draft, Error));
    TestTrue(TEXT("A matching route from another game instance cannot claim the request"),
        Session->ClaimLaunch(*ForeignRoute, Options, Claim, Error) == EGirlSaviorExpeditionClaim::Rejected);
    if (!TestTrue(TEXT("The matching route claims the pending pair"),
        Session->ClaimLaunch(*Route, Options, Claim, Error) == EGirlSaviorExpeditionClaim::Claimed))
    {
        return false;
    }

    const FGuid AcceptedId = Claim.RunId;
    TestTrue(TEXT("The claim retains the original gap and weapon row copies"),
        Claim.ValidateWeapons(Error) && Claim.WeaponLoadout.Slot1.IsNone()
            && Claim.WeaponSlots[1].WeaponId == TEXT("AssaultRifle")
            && Claim.WeaponSlots[2].WeaponId == TEXT("Shotgun")
            && Claim.WeaponSlots[1].Row.DamagePerProjectile == 5.5f);
    if (!TestTrue(TEXT("Claim preserves the selected order"), Claim.AbilityIds == TArray<FName>({Rate, Slow})))
    {
        return false;
    }
    FGirlSaviorExpeditionLaunch Tampered = Claim;
    Tampered.AbilityIds.Swap(0, 1);
    TestFalse(TEXT("Payload validation detects mismatched stable IDs and assets"), Tampered.ValidateAbilities(Error));
    Tampered.WeaponSlots.Swap(1, 2);
    TestFalse(TEXT("Payload validation rejects out-of-order weapon rows"), Tampered.ValidateWeapons(Error));
    TestFalse(TEXT("A claim alone does not permit returning"), Session->CanReturn(*Route));
    TestTrue(TEXT("A duplicate claimant leaves the reserved pair intact"),
        Session->ClaimLaunch(*Route, Options, Claim, Error) == EGirlSaviorExpeditionClaim::Duplicate && Claim.Abilities.IsEmpty());
    TestFalse(TEXT("Only the claiming run ID can commit"), Session->CommitLaunch(*Route, FGuid::NewGuid(), Error));
    TestTrue(TEXT("The claiming world commits the selected pair"), Session->CommitLaunch(*Route, AcceptedId, Error));

    const TConstArrayView<FName> Remembered = Session->GetLastConfirmedAbilityIds();
    TestTrue(TEXT("Only the committed pair is remembered in order"),
        Remembered.Num() == 2 && Remembered[0] == Rate && Remembered[1] == Slow);
    Session->SetNextDispatchForTest(RejectDispatch);
    TestFalse(TEXT("Failed return dispatch leaves the expedition open"), Session->RequestReturn(*Route, Error));
    TestTrue(TEXT("Failed return restores retry eligibility"), Session->CanReturn(*Route));
    Session->SetNextDispatchForTest(CaptureDispatch);
    TestTrue(TEXT("Return can be retried"), Session->RequestReturn(*Route, Error));
    TestTrue(TEXT("Return travel carries no token"), Options == TEXT("?"));
    Session->NotifyCampReady(*Camp);
    TestTrue(TEXT("Ready Camp retires the run and retains its selection"),
        !Session->GetActiveRunId(*Route).IsValid() && Session->GetLastConfirmedAbilityIds().Num() == 2);
    return true;
}

#endif
