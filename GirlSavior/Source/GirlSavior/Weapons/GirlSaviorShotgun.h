#pragma once

#include "Weapons/GirlSaviorFirearm.h"
#include "GirlSaviorShotgun.generated.h"

UCLASS()
class GIRLSAVIOR_API UGirlSaviorShotgun : public UGirlSaviorFirearm
{
    GENERATED_BODY()

public:
    virtual bool ValidateBehavior(const FGirlSaviorWeaponRow& Row, FText& OutError) const override;

protected:
    virtual void BuildShotDirections(const FGirlSaviorWeaponFireContext& Context,
        FGirlSaviorPreparedDischarge& OutDischarge) const override;
};
