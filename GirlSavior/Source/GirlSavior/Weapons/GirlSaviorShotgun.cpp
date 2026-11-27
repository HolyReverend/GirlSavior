#include "Weapons/GirlSaviorShotgun.h"

bool UGirlSaviorShotgun::ValidateBehavior(const FGirlSaviorWeaponRow& Row, FText& OutError) const
{
    if (Row.ProjectileCount != 3 && Row.ProjectileCount != 5)
    {
        OutError = NSLOCTEXT("GirlSaviorWeapons", "ShotgunPelletCount",
            "Для дробовика нужен нечётный ProjectileCount от 3 до 5.");
        return false;
    }
    if (Row.PelletStepDegrees <= 0.f)
    {
        OutError = NSLOCTEXT("GirlSaviorWeapons", "ShotgunPelletStep",
            "Для веера дробовика нужен положительный PelletStepDegrees.");
        return false;
    }
    return true;
}

// Put one pellet at the center and match each left-hand offset with one on the right.
void UGirlSaviorShotgun::BuildShotDirections(const FGirlSaviorWeaponFireContext& Context,
    FGirlSaviorPreparedDischarge& OutDischarge) const
{
    const FGirlSaviorWeaponRow& Row = GetDefinition();
    const int32 Center = (Row.ProjectileCount - 1) / 2;
    for (int32 Index = 0; Index < Row.ProjectileCount; ++Index)
    {
        const double Yaw = double(Index - Center) * Row.PelletStepDegrees;
        OutDischarge.Directions.Add(Context.AimDirection.RotateAngleAxis(Yaw, FVector::UpVector));
    }
}
