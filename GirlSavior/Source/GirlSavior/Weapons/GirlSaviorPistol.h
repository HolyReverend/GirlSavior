#pragma once

#include "Weapons/GirlSaviorFirearm.h"
#include "GirlSaviorPistol.generated.h"

UCLASS()
class GIRLSAVIOR_API UGirlSaviorPistol : public UGirlSaviorFirearm
{
    GENERATED_BODY()

public:
    virtual bool ValidateBehavior(const FGirlSaviorWeaponRow& Row, FText& OutError) const override;
};
