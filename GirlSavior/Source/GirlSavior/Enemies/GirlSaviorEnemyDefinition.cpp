#include "Enemies/GirlSaviorEnemyDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UGirlSaviorEnemyDefinition::UGirlSaviorEnemyDefinition()
{
    Vitals.MaxHealth = 30.f;
}

// The same checks apply in the editor and before an enemy takes part in combat.
bool UGirlSaviorEnemyDefinition::IsValidForSpawn() const
{
    const auto Positive = [](const float Value) { return FMath::IsFinite(Value) && Value > 0.f; };
    return Role == EGirlSaviorEnemyRole::Pursuer
        && Vitals.IsConfigurationValid() && Vitals.InitialBarrier == 0.f
        && Positive(WalkSpeed) && Positive(CapsuleRadius) && Positive(CapsuleHalfHeight)
        && CapsuleHalfHeight >= CapsuleRadius
        && Positive(Pursuer.Reach) && Pursuer.Reach > CapsuleRadius
        && Positive(Pursuer.HalfAngleDegrees) && Pursuer.HalfAngleDegrees < 90.f
        && Positive(Pursuer.Preparation) && Positive(Pursuer.LockedTail)
        && Pursuer.LockedTail < Pursuer.Preparation && Positive(Pursuer.Recovery)
        && Positive(Pursuer.PhysicalDamage);
}

#if WITH_EDITOR
EDataValidationResult UGirlSaviorEnemyDefinition::IsDataValid(FDataValidationContext& Context) const
{
    if (!IsValidForSpawn())
    {
        Context.AddError(NSLOCTEXT("GirlSaviorEnemyDefinition", "InvalidPursuer",
            "The Pursuer requires valid vitals, body dimensions, speed and attack tuning."));
        return EDataValidationResult::Invalid;
    }
    return EDataValidationResult::Valid;
}
#endif
