#pragma once

#include "Encounters/GirlSaviorWaveSettings.h"

// Live and reserved places share the same budget. Pass the live count alone when committing its own reservation.
struct FGirlSaviorWavePopulation
{
    int32 Total = 0;
    int32 Shooters = 0;
    int32 Chargers = 0;

    int32 Get(EGirlSaviorEnemyRole Role) const;
    void Add(EGirlSaviorEnemyRole Role);
};

namespace GirlSaviorWavePolicy
{
int32 GetBudget(const UGirlSaviorWaveSettings& Settings, const UGirlSaviorWaveProfile& Profile, int32 Occupied);
bool TimingAllows(const UGirlSaviorWaveSettings& Settings, const UGirlSaviorWaveProfile& Profile,
    const TOptional<double>& LastCommit, double Now, bool bEmptyWithNoImminentApproach);
bool CanAddGroup(const UGirlSaviorWaveSettings& Settings, const UGirlSaviorWaveProfile& Profile,
    FGirlSaviorWavePopulation Live, TConstArrayView<EGirlSaviorEnemyRole> ReservedRoles);
TArray<EGirlSaviorEnemyRole> SelectRepeatRoles(const UGirlSaviorWaveSettings& Settings,
    TConstArrayView<EGirlSaviorEnemyRole> Recipe, int32 Budget, FGirlSaviorWavePopulation Live);
}
