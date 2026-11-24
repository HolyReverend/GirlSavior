#include "Support/GirlSaviorSupportAbilityDefinition.h"

#include "Engine/Texture2D.h"
#include "Support/GirlSaviorSupportEffectDefinition.h"
#include "Support/GirlSaviorSupportValidation.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "GirlSaviorSupportAbility"

using GirlSavior::SupportValidation::Fail;

// Every owned entry needs a stable ID and readable text, even when it is hidden from this fixture.
bool UGirlSaviorSupportAbilityDefinition::ValidateIdentity(FText& OutError) const
{
    const FText Missing = LOCTEXT("Required", "A value is required.");
    if (AbilityId.IsNone())
    {
        return Fail(*this, TEXT("AbilityId"), Missing, OutError);
    }
    if (DisplayName.IsEmptyOrWhitespace())
    {
        return Fail(*this, TEXT("DisplayName"), Missing, OutError);
    }
    if (Description.IsEmptyOrWhitespace())
    {
        return Fail(*this, TEXT("Description"), Missing, OutError);
    }
    return true;
}

// A playable card needs its icon and at least one valid, uniquely keyed inline mechanic.
bool UGirlSaviorSupportAbilityDefinition::Validate(FText& OutError) const
{
    if (!ValidateIdentity(OutError))
    {
        return false;
    }
    if (!IsValid(Icon))
    {
        return Fail(*this, TEXT("Icon"), LOCTEXT("MissingIcon", "Assign a card icon."), OutError);
    }
    if (Effects.IsEmpty())
    {
        return Fail(*this, TEXT("Effects"), LOCTEXT("MissingEffects", "Add at least one effect."), OutError);
    }

    TSet<FName> EffectKeys;
    for (int32 Index = 0; Index < Effects.Num(); ++Index)
    {
        const UGirlSaviorSupportEffectDefinition* Effect = Effects[Index];
        const FString Field = FString::Printf(TEXT("Effects[%d]"), Index);
        if (!IsValid(Effect) || Effect->GetOuter() != this)
        {
            return Fail(*this, Field, LOCTEXT("MissingInlineEffect", "Add an inline effect owned by this ability."), OutError);
        }
        const FName Key = Effect->GetEffectKey();
        if (Key.IsNone() || EffectKeys.Contains(Key))
        {
            return Fail(*this, Field, LOCTEXT("RepeatedEffect", "Each effect must have a unique mechanic key."), OutError);
        }
        if (!Effect->Validate(OutError))
        {
            return false;
        }
        EffectKeys.Add(Key);
    }
    return true;
}

// Prototype mechanics do not stack. Share this check with destination-side validation.
bool UGirlSaviorSupportAbilityDefinition::ValidatePair(const UGirlSaviorSupportAbilityDefinition& First,
    const UGirlSaviorSupportAbilityDefinition& Second, FText& OutError)
{
    if (!First.Validate(OutError) || !Second.Validate(OutError))
    {
        return false;
    }
    if (First.AbilityId == Second.AbilityId)
    {
        return Fail(Second, TEXT("AbilityId"), LOCTEXT("RepeatedAbility", "Select two different abilities."), OutError);
    }

    TSet<FName> FirstKeys;
    for (const UGirlSaviorSupportEffectDefinition* Effect : First.Effects)
    {
        FirstKeys.Add(Effect->GetEffectKey());
    }
    for (const UGirlSaviorSupportEffectDefinition* Effect : Second.Effects)
    {
        const FName Key = Effect->GetEffectKey();
        if (FirstKeys.Contains(Key))
        {
            return Fail(Second, TEXT("Effects"), FText::Format(LOCTEXT("PairConflict", "The mechanic {0} is already used by {1}."),
                FText::FromName(Key), FText::FromName(First.AbilityId)), OutError);
        }
    }
    return true;
}

// Compose one localized detail line per inline effect. Invalid entries are rejected by Validate.
FText UGirlSaviorSupportAbilityDefinition::BuildDetailsText() const
{
    TArray<FText> Lines;
    for (const UGirlSaviorSupportEffectDefinition* Effect : Effects)
    {
        if (IsValid(Effect))
        {
            Lines.Add(Effect->BuildDetailsText());
        }
    }
    return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

#if WITH_EDITOR
// Report the same problem the native launch path would reject.
EDataValidationResult UGirlSaviorSupportAbilityDefinition::IsDataValid(FDataValidationContext& Context) const
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
