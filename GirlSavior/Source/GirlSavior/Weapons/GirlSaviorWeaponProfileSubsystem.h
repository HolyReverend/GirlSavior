#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Weapons/GirlSaviorWeaponDefinition.h"
#include "GirlSaviorWeaponProfileSubsystem.generated.h"

class UGirlSaviorWeaponSaveGame;

UCLASS()
class GIRLSAVIOR_API UGirlSaviorWeaponProfileSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    bool EnsureLoaded(const UDataTable& Table, FText& OutError);
    bool TryConfirmLoadout(const UDataTable& Table, const FGirlSaviorWeaponLoadout& Draft, FText& OutError);
    bool ValidateLoadout(const UDataTable& Table, const FGirlSaviorWeaponLoadout& Draft,
        TArray<FGirlSaviorResolvedWeaponSlot>& OutSlots, FText& OutError) const;

    const FGirlSaviorWeaponLoadout& GetConfirmedLoadout() const { return ConfirmedLoadout; }
    const TArray<FName>& GetUnlockedWeaponIds() const { return UnlockedWeaponIds; }

#if WITH_DEV_AUTOMATION_TESTS
    // Select isolated storage before loading, with optional in-memory saves and write failure injection.
    void SetTestStorage(const FGuid& Namespace, bool bPlaytest, bool bInMemory = false);
    void SetTestWriteFailure(bool bFail) { bTestWriteFailure = bFail; }
#endif

private:
    bool SetupStorage(FText& OutError);
    void NormalizeLoadedProfile(const UDataTable& Table);
    FString GetSlotName(int32 SlotIndex) const;
    bool DoesRecordExist(int32 SlotIndex) const;
    UGirlSaviorWeaponSaveGame* ReadRecord(int32 SlotIndex) const;
    bool WriteRecord(UGirlSaviorWeaponSaveGame& Record, int32 SlotIndex);

    FGirlSaviorWeaponLoadout ConfirmedLoadout;
    TArray<FName> UnlockedWeaponIds;
    FString SlotPrefix;
    int64 Generation = 0;
    int32 ActiveRecordIndex = INDEX_NONE;
    bool bLoaded = false;
    bool bPlaytestProfile = false;
    bool bHasFutureSchema = false;

#if WITH_DEV_AUTOMATION_TESTS
    bool bTestWriteFailure = false;
    bool bTestInMemory = false;
    TArray<uint8> TestRecords[2];
#endif
};
