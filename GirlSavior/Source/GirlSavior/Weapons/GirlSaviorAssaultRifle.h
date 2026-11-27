#pragma once

#include "Weapons/GirlSaviorFirearm.h"
#include "GirlSaviorAssaultRifle.generated.h"

UCLASS()
class GIRLSAVIOR_API UGirlSaviorAssaultRifle : public UGirlSaviorFirearm
{
    GENERATED_BODY()

public:
    virtual void Initialize(FName InWeaponId, const FGirlSaviorWeaponRow& InDefinition,
        TOptional<int32> RandomSeed = {}) override;

    virtual bool ValidateBehavior(const FGirlSaviorWeaponRow& Row, FText& OutError) const override;

protected:
    virtual void BuildShotDirections(const FGirlSaviorWeaponFireContext& Context,
        FGirlSaviorPreparedDischarge& OutDischarge) const override;
    virtual void OnShotCommitted(const FGirlSaviorPreparedDischarge& Discharge, double Now) override;

private:
    FRandomStream RandomStream;
    double LastShotTime = 0.0;
    int32 TightShotsUsed = 0;
};
