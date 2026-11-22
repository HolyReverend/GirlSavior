#include "Encounters/GirlSaviorWavePolicy.h"

int32 FGirlSaviorWavePopulation::Get(EGirlSaviorEnemyRole Role) const
{
    switch (Role)
    {
    case EGirlSaviorEnemyRole::Pursuer: return Total - Shooters - Chargers;
    case EGirlSaviorEnemyRole::Shooter: return Shooters;
    case EGirlSaviorEnemyRole::Charger: return Chargers;
    }
    return 0;
}

void FGirlSaviorWavePopulation::Add(EGirlSaviorEnemyRole Role)
{
    ++Total;
    Shooters += Role == EGirlSaviorEnemyRole::Shooter;
    Chargers += Role == EGirlSaviorEnemyRole::Charger;
}

int32 GirlSaviorWavePolicy::GetBudget(const UGirlSaviorWaveSettings& Settings,
    const UGirlSaviorWaveProfile& Profile, int32 Occupied)
{
    return Occupied > Profile.RefillThreshold ? 0 : FMath::Max(0,
        FMath::Min3(Profile.TargetCount - Occupied, Settings.GlobalCap - Occupied, Settings.MaxGroupSize));
}

bool GirlSaviorWavePolicy::TimingAllows(const UGirlSaviorWaveSettings& Settings,
    const UGirlSaviorWaveProfile& Profile, const TOptional<double>& LastCommit, double Now, bool bEmptyWithNoImminentApproach)
{
    if (!LastCommit.IsSet())
    {
        return true;
    }
    const double Elapsed = Now - LastCommit.GetValue();
    return Elapsed >= Settings.MinimumGroupGap
        && (Elapsed >= Profile.Interval || (Profile.bAccelerateEmptyFrame && bEmptyWithNoImminentApproach));
}

bool GirlSaviorWavePolicy::CanAddGroup(const UGirlSaviorWaveSettings& Settings, const UGirlSaviorWaveProfile& Profile,
    FGirlSaviorWavePopulation Live, TConstArrayView<EGirlSaviorEnemyRole> ReservedRoles)
{
    if (ReservedRoles.IsEmpty() || ReservedRoles.Num() > GetBudget(Settings, Profile, Live.Total))
    {
        return false;
    }
    for (const auto Role : ReservedRoles)
    {
        if (Live.Get(Role) >= Settings.GetRoleCap(Role))
        {
            return false;
        }
        Live.Add(Role);
    }
    return true;
}

TArray<EGirlSaviorEnemyRole> GirlSaviorWavePolicy::SelectRepeatRoles(const UGirlSaviorWaveSettings& Settings,
    TConstArrayView<EGirlSaviorEnemyRole> Recipe, int32 Budget, FGirlSaviorWavePopulation Live)
{
    TArray<EGirlSaviorEnemyRole> Selected;
    bool bHasPursuer = false;
    for (const auto Role : Recipe)
    {
        const int32 PlacesLeft = Budget - Selected.Num();
        if (PlacesLeft <= 0)
        {
            break;
        }
        const bool bIsPursuer = Role == EGirlSaviorEnemyRole::Pursuer;
        // Preserve a place for the required Pursuer even when an earlier recipe entry has priority.
        if ((PlacesLeft > 1 || bHasPursuer || bIsPursuer) && Live.Get(Role) < Settings.GetRoleCap(Role))
        {
            Selected.Add(Role);
            Live.Add(Role);
            bHasPursuer |= bIsPursuer;
        }
    }
    if (!bHasPursuer)
    {
        Selected.Reset();
    }
    return Selected;
}
