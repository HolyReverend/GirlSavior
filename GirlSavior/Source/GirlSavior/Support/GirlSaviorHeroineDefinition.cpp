#include "Support/GirlSaviorHeroineDefinition.h"

#include "Support/GirlSaviorSupportAbilityDefinition.h"
#include "Support/GirlSaviorSupportValidation.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "GirlSaviorHeroine"

using GirlSavior::SupportValidation::Fail;

// Validate the profile and the identity of every ability owned by this heroine.
bool UGirlSaviorHeroineDefinition::Validate(FText& OutError) const
{
    const FText Missing = LOCTEXT("Required", "A value is required.");
    if (HeroineId.IsNone())
    {
        return Fail(*this, TEXT("HeroineId"), Missing, OutError);
    }
    if (DisplayName.IsEmptyOrWhitespace())
    {
        return Fail(*this, TEXT("DisplayName"), Missing, OutError);
    }
    if (Description.IsEmptyOrWhitespace())
    {
        return Fail(*this, TEXT("Description"), Missing, OutError);
    }
    if (SupportRole.IsEmptyOrWhitespace())
    {
        return Fail(*this, TEXT("SupportRole"), Missing, OutError);
    }

    TSet<FName> OwnedIds;
    for (int32 Index = 0; Index < Abilities.Num(); ++Index)
    {
        const UGirlSaviorSupportAbilityDefinition* Ability = Abilities[Index];
        const FString Field = FString::Printf(TEXT("Abilities[%d]"), Index);
        if (!IsValid(Ability))
        {
            return Fail(*this, Field, LOCTEXT("MissingAbility", "Assign an ability asset."), OutError);
        }
        if (!Ability->ValidateIdentity(OutError))
        {
            return false;
        }
        if (OwnedIds.Contains(Ability->AbilityId))
        {
            return Fail(*this, Field, LOCTEXT("RepeatedAbility", "The heroine already owns this ability ID."), OutError);
        }
        OwnedIds.Add(Ability->AbilityId);
    }
    return true;
}

#if WITH_EDITOR
// Use native profile and ownership checks when validating an individual heroine asset.
EDataValidationResult UGirlSaviorHeroineDefinition::IsDataValid(FDataValidationContext& Context) const
{
    FText Error;
    if (!Validate(Error))
    {
        Context.AddError(Error);
        return EDataValidationResult::Invalid;
    }
    return EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
