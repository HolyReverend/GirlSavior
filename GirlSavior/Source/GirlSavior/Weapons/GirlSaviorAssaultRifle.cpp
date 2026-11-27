#include "Weapons/GirlSaviorAssaultRifle.h"

void UGirlSaviorAssaultRifle::Initialize(FName InWeaponId, const FGirlSaviorWeaponRow& InDefinition,
    TOptional<int32> RandomSeed)
{
    Super::Initialize(InWeaponId, InDefinition, RandomSeed);
    if (RandomSeed.IsSet())
    {
        RandomStream.Initialize(RandomSeed.GetValue());
    }
    else
    {
        RandomStream.GenerateNewSeed();
    }
    LastShotTime = 0.0;
    TightShotsUsed = 0;
}

bool UGirlSaviorAssaultRifle::ValidateBehavior(const FGirlSaviorWeaponRow& Row, FText& OutError) const
{
    if (Row.ProjectileCount != 1 || Row.PelletStepDegrees != 0.f)
    {
        OutError = NSLOCTEXT("GirlSaviorWeapons", "RifleProjectileCount",
            "Для винтовки нужны ProjectileCount = 1 и PelletStepDegrees = 0.");
        return false;
    }
    if (Row.TightShotCount <= 0 || Row.SpreadResetSeconds <= 0.f)
    {
        OutError = NSLOCTEXT("GirlSaviorWeapons", "RifleSpreadReset",
            "Для серии винтовки нужны положительные TightShotCount и SpreadResetSeconds.");
        return false;
    }
    if (Row.SustainedSpreadDegrees < Row.TightSpreadDegrees)
    {
        OutError = NSLOCTEXT("GirlSaviorWeapons", "RifleSpreadBounds",
            "SustainedSpreadDegrees должен быть не меньше TightSpreadDegrees.");
        return false;
    }
    return true;
}

// Generate against a copy so a rejected launch cannot change the next valid shot.
void UGirlSaviorAssaultRifle::BuildShotDirections(const FGirlSaviorWeaponFireContext& Context,
    FGirlSaviorPreparedDischarge& OutDischarge) const
{
    const FGirlSaviorWeaponRow& Row = GetDefinition();
    const bool bReset = Context.Now >= LastShotTime + Row.SpreadResetSeconds;
    const int32 Used = bReset ? 0 : TightShotsUsed;
    const bool bTight = Used < Row.TightShotCount;
    const float Spread = bTight ? Row.TightSpreadDegrees : Row.SustainedSpreadDegrees;

    OutDischarge.RandomStream = RandomStream;
    OutDischarge.TightShotsUsed = bTight ? Used + 1 : Used;
    const double Yaw = OutDischarge.RandomStream.FRandRange(-Spread, Spread);
    OutDischarge.Directions.Add(Context.AimDirection.RotateAngleAxis(Yaw, FVector::UpVector));
}

void UGirlSaviorAssaultRifle::OnShotCommitted(const FGirlSaviorPreparedDischarge& Discharge, double Now)
{
    RandomStream = Discharge.RandomStream;
    TightShotsUsed = Discharge.TightShotsUsed;
    LastShotTime = Now;
}
