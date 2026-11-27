#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Serialization/Archive.h"
#include "Weapons/GirlSaviorWeaponDefinition.h"
#include "GirlSaviorWeaponSaveGame.generated.h"

UCLASS()
class GIRLSAVIOR_API UGirlSaviorWeaponSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchemaVersion = 1;

    UPROPERTY()
    int32 SchemaVersion = CurrentSchemaVersion;

    UPROPERTY()
    int64 Generation = 0;

    UPROPERTY()
    TArray<FName> UnlockedWeaponIds;

    UPROPERTY()
    FGirlSaviorWeaponLoadout ConfirmedLoadout;

    virtual void Serialize(FArchive& Ar) override
    {
        Super::Serialize(Ar);
        if (Ar.IsLoading() && Ar.IsError())
        {
            SchemaVersion = 0;
            Generation = 0;
        }
    }
};
