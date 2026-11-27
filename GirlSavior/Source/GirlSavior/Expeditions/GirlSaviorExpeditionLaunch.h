#pragma once

#include "CoreMinimal.h"
#include "Weapons/GirlSaviorWeaponDefinition.h"
#include "GirlSaviorExpeditionLaunch.generated.h"

class UGirlSaviorSupportAbilityDefinition;
class UWorld;

// Committed selections cross travel in a snapshot while the URL carries only the run ID.
USTRUCT()
struct GIRLSAVIOR_API FGirlSaviorExpeditionLaunch
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    FGuid RunId;

    UPROPERTY(Transient)
    FName MissionId;

    UPROPERTY(Transient)
    TArray<FName> AbilityIds;

    UPROPERTY(Transient)
    TArray<TObjectPtr<const UGirlSaviorSupportAbilityDefinition>> Abilities;

    UPROPERTY(Transient)
    FGirlSaviorWeaponLoadout WeaponLoadout;

    UPROPERTY(Transient)
    TArray<FGirlSaviorResolvedWeaponSlot> WeaponSlots;

    UPROPERTY(Transient)
    TSoftObjectPtr<UWorld> CampMap;

    UPROPERTY(Transient)
    TSoftObjectPtr<UWorld> ExpeditionMap;

    bool ValidateAbilities(FText& OutError) const;
    bool ValidateWeapons(FText& OutError) const;
};
