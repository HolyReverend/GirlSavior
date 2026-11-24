#pragma once

#include "CoreMinimal.h"
#include "GirlSaviorExpeditionLaunch.generated.h"

class UGirlSaviorSupportAbilityDefinition;
class UWorld;

// A confirmed draft crosses travel by reference while the URL carries only its unique ID.
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
    TSoftObjectPtr<UWorld> CampMap;

    UPROPERTY(Transient)
    TSoftObjectPtr<UWorld> ExpeditionMap;

    bool ValidateAbilities(FText& OutError) const;
};
