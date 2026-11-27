#include "Weapons/GirlSaviorFirearm.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Encounters/GirlSaviorCombatEncounter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorFirearm, Log, All);

void UGirlSaviorFirearm::Initialize(FName InWeaponId, const FGirlSaviorWeaponRow& InDefinition, TOptional<int32> RandomSeed)
{
    WeaponId = InWeaponId;
    Definition = InDefinition;
}

bool UGirlSaviorFirearm::ValidateBehavior(const FGirlSaviorWeaponRow& Row, FText& OutError) const
{
    return true;
}

// Center the default discharge on the current aim sample.
void UGirlSaviorFirearm::BuildShotDirections(const FGirlSaviorWeaponFireContext& Context,
    FGirlSaviorPreparedDischarge& OutDischarge) const
{
    OutDischarge.Directions.Add(Context.AimDirection);
}

void UGirlSaviorFirearm::OnShotCommitted(const FGirlSaviorPreparedDischarge& Discharge, double Now)
{
}

bool UGirlSaviorFirearm::TryFire(const FGirlSaviorWeaponFireContext& Context, FGirlSaviorWeaponShotCue& OutCue)
{
    if (WeaponId.IsNone() || !IsValid(Context.Hero) || !IsValid(Context.Encounter))
    {
        return false;
    }
    UWorld* World = Context.Hero->GetWorld();
    if (!World || Context.AimDirection.ContainsNaN() || Context.Origin.ContainsNaN()
        || !FMath::IsFinite(Context.Now))
    {
        return false;
    }

    FGirlSaviorWeaponFireContext AimedContext = Context;
    AimedContext.AimDirection = Context.AimDirection.GetSafeNormal2D();
    if (AimedContext.AimDirection.IsNearlyZero())
    {
        return false;
    }
    FGirlSaviorPreparedDischarge Discharge;
    BuildShotDirections(AimedContext, Discharge);
    if (Discharge.Directions.Num() != Definition.ProjectileCount)
    {
        return false;
    }

    FGirlSaviorProjectileLaunch Launch;
    Launch.Radius = Definition.ProjectileRadius;
    Launch.Speed = Definition.ProjectileSpeed;
    Launch.MaxDistance = Definition.MaxDistance;
    Launch.bApplyDamageFalloff = true;
    Launch.FullDamageDistance = Definition.FullDamageDistance;
    const auto& Presentation = Definition.Presentation;
    Launch.Trail = FGirlSaviorProjectileTrail{Presentation.TrailLength, Presentation.TrailWidth,
        Presentation.TrailFadeSeconds, Presentation.TrailColor};
    Launch.Damage.Physical = Definition.DamagePerProjectile;
    Launch.Damage.Delivery = EGirlSaviorDamageDelivery::DirectProjectile;
    Launch.Damage.SourceFaction = EGirlSaviorFaction::Hero;
    Launch.Damage.SourceId = FName(*FString::Printf(TEXT("Hero.%s"), *WeaponId.ToString()));
    Launch.Damage.WeaponId = WeaponId;
    Launch.Damage.Cause = Definition.DamageCause;
    Launch.Damage.Instigator = Context.Hero;
    Launch.Damage.Origin = Context.Origin;

    const int64 ShotGroupId = Context.Encounter->AllocateAttackId();
    Launch.Damage.ShotGroupId = ShotGroupId;
    int32 LaunchedCount = 0;
    for (int32 Index = 0; Index < Discharge.Directions.Num(); ++Index)
    {
        Launch.Damage.AttackId = Discharge.Directions.Num() == 1 ? ShotGroupId : Context.Encounter->AllocateAttackId();
        if (AGirlSaviorTestProjectile::SpawnCombatProjectile(*World, Definition.ProjectileClass, Context.Hero,
            Discharge.Directions[Index], Launch, *Context.Encounter))
        {
            ++LaunchedCount;
        }
    }
    if (LaunchedCount == 0)
    {
        return false;
    }

    if (LaunchedCount != Discharge.Directions.Num())
    {
        UE_LOG(LogGirlSaviorFirearm, Warning, TEXT("Weapon %s launched %d of %d projectiles."),
            *WeaponId.ToString(), LaunchedCount, Discharge.Directions.Num());
    }

    OnShotCommitted(Discharge, Context.Now);
    OutCue.WeaponId = WeaponId;
    OutCue.ShotGroupId = ShotGroupId;
    OutCue.Origin = Context.Origin;
    OutCue.AimDirection = AimedContext.AimDirection;
    OutCue.Presentation = Definition.Presentation;
    return true;
}
