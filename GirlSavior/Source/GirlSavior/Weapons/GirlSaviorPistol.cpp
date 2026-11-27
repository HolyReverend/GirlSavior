#include "Weapons/GirlSaviorPistol.h"

// A pistol emits one centered projectile per discharge.
bool UGirlSaviorPistol::ValidateBehavior(const FGirlSaviorWeaponRow& Row, FText& OutError) const
{
    if (Row.ProjectileCount != 1 || Row.PelletStepDegrees != 0.f)
    {
        OutError = NSLOCTEXT("GirlSaviorWeapons", "PistolBehavior",
            "Для пистолета нужны ProjectileCount = 1 и PelletStepDegrees = 0.");
        return false;
    }
    return true;
}
