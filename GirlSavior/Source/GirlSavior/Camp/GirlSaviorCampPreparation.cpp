#include "Camp/GirlSaviorCampPreparation.h"

#include "Camp/GirlSaviorCampDefinition.h"

#define LOCTEXT_NAMESPACE "GirlSaviorCampPreparation"

// Seed the draft only when the entire remembered pair is still available and valid.
void FGirlSaviorCampPreparation::Begin(const UGirlSaviorCampDefinition& Catalog, TConstArrayView<FName> ConfirmedIds)
{
    MissionId = Catalog.MissionId;
    AbilityIds.Reset();
    if (ConfirmedIds.Num() == SlotCount)
    {
        FGirlSaviorSupportOffer First;
        FGirlSaviorSupportOffer Second;
        FText Error;
        if (Catalog.ResolveLaunchPair(ConfirmedIds[0], ConfirmedIds[1], First, Second, Error))
        {
            AbilityIds.Append(ConfirmedIds.GetData(), ConfirmedIds.Num());
        }
    }
}

// A picked card removes itself. Removing one card preserves the order of the others.
EGirlSaviorCampSelectionChange FGirlSaviorCampPreparation::ToggleAbility(
    const UGirlSaviorCampDefinition& Catalog, FName AbilityId)
{
    if (AbilityIds.RemoveSingle(AbilityId) != 0)
    {
        return EGirlSaviorCampSelectionChange::Removed;
    }
    if (!Catalog.FindOffer(AbilityId).Ability)
    {
        return EGirlSaviorCampSelectionChange::Unavailable;
    }
    if (AbilityIds.Num() == SlotCount)
    {
        return EGirlSaviorCampSelectionChange::Full;
    }

    AbilityIds.Add(AbilityId);
    return EGirlSaviorCampSelectionChange::Selected;
}

// Both the live button state and the action itself use the same launch checks.
bool FGirlSaviorCampPreparation::ResolveForLaunch(const UGirlSaviorCampDefinition& Catalog,
    FGirlSaviorSupportOffer& OutFirst, FGirlSaviorSupportOffer& OutSecond, FText& OutError) const
{
    if (MissionId.IsNone() || MissionId != Catalog.MissionId)
    {
        OutError = LOCTEXT("UnavailableMission", "Вылазка недоступна.");
        return false;
    }
    if (AbilityIds.Num() != SlotCount)
    {
        OutError = LOCTEXT("NeedTwo", "Выберите два разных умения.");
        return false;
    }
    return Catalog.ResolveLaunchPair(AbilityIds[0], AbilityIds[1], OutFirst, OutSecond, OutError);
}

#undef LOCTEXT_NAMESPACE
