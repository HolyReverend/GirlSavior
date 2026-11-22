#include "Enemies/GirlSaviorEnemyDefinition.h"

#include "Combat/GirlSaviorTestProjectile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
bool IsPositiveFinite(float Value)
{
    return FMath::IsFinite(Value) && Value > 0.f;
}

template<typename TuningType>
bool HasValidAttackTimingAndDamage(const TuningType& Tuning)
{
    return IsPositiveFinite(Tuning.Preparation) && IsPositiveFinite(Tuning.LockedTail)
        && Tuning.LockedTail < Tuning.Preparation && IsPositiveFinite(Tuning.Recovery)
        && IsPositiveFinite(Tuning.PhysicalDamage);
}
}

UGirlSaviorEnemyDefinition::UGirlSaviorEnemyDefinition()
{
    Vitals.MaxHealth = 30.f;
}

// The same checks apply in the editor and before an enemy takes part in combat.
bool UGirlSaviorEnemyDefinition::IsValidForSpawn() const
{
    if (!Vitals.IsConfigurationValid() || Vitals.InitialBarrier != 0.f
        || !IsPositiveFinite(WalkSpeed) || !IsPositiveFinite(CapsuleRadius)
        || !IsPositiveFinite(CapsuleHalfHeight) || CapsuleHalfHeight < CapsuleRadius)
    {
        return false;
    }

    switch (Role)
    {
    case EGirlSaviorEnemyRole::Pursuer:
        return HasValidAttackTimingAndDamage(Pursuer)
            && IsPositiveFinite(Pursuer.Reach) && Pursuer.Reach > CapsuleRadius
            && IsPositiveFinite(Pursuer.HalfAngleDegrees) && Pursuer.HalfAngleDegrees < 90.f;
    case EGirlSaviorEnemyRole::Shooter:
        return HasValidAttackTimingAndDamage(Shooter)
            && IsPositiveFinite(Shooter.PreferredDistance) && Shooter.PreferredDistance > CapsuleRadius
            && IsPositiveFinite(Shooter.Range) && Shooter.Range >= Shooter.PreferredDistance
            && IsPositiveFinite(Shooter.ProjectileRadius) && Shooter.ProjectileRadius < CapsuleRadius
            && IsPositiveFinite(Shooter.ProjectileSpeed) && IsPositiveFinite(Shooter.ProjectileRange)
            && Shooter.ProjectileRange >= Shooter.Range
            && FMath::IsFinite(4.f * (Shooter.ProjectileRange / Shooter.ProjectileSpeed) + 0.25f)
            && Shooter.ProjectileClass && !Shooter.ProjectileClass->HasAnyClassFlags(CLASS_Abstract);
    case EGirlSaviorEnemyRole::Charger:
        return HasValidAttackTimingAndDamage(Charger)
            && IsPositiveFinite(Charger.MinRange) && Charger.MinRange > CapsuleRadius
            && IsPositiveFinite(Charger.MaxRange) && Charger.MaxRange > Charger.MinRange
            && IsPositiveFinite(Charger.ChargeSpeed) && IsPositiveFinite(Charger.MaxTravel)
            && Charger.MaxTravel >= Charger.MaxRange - CapsuleRadius;
    }
    return false;
}

#if WITH_EDITOR
EDataValidationResult UGirlSaviorEnemyDefinition::IsDataValid(FDataValidationContext& Context) const
{
    if (!IsValidForSpawn())
    {
        Context.AddError(NSLOCTEXT("GirlSaviorEnemyDefinition", "InvalidEnemy",
            "An enemy requires valid vitals, body dimensions, movement and role-specific attack tuning."));
        return EDataValidationResult::Invalid;
    }
    return EDataValidationResult::Valid;
}
#endif
