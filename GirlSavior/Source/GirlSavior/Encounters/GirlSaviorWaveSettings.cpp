#include "Encounters/GirlSaviorWaveSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
bool IsPositive(float Value)
{
    return FMath::IsFinite(Value) && Value > 0.f;
}

bool IsKnownRole(EGirlSaviorEnemyRole Role)
{
    return Role == EGirlSaviorEnemyRole::Pursuer || Role == EGirlSaviorEnemyRole::Shooter
        || Role == EGirlSaviorEnemyRole::Charger;
}
}

bool UGirlSaviorWaveSettings::IsConfigurationValid() const
{
    return GlobalCap > 0 && ShooterCap >= 0 && ChargerCap >= 0
        && ShooterCap <= GlobalCap && ChargerCap <= GlobalCap && MaxGroupSize > 0 && MaxGroupSize <= GlobalCap
        && IsPositive(MinimumGroupGap) && IsPositive(ReservationDelay) && ScreenMargin >= 0
        && IsPositive(MinimumApproachDistance) && IsPositive(ImminentApproachTime) && IsPositive(NavProjectionTolerance);
}

int32 UGirlSaviorWaveSettings::GetRoleCap(EGirlSaviorEnemyRole Role) const
{
    switch (Role)
    {
    case EGirlSaviorEnemyRole::Pursuer: return GlobalCap;
    case EGirlSaviorEnemyRole::Shooter: return ShooterCap;
    case EGirlSaviorEnemyRole::Charger: return ChargerCap;
    }
    return 0;
}

// Intrinsic validation is usable in the Content Browser. The encounter also checks shared limits.
bool UGirlSaviorWaveProfile::IsConfigurationValid(const UGirlSaviorWaveSettings* Settings) const
{
    if (TargetCount <= 0 || RefillThreshold < 0 || RefillThreshold >= TargetCount || !IsPositive(Interval)
        || Recipes.IsEmpty() || (Settings && TargetCount > Settings->GlobalCap)
        || (bHasIntroduction && (!IsKnownRole(IntroductionRole) || (Settings && Settings->GetRoleCap(IntroductionRole) == 0))))
    {
        return false;
    }
    for (const auto& Recipe : Recipes)
    {
        if (Recipe.Roles.IsEmpty() || !Recipe.Roles.Contains(EGirlSaviorEnemyRole::Pursuer)
            || (Settings && Recipe.Roles.Num() > Settings->MaxGroupSize)
            || Recipe.Roles.ContainsByPredicate([](auto Role) { return !IsKnownRole(Role); }))
        {
            return false;
        }
    }
    return true;
}

#if WITH_EDITOR
EDataValidationResult UGirlSaviorWaveSettings::IsDataValid(FDataValidationContext& Context) const
{
    if (IsConfigurationValid())
    {
        return EDataValidationResult::Valid;
    }
    Context.AddError(NSLOCTEXT("GirlSaviorWave", "InvalidSettings", "Check wave limits, intervals and admission distances."));
    return EDataValidationResult::Invalid;
}

EDataValidationResult UGirlSaviorWaveProfile::IsDataValid(FDataValidationContext& Context) const
{
    if (IsConfigurationValid())
    {
        return EDataValidationResult::Valid;
    }
    Context.AddError(NSLOCTEXT("GirlSaviorWave", "InvalidProfile", "Check refill threshold, interval and ordered role recipes."));
    return EDataValidationResult::Invalid;
}
#endif
