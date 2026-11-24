#pragma once

#include "CoreMinimal.h"

class UGirlSaviorCampDefinition;
struct FGirlSaviorSupportOffer;

enum class EGirlSaviorCampSelectionChange : uint8
{
    Selected,
    Removed,
    Full,
    Unavailable
};

// One table visit owns one ordered draft. The session owns any previously confirmed pair.
struct GIRLSAVIOR_API FGirlSaviorCampPreparation
{
    static constexpr int32 SlotCount = 2;

    void Begin(const UGirlSaviorCampDefinition& Catalog, TConstArrayView<FName> ConfirmedIds = {});
    EGirlSaviorCampSelectionChange ToggleAbility(const UGirlSaviorCampDefinition& Catalog, FName AbilityId);
    bool ResolveForLaunch(const UGirlSaviorCampDefinition& Catalog, FGirlSaviorSupportOffer& OutFirst,
        FGirlSaviorSupportOffer& OutSecond, FText& OutError) const;

    FName GetMissionId() const { return MissionId; }
    TConstArrayView<FName> GetAbilityIds() const { return AbilityIds; }

private:
    FName MissionId;
    TArray<FName, TInlineAllocator<SlotCount>> AbilityIds;
};
