#include "Weapons/GirlSaviorWeaponProfileSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Weapons/GirlSaviorWeaponSaveGame.h"

#define LOCTEXT_NAMESPACE "GirlSaviorWeaponProfile"

namespace
{
const FName PistolId(TEXT("Pistol"));
constexpr int32 UserIndex = 0;

const FGirlSaviorWeaponRow* FindValidRow(const UDataTable& Table, FName Id, FText& OutError)
{
    if (Table.GetRowStruct() != FGirlSaviorWeaponRow::StaticStruct())
    {
        OutError = LOCTEXT("WrongTableType", "Таблица использует неверный тип строки.");
        return nullptr;
    }
    const auto* Row = Table.FindRow<FGirlSaviorWeaponRow>(Id, TEXT("WeaponProfile"), false);
    if (!Row)
    {
        OutError = LOCTEXT("MissingRow", "Строка отсутствует в таблице оружия.");
        return nullptr;
    }
    return Row->Validate(OutError) ? Row : nullptr;
}
}

bool UGirlSaviorWeaponProfileSubsystem::EnsureLoaded(const UDataTable& Table, FText& OutError)
{
    FText RowError;
    if (!FindValidRow(Table, PistolId, RowError))
    {
        OutError = FText::Format(LOCTEXT("InvalidPistol", "Ошибка каталога оружия. Пистолет недоступен. {0}"), RowError);
        return false;
    }
    if (bLoaded)
    {
        OutError = FText::GetEmpty();
        return true;
    }
    if (!SetupStorage(OutError))
    {
        return false;
    }

    bool bAnyRecordExists = false;
    bHasFutureSchema = false;
    const UGirlSaviorWeaponSaveGame* Newest = nullptr;
    int32 NewestIndex = INDEX_NONE;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        if (!DoesRecordExist(Index))
        {
            continue;
        }
        bAnyRecordExists = true;
        const auto* Record = ReadRecord(Index);
        if (!Record)
        {
            continue;
        }
        if (Record->SchemaVersion > UGirlSaviorWeaponSaveGame::CurrentSchemaVersion)
        {
            bHasFutureSchema = true;
            continue;
        }
        if (Record->SchemaVersion == UGirlSaviorWeaponSaveGame::CurrentSchemaVersion && Record->Generation > 0
            && (!Newest || Record->Generation > Newest->Generation))
        {
            Newest = Record;
            NewestIndex = Index;
        }
    }

    if (Newest)
    {
        Generation = Newest->Generation;
        ActiveRecordIndex = NewestIndex;
        ConfirmedLoadout = Newest->ConfirmedLoadout;
        UnlockedWeaponIds = Newest->UnlockedWeaponIds;
    }
    else if (bAnyRecordExists)
    {
        OutError = bHasFutureSchema
            ? LOCTEXT("FutureProfile", "Профиль оружия создан в более новой версии игры.")
            : LOCTEXT("UnreadableProfile", "Не удалось прочитать профиль оружия. Сохранения оставлены для восстановления.");
        return false;
    }
    else if (bPlaytestProfile)
    {
        UnlockedWeaponIds = { PistolId, FName(TEXT("Shotgun")), FName(TEXT("AssaultRifle")) };
    }

    NormalizeLoadedProfile(Table);
    bLoaded = true;
    OutError = FText::GetEmpty();
    return true;
}

// Preserve stable ownership IDs and the positions of valid equipped rows.
void UGirlSaviorWeaponProfileSubsystem::NormalizeLoadedProfile(const UDataTable& Table)
{
    TSet<FName> Owned;
    UnlockedWeaponIds.RemoveAll([&Owned](FName Id)
    {
        if (Id.IsNone() || Owned.Contains(Id))
        {
            return true;
        }
        Owned.Add(Id);
        return false;
    });
    UnlockedWeaponIds.AddUnique(PistolId);

    TSet<FName> Equipped;
    FName* Slots[] = { &ConfirmedLoadout.Slot1, &ConfirmedLoadout.Slot2, &ConfirmedLoadout.Slot3 };
    for (FName* Slot : Slots)
    {
        const FName Id = *Slot;
        FText IgnoredError;
        if (Id.IsNone() || Equipped.Contains(Id) || !UnlockedWeaponIds.Contains(Id)
            || !FindValidRow(Table, Id, IgnoredError))
        {
            *Slot = NAME_None;
            continue;
        }
        Equipped.Add(Id);
    }
    if (Equipped.IsEmpty())
    {
        ConfirmedLoadout.Slot1 = PistolId;
    }
}

bool UGirlSaviorWeaponProfileSubsystem::ValidateLoadout(const UDataTable& Table, const FGirlSaviorWeaponLoadout& Draft,
    TArray<FGirlSaviorResolvedWeaponSlot>& OutSlots, FText& OutError) const
{
    if (!bLoaded)
    {
        OutError = LOCTEXT("ProfileNotLoaded", "Профиль оружия ещё не загружен.");
        return false;
    }
    TArray<FGirlSaviorResolvedWeaponSlot> Resolved;
    if (!ResolveWeaponLoadout(Table, Draft, Resolved, OutError))
    {
        return false;
    }
    for (const FGirlSaviorResolvedWeaponSlot& Slot : Resolved)
    {
        if (!Slot.WeaponId.IsNone() && !UnlockedWeaponIds.Contains(Slot.WeaponId))
        {
            OutError = FText::Format(LOCTEXT("WeaponLocked", "Оружие «{0}» ещё не получено."), Slot.Row.DisplayName);
            return false;
        }
    }
    OutSlots = MoveTemp(Resolved);
    OutError = FText::GetEmpty();
    return true;
}

// Write into the inactive record, then publish the candidate after the save succeeds.
bool UGirlSaviorWeaponProfileSubsystem::TryConfirmLoadout(const UDataTable& Table,
    const FGirlSaviorWeaponLoadout& Draft, FText& OutError)
{
    if (!EnsureLoaded(Table, OutError))
    {
        return false;
    }
    if (bHasFutureSchema)
    {
        OutError = LOCTEXT("FutureProfile", "Профиль оружия создан в более новой версии игры.");
        return false;
    }
    TArray<FGirlSaviorResolvedWeaponSlot> Resolved;
    if (!ValidateLoadout(Table, Draft, Resolved, OutError))
    {
        return false;
    }

    if (Generation == MAX_int64)
    {
        OutError = LOCTEXT("GenerationLimit", "Достигнут предел версий сохранения профиля оружия.");
        return false;
    }
    auto* Candidate = NewObject<UGirlSaviorWeaponSaveGame>();
    Candidate->Generation = Generation + 1;
    Candidate->UnlockedWeaponIds = UnlockedWeaponIds;
    Candidate->ConfirmedLoadout = Draft;
    const int32 TargetIndex = ActiveRecordIndex == 0 ? 1 : 0;
    if (!WriteRecord(*Candidate, TargetIndex))
    {
        OutError = LOCTEXT("WriteFailed", "Не удалось сохранить набор оружия. Попробуйте ещё раз.");
        return false;
    }

    ActiveRecordIndex = TargetIndex;
    Generation = Candidate->Generation;
    ConfirmedLoadout = Candidate->ConfirmedLoadout;
    OutError = FText::GetEmpty();
    return true;
}

bool UGirlSaviorWeaponProfileSubsystem::SetupStorage(FText& OutError)
{
    if (!SlotPrefix.IsEmpty())
    {
        return true;
    }
#if !UE_BUILD_SHIPPING
    bPlaytestProfile = FParse::Param(FCommandLine::Get(), TEXT("GirlSaviorWeaponPlaytest"));
#endif
#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
    FString Namespace;
    if (FParse::Value(FCommandLine::Get(), TEXT("GirlSaviorWeaponTestProfile="), Namespace))
    {
        FGuid Id;
        if (!FGuid::ParseExact(Namespace, EGuidFormats::Digits, Id) || !Id.IsValid())
        {
            OutError = LOCTEXT("InvalidTestProfile", "Неверный идентификатор тестового профиля оружия.");
            return false;
        }
        SlotPrefix = FString(TEXT("WeaponProfileTest_")) + Id.ToString(EGuidFormats::Digits);
        return true;
    }
#endif
    SlotPrefix = bPlaytestProfile ? TEXT("WeaponPlaytest") : TEXT("WeaponProfile");
    return true;
}

FString UGirlSaviorWeaponProfileSubsystem::GetSlotName(int32 SlotIndex) const
{
    return SlotPrefix + (SlotIndex == 0 ? TEXT("_A") : TEXT("_B"));
}

bool UGirlSaviorWeaponProfileSubsystem::DoesRecordExist(int32 SlotIndex) const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bTestInMemory)
    {
        return !TestRecords[SlotIndex].IsEmpty();
    }
#endif
    return UGameplayStatics::DoesSaveGameExist(GetSlotName(SlotIndex), UserIndex);
}

UGirlSaviorWeaponSaveGame* UGirlSaviorWeaponProfileSubsystem::ReadRecord(int32 SlotIndex) const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bTestInMemory)
    {
        return Cast<UGirlSaviorWeaponSaveGame>(UGameplayStatics::LoadGameFromMemory(TestRecords[SlotIndex]));
    }
#endif
    return Cast<UGirlSaviorWeaponSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSlotName(SlotIndex), UserIndex));
}

bool UGirlSaviorWeaponProfileSubsystem::WriteRecord(UGirlSaviorWeaponSaveGame& Record, int32 SlotIndex)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bTestWriteFailure)
    {
        return false;
    }
    if (bTestInMemory)
    {
        TArray<uint8> Bytes;
        if (!UGameplayStatics::SaveGameToMemory(&Record, Bytes))
        {
            return false;
        }
        TestRecords[SlotIndex] = MoveTemp(Bytes);
        return true;
    }
#endif
    return UGameplayStatics::SaveGameToSlot(&Record, GetSlotName(SlotIndex), UserIndex);
}

#if WITH_DEV_AUTOMATION_TESTS
void UGirlSaviorWeaponProfileSubsystem::SetTestStorage(const FGuid& Namespace, bool bPlaytest, bool bInMemory)
{
    check(Namespace.IsValid() && SlotPrefix.IsEmpty());
    SlotPrefix = FString(TEXT("WeaponProfileTest_")) + Namespace.ToString(EGuidFormats::Digits);
    bPlaytestProfile = bPlaytest;
    bTestInMemory = bInMemory;
}
#endif

#undef LOCTEXT_NAMESPACE
