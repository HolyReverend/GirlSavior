#include "Camp/GirlSaviorCampPrototypeDefinition.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Support/GirlSaviorHeroineDefinition.h"
#include "Support/GirlSaviorSupportAbilityDefinition.h"
#include "Support/GirlSaviorSupportValidation.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "GirlSaviorCampDefinition"

using GirlSavior::SupportValidation::Fail;

namespace GirlSavior::CampValidation
{
// Map travel uses top-level world objects named after their packages.
bool IsCanonicalMapPath(const FSoftObjectPath& Path)
{
    const FString PackageName = Path.GetLongPackageName();
    return Path.GetSubPathString().IsEmpty()
        && FPackageName::IsValidLongPackageName(PackageName)
        && Path.GetAssetName() == FPackageName::GetShortName(PackageName);
}

// Check the asset class without loading either world, including in cooked builds.
bool MapPackageExists(const FSoftObjectPath& Path)
{
    if (!FPackageName::DoesPackageExist(Path.GetLongPackageName()))
    {
        return false;
    }
    const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    const FAssetData Map = AssetRegistry.GetAssetByObjectPath(Path, /*bIncludeOnlyOnDiskAssets=*/true);
    return Map.AssetClassPath == UWorld::StaticClass()->GetClassPathName();
}
}

// Resolve the complete authored roster before looking at the subset advertised by Camp.
bool UGirlSaviorCampPrototypeDefinition::ValidateCatalog(FText& OutError) const
{
    if (MissionId.IsNone())
    {
        return Fail(*this, TEXT("MissionId"), LOCTEXT("MissionIdRequired", "The mission needs a stable ID."), OutError);
    }
    if (MissionName.IsEmptyOrWhitespace())
    {
        return Fail(*this, TEXT("MissionName"), LOCTEXT("MissionNameRequired", "The mission needs a title."), OutError);
    }
    if (MissionDescription.IsEmptyOrWhitespace())
    {
        return Fail(*this, TEXT("MissionDescription"), LOCTEXT("MissionDescriptionRequired", "The mission needs a description."), OutError);
    }
    if (AvailableAbilityIds.Num() != 3 || Roster.Num() != 3)
    {
        return Fail(*this, TEXT("Roster / AvailableAbilityIds"),
            LOCTEXT("FixtureSize", "Camp requires three heroines and three offered abilities."), OutError);
    }

    TSet<FName> HeroineIds;
    TMap<FName, FGirlSaviorSupportOffer> OwnedOffers;
    for (int32 Index = 0; Index < Roster.Num(); ++Index)
    {
        const UGirlSaviorHeroineDefinition* Heroine = Roster[Index];
        const FString Field = FString::Printf(TEXT("Roster[%d]"), Index);
        if (!IsValid(Heroine))
        {
            return Fail(*this, Field, LOCTEXT("MissingHeroine", "Assign a heroine asset."), OutError);
        }
        if (!Heroine->Validate(OutError))
        {
            return false;
        }
        if (HeroineIds.Contains(Heroine->HeroineId))
        {
            return Fail(*Heroine, TEXT("HeroineId"), LOCTEXT("RepeatedHeroine", "Each roster entry needs a unique heroine ID."), OutError);
        }
        HeroineIds.Add(Heroine->HeroineId);

        for (const UGirlSaviorSupportAbilityDefinition* Ability : Heroine->Abilities)
        {
            if (OwnedOffers.Contains(Ability->AbilityId))
            {
                return Fail(*Heroine, TEXT("Abilities"),
                    FText::Format(LOCTEXT("SharedOwnership", "Ability ID {0} already has a roster owner."),
                        FText::FromName(Ability->AbilityId)), OutError);
            }
            OwnedOffers.Add(Ability->AbilityId, FGirlSaviorSupportOffer{Heroine, Ability});
        }
    }

    // One offered card per heroine also rejects repeated IDs in the offer list.
    TSet<const UGirlSaviorHeroineDefinition*> OfferedHeroines;
    TArray<const UGirlSaviorSupportAbilityDefinition*, TInlineAllocator<3>> OfferedAbilities;
    for (int32 Index = 0; Index < AvailableAbilityIds.Num(); ++Index)
    {
        const FName AbilityId = AvailableAbilityIds[Index];
        const FGirlSaviorSupportOffer* Offer = OwnedOffers.Find(AbilityId);
        const FString Field = FString::Printf(TEXT("AvailableAbilityIds[%d]"), Index);
        if (!Offer)
        {
            return Fail(*this, Field, LOCTEXT("UnownedOffer", "The offer must resolve to an ability owned by a roster heroine."), OutError);
        }
        if (OfferedHeroines.Contains(Offer->Heroine))
        {
            return Fail(*this, Field, LOCTEXT("RepeatedOfferOwner", "Each heroine can offer only one ability in this fixture."), OutError);
        }
        OfferedHeroines.Add(Offer->Heroine);
        OfferedAbilities.Add(Offer->Ability);
    }

    // Validate every advertised pair, including the ability and mechanic data in each.
    for (int32 First = 0; First < OfferedAbilities.Num(); ++First)
    {
        for (int32 Second = First + 1; Second < OfferedAbilities.Num(); ++Second)
        {
            if (!UGirlSaviorSupportAbilityDefinition::ValidatePair(*OfferedAbilities[First], *OfferedAbilities[Second], OutError))
            {
                return false;
            }
        }
    }
    return true;
}

// Launch requires a complete catalog and distinct map references. Travel preflights the destination.
bool UGirlSaviorCampPrototypeDefinition::Validate(FText& OutError) const
{
    if (!ValidateCatalog(OutError))
    {
        return false;
    }

    const FSoftObjectPath ExpeditionPath = ExpeditionMap.ToSoftObjectPath();
    const FSoftObjectPath CampPath = CampMap.ToSoftObjectPath();
    const FText RequiredMap = LOCTEXT("RequiredMap", "Assign a top-level map asset.");
    if (!GirlSavior::CampValidation::IsCanonicalMapPath(ExpeditionPath))
    {
        return Fail(*this, TEXT("ExpeditionMap"), RequiredMap, OutError);
    }
    if (!GirlSavior::CampValidation::IsCanonicalMapPath(CampPath))
    {
        return Fail(*this, TEXT("CampMap"), RequiredMap, OutError);
    }
    if (ExpeditionPath.GetLongPackageFName() == CampPath.GetLongPackageFName())
    {
        return Fail(*this, TEXT("CampMap"), LOCTEXT("SameMap", "Camp and expedition must use different map packages."), OutError);
    }

    const FText MissingMap = LOCTEXT("MissingMapPackage", "The reference must point to a saved world asset.");
    if (!GirlSavior::CampValidation::MapPackageExists(ExpeditionPath))
    {
        return Fail(*this, TEXT("ExpeditionMap"), MissingMap, OutError);
    }
    if (!GirlSavior::CampValidation::MapPackageExists(CampPath))
    {
        return Fail(*this, TEXT("CampMap"), MissingMap, OutError);
    }
    return true;
}

// Look up the card by stable ID in the authored owner list.
FGirlSaviorSupportOffer UGirlSaviorCampPrototypeDefinition::FindOffer(FName AbilityId) const
{
    if (AbilityId.IsNone() || !AvailableAbilityIds.Contains(AbilityId))
    {
        return {};
    }

    for (const UGirlSaviorHeroineDefinition* Heroine : Roster)
    {
        if (!IsValid(Heroine))
        {
            continue;
        }
        for (const UGirlSaviorSupportAbilityDefinition* Ability : Heroine->Abilities)
        {
            if (IsValid(Ability) && Ability->AbilityId == AbilityId)
            {
                return {Heroine, Ability};
            }
        }
    }
    return {};
}

// Reject invalid selections before writing either result, preserving the caller's draft on failure.
bool UGirlSaviorCampPrototypeDefinition::ResolveLaunchPair(FName FirstId, FName SecondId,
    FGirlSaviorSupportOffer& OutFirst, FGirlSaviorSupportOffer& OutSecond, FText& OutError) const
{
    if (!Validate(OutError))
    {
        return false;
    }

    const FGirlSaviorSupportOffer First = FindOffer(FirstId);
    const FGirlSaviorSupportOffer Second = FindOffer(SecondId);
    if (!First.Ability || !Second.Ability || FirstId == SecondId)
    {
        return Fail(*this, TEXT("AvailableAbilityIds"),
            LOCTEXT("InvalidSelection", "Select two different offered abilities."), OutError);
    }

    OutFirst = First;
    OutSecond = Second;
    return true;
}

#if WITH_EDITOR
// The standard editor validation action becomes fully valid once Camp 2 assigns both maps.
EDataValidationResult UGirlSaviorCampPrototypeDefinition::IsDataValid(FDataValidationContext& Context) const
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
