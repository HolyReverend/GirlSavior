#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Misc/Optional.h"
#include "Weapons/GirlSaviorWeaponDefinition.h"
#include "GirlSaviorFirearm.generated.h"

class AGirlSaviorCharacter;
class AGirlSaviorCombatEncounter;

struct FGirlSaviorWeaponFireContext
{
    AGirlSaviorCharacter* Hero = nullptr;
    AGirlSaviorCombatEncounter* Encounter = nullptr;
    FVector Origin = FVector::ZeroVector;
    FVector AimDirection = FVector::ZeroVector;
    double Now = 0.0;
};

struct FGirlSaviorWeaponShotCue
{
    FName WeaponId;
    int64 ShotGroupId = 0;
    FVector Origin = FVector::ZeroVector;
    FVector AimDirection = FVector::ZeroVector;
    FGirlSaviorWeaponPresentation Presentation;
};

struct FGirlSaviorPreparedDischarge
{
    TArray<FVector, TInlineAllocator<5>> Directions;
    FRandomStream RandomStream;
    int32 TightShotsUsed = 0;
};

UCLASS(Abstract, Transient)
class GIRLSAVIOR_API UGirlSaviorFirearm : public UObject
{
    GENERATED_BODY()

public:
    // The caller validates the row before creating the instance.
    virtual void Initialize(FName InWeaponId, const FGirlSaviorWeaponRow& InDefinition, TOptional<int32> RandomSeed = {});

    const FGirlSaviorWeaponRow& GetDefinition() const { return Definition; }
    FName GetWeaponId() const { return WeaponId; }

    // Write a cue and commit weapon-local state only after encountering a consumed projectile.
    bool TryFire(const FGirlSaviorWeaponFireContext& Context, FGirlSaviorWeaponShotCue& OutCue);

    // Called on the class default object when validating authored data.
    virtual bool ValidateBehavior(const FGirlSaviorWeaponRow& Row, FText& OutError) const;

protected:
    virtual void BuildShotDirections(const FGirlSaviorWeaponFireContext& Context,
        FGirlSaviorPreparedDischarge& OutDischarge) const;
    virtual void OnShotCommitted(const FGirlSaviorPreparedDischarge& Discharge, double Now);

private:
    friend class FGirlSaviorWeaponCombatTestAccess;

    UPROPERTY(Transient)
    FGirlSaviorWeaponRow Definition;

    FName WeaponId;
};
