#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Weapons/GirlSaviorFirearm.h"
#include "Weapons/GirlSaviorPistol.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundWave.h"

namespace
{
FGirlSaviorWeaponRow MakeValidPistolRow()
{
    FGirlSaviorWeaponRow Row;
    Row.DisplayName = FText::FromString(TEXT("Pistol"));
    Row.Description = FText::FromString(TEXT("Single aimed shot"));
    Row.DamageCause = FText::FromString(TEXT("Pistol shot"));
    Row.FirearmClass = UGirlSaviorPistol::StaticClass();
    Row.ProjectileClass = AGirlSaviorTestProjectile::StaticClass();
    Row.Icon = NewObject<UTexture2D>();
    Row.Presentation.HeldMesh = NewObject<UStaticMesh>();
    Row.Presentation.ShotSound = NewObject<USoundWave>();
    Row.DamagePerProjectile = 10.f;
    Row.ShotsPerSecond = 4.f;
    Row.ProjectileRadius = 6.f;
    Row.ProjectileSpeed = 2000.f;
    Row.FullDamageDistance = Row.MaxDistance = 1400.f;
    return Row;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWeaponRowValidationTest, "GirlSavior.Weapons.Definitions.RowValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWeaponRowValidationTest::RunTest(const FString& Parameters)
{
    FText Error;
    const FGirlSaviorWeaponRow Valid = MakeValidPistolRow();
    TestTrue(TEXT("Pistol accepts a hard cutoff"), Valid.Validate(Error));

    FGirlSaviorWeaponRow Invalid = Valid;
    Invalid.FullDamageDistance = 1401.f;
    TestFalse(TEXT("Full damage range cannot exceed the maximum"), Invalid.Validate(Error));
    TestFalse(TEXT("Invalid data supplies an explanation"), Error.IsEmpty());

    Invalid = Valid;
    Invalid.ProjectileSpeed = 0.f;
    TestFalse(TEXT("Zero projectile speed is rejected"), Invalid.Validate(Error));
    Invalid = Valid;
    Invalid.ProjectileCount = 3;
    TestFalse(TEXT("Pistol requires a single projectile"), Invalid.Validate(Error));
    Invalid = Valid;
    Invalid.FirearmClass = UGirlSaviorFirearm::StaticClass();
    TestFalse(TEXT("Abstract firearm is rejected"), Invalid.Validate(Error));
    Invalid = Valid;
    Invalid.Presentation.ShotSound = nullptr;
    TestFalse(TEXT("Missing shot sound is rejected"), Invalid.Validate(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWeaponLoadoutResolutionTest, "GirlSavior.Weapons.Definitions.LoadoutResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWeaponLoadoutResolutionTest::RunTest(const FString& Parameters)
{
    UDataTable* Table = NewObject<UDataTable>();
    Table->RowStruct = FGirlSaviorWeaponRow::StaticStruct();
    const FName Pistol(TEXT("Pistol"));
    Table->AddRow(Pistol, MakeValidPistolRow());

    FGirlSaviorWeaponLoadout Loadout;
    TestEqual(TEXT("Default equips the pistol in slot one"), Loadout.Slot1, Pistol);
    Loadout.Slot1 = NAME_None;
    Loadout.Slot2 = Pistol;
    TArray<FGirlSaviorResolvedWeaponSlot> Slots;
    FText Error;
    if (!TestTrue(TEXT("A gap is valid"), ResolveWeaponLoadout(*Table, Loadout, Slots, Error))
        || !TestEqual(TEXT("Resolution preserves exactly three slots"), Slots.Num(), 3))
    {
        return false;
    }

    TestEqual(TEXT("First slot remains empty"), Slots[0].WeaponId, NAME_None);
    TestEqual(TEXT("Second slot contains the authored pistol"), Slots[1].WeaponId, Pistol);
    TestEqual(TEXT("Third slot remains empty"), Slots[2].WeaponId, NAME_None);
    TestEqual(TEXT("Second slot owns the tuning snapshot"), Slots[1].Row.DamagePerProjectile, 10.f);

    Loadout.Slot3 = Pistol;
    TestFalse(TEXT("Duplicate weapon is rejected"), ResolveWeaponLoadout(*Table, Loadout, Slots, Error));
    TestEqual(TEXT("Rejected loadout preserves the published slots"), Slots.Num(), 3);
    Loadout.Slot3 = TEXT("Unknown");
    TestFalse(TEXT("Unknown row is rejected"), ResolveWeaponLoadout(*Table, Loadout, Slots, Error));
    Loadout.Slot2 = Loadout.Slot3 = NAME_None;
    TestFalse(TEXT("All-empty loadout is rejected"), ResolveWeaponLoadout(*Table, Loadout, Slots, Error));

    Table->RemoveRow(Pistol);
    TestEqual(TEXT("Snapshot survives the source row being removed"), Slots[1].Row.DamagePerProjectile, 10.f);
    return true;
}

#endif
