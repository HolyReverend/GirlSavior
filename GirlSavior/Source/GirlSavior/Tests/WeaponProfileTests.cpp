#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"
#include "Weapons/GirlSaviorPistol.h"
#include "Weapons/GirlSaviorWeaponProfileSubsystem.h"
#include "Weapons/GirlSaviorWeaponSaveGame.h"
#include "Combat/GirlSaviorTestProjectile.h"

namespace
{
// Each test uses actual SaveGame serialization with its own disposable slot pair.
struct FWeaponProfileFixture
{
    FGuid Namespace = FGuid::NewGuid();
    FString Prefix = FString(TEXT("WeaponProfileTest_")) + Namespace.ToString(EGuidFormats::Digits);
    UDataTable* Table = NewObject<UDataTable>();

    FWeaponProfileFixture()
    {
        FGirlSaviorWeaponRow Row;
        Row.DisplayName = FText::FromString(TEXT("Weapon"));
        Row.Description = FText::FromString(TEXT("Test row"));
        Row.DamageCause = FText::FromString(TEXT("Shot"));
        Row.Icon = NewObject<UTexture2D>();
        Row.FirearmClass = UGirlSaviorPistol::StaticClass();
        Row.ProjectileClass = AGirlSaviorTestProjectile::StaticClass();
        Row.Presentation.HeldMesh = NewObject<UStaticMesh>();
        Row.Presentation.ShotSound = NewObject<USoundWave>();
        Row.DamagePerProjectile = 10.f;
        Row.ShotsPerSecond = 4.f;
        Row.ProjectileRadius = 6.f;
        Row.ProjectileSpeed = 2000.f;
        Row.FullDamageDistance = Row.MaxDistance = 1400.f;
        Table->RowStruct = FGirlSaviorWeaponRow::StaticStruct();
        Table->AddRow(TEXT("Pistol"), Row);
        Table->AddRow(TEXT("Shotgun"), Row);
    }

    ~FWeaponProfileFixture()
    {
        UGameplayStatics::DeleteGameInSlot(Prefix + TEXT("_A"), 0);
        UGameplayStatics::DeleteGameInSlot(Prefix + TEXT("_B"), 0);
    }

    UGirlSaviorWeaponProfileSubsystem* NewProfile(bool bPlaytest = false) const
    {
        auto* Profile = NewObject<UGirlSaviorWeaponProfileSubsystem>(NewObject<UGameInstance>());
        Profile->SetTestStorage(Namespace, bPlaytest);
        return Profile;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWeaponProfileConfirmationTest, "GirlSavior.Weapons.Profile.Confirmation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWeaponProfileConfirmationTest::RunTest(const FString& Parameters)
{
    FWeaponProfileFixture Fixture;
    FText Error;
    auto* Profile = Fixture.NewProfile();
    if (!TestTrue(TEXT("Missing pair starts a profile"), Profile->EnsureLoaded(*Fixture.Table, Error)))
    {
        return false;
    }
    TestEqual(TEXT("Normal profile owns only pistol"), Profile->GetUnlockedWeaponIds().Num(), 1);
    TestEqual(TEXT("Fresh slot one is pistol"), Profile->GetConfirmedLoadout().Slot1, FName(TEXT("Pistol")));

    FGirlSaviorWeaponLoadout Draft;
    Draft.Slot1 = NAME_None;
    Draft.Slot2 = TEXT("Shotgun");
    TestFalse(TEXT("Known but locked weapon is rejected"), Profile->TryConfirmLoadout(*Fixture.Table, Draft, Error));

    Draft.Slot2 = TEXT("Pistol");
    if (!TestTrue(TEXT("A deliberate gap can be saved"), Profile->TryConfirmLoadout(*Fixture.Table, Draft, Error)))
    {
        return false;
    }
    Draft.Slot2 = NAME_None;
    Draft.Slot3 = TEXT("Pistol");
    Profile->SetTestWriteFailure(true);
    TestFalse(TEXT("Failed write is reported"), Profile->TryConfirmLoadout(*Fixture.Table, Draft, Error));
    TestEqual(TEXT("Failed write keeps previous in-memory slot"), Profile->GetConfirmedLoadout().Slot2, FName(TEXT("Pistol")));

    auto* Reloaded = Fixture.NewProfile();
    TestTrue(TEXT("New subsystem reloads the prior saved record"), Reloaded->EnsureLoaded(*Fixture.Table, Error));
    TestEqual(TEXT("Prior gap survives failed confirmation"), Reloaded->GetConfirmedLoadout().Slot1, NAME_None);
    TestEqual(TEXT("Prior saved slot survives failure"), Reloaded->GetConfirmedLoadout().Slot2, FName(TEXT("Pistol")));
    Profile->SetTestWriteFailure(false);
    TestTrue(TEXT("Retry writes the new slot order"), Profile->TryConfirmLoadout(*Fixture.Table, Draft, Error));
    auto* Latest = Fixture.NewProfile();
    TestTrue(TEXT("Both saved generations can be loaded"), Latest->EnsureLoaded(*Fixture.Table, Error));
    TestEqual(TEXT("The higher generation wins"), Latest->GetConfirmedLoadout().Slot3, FName(TEXT("Pistol")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWeaponProfileRecoveryTest, "GirlSavior.Weapons.Profile.Recovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWeaponProfileRecoveryTest::RunTest(const FString& Parameters)
{
    FWeaponProfileFixture Fixture;
    auto* Old = NewObject<UGirlSaviorWeaponSaveGame>();
    Old->Generation = 3;
    Old->UnlockedWeaponIds = {TEXT("Removed"), TEXT("Shotgun"), TEXT("Shotgun")};
    Old->ConfirmedLoadout.Slot1 = TEXT("Removed");
    Old->ConfirmedLoadout.Slot2 = TEXT("Shotgun");
    Old->ConfirmedLoadout.Slot3 = TEXT("Shotgun");
    const FString SlotA = Fixture.Prefix + TEXT("_A");
    const FString SlotB = Fixture.Prefix + TEXT("_B");
    TestTrue(TEXT("Seed old supported record"), UGameplayStatics::SaveGameToSlot(Old, SlotA, 0));
    const TArray<uint8> CorruptBytes = {0, 0, 0, 0, 0, 0, 0, 0};
    TestTrue(TEXT("Seed unreadable slot"), UGameplayStatics::SaveDataToSlot(CorruptBytes, SlotB, 0));

    FText Error;
    auto* Profile = Fixture.NewProfile();
    if (!TestTrue(TEXT("One readable record is sufficient"), Profile->EnsureLoaded(*Fixture.Table, Error)))
    {
        return false;
    }
    TestEqual(TEXT("Unknown slot is cleared"), Profile->GetConfirmedLoadout().Slot1, NAME_None);
    TestEqual(TEXT("First valid occurrence keeps its position"), Profile->GetConfirmedLoadout().Slot2, FName(TEXT("Shotgun")));
    TestEqual(TEXT("Duplicate slot is cleared"), Profile->GetConfirmedLoadout().Slot3, NAME_None);
    TestTrue(TEXT("Pistol ownership is restored"), Profile->GetUnlockedWeaponIds().Contains(TEXT("Pistol")));
    TestTrue(TEXT("Removed weapon stays owned"), Profile->GetUnlockedWeaponIds().Contains(TEXT("Removed")));
    TestEqual(TEXT("Duplicate ownership is removed"), Profile->GetUnlockedWeaponIds().Num(), 3);

    auto* Future = NewObject<UGirlSaviorWeaponSaveGame>();
    Future->SchemaVersion = Old->SchemaVersion + 1;
    Future->Generation = 4;
    TestTrue(TEXT("Seed future record"), UGameplayStatics::SaveGameToSlot(Future, SlotB, 0));
    auto* Blocked = Fixture.NewProfile();
    TestFalse(TEXT("Future schema blocks confirmation"),
        Blocked->TryConfirmLoadout(*Fixture.Table, FGirlSaviorWeaponLoadout{}, Error));

    TestTrue(TEXT("Replace old record with unreadable bytes"), UGameplayStatics::SaveDataToSlot(CorruptBytes, SlotA, 0));
    TestTrue(TEXT("Replace future test record with unreadable bytes"), UGameplayStatics::SaveDataToSlot(CorruptBytes, SlotB, 0));
    TestFalse(TEXT("Two unreadable saves require recovery"), Fixture.NewProfile()->EnsureLoaded(*Fixture.Table, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorWeaponPlaytestProfileTest, "GirlSavior.Weapons.Profile.Playtest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorWeaponPlaytestProfileTest::RunTest(const FString& Parameters)
{
    FWeaponProfileFixture Fixture;
    FText Error;
    auto* Playtest = Fixture.NewProfile(true);
    TestTrue(TEXT("Fresh playtest profile starts successfully"), Playtest->EnsureLoaded(*Fixture.Table, Error));
    TestEqual(TEXT("Playtest grants all three owned IDs"), Playtest->GetUnlockedWeaponIds().Num(), 3);
    TestEqual(TEXT("Playtest starts with only pistol equipped"), Playtest->GetConfirmedLoadout().Slot2, NAME_None);
    TestFalse(TEXT("Loading never writes the first record"), UGameplayStatics::DoesSaveGameExist(Fixture.Prefix + TEXT("_A"), 0));
    Fixture.Table->RemoveRow(TEXT("Pistol"));
    TestFalse(TEXT("Invalid pistol catalog closes profile access"), Fixture.NewProfile()->EnsureLoaded(*Fixture.Table, Error));
    return true;
}

#endif
