#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GirlSaviorSupportEffectDefinition.generated.h"

class UGirlSaviorSupportEffectRuntime;
class UGirlSaviorSupportRun;

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class GIRLSAVIOR_API UGirlSaviorSupportEffectDefinition : public UObject
{
    GENERATED_BODY()

public:
    // Report the first invalid field. A valid definition leaves OutError untouched.
    virtual bool Validate(FText& OutError) const PURE_VIRTUAL(UGirlSaviorSupportEffectDefinition::Validate, return false;);
    virtual FName GetEffectKey() const PURE_VIRTUAL(UGirlSaviorSupportEffectDefinition::GetEffectKey, return NAME_None;);
    virtual FText BuildDetailsText() const PURE_VIRTUAL(UGirlSaviorSupportEffectDefinition::BuildDetailsText, return FText::GetEmpty(););

    // Snapshot validated tuning and keep the new runtime alive through the run.
    UGirlSaviorSupportEffectRuntime* CreateRuntime(UGirlSaviorSupportRun& Run, FText& OutError) const;

protected:
    virtual UGirlSaviorSupportEffectRuntime* CreateValidatedRuntime(UGirlSaviorSupportRun& Run) const
        PURE_VIRTUAL(UGirlSaviorSupportEffectDefinition::CreateValidatedRuntime, return nullptr;);
};

UCLASS(meta=(DisplayName="Weapon Hit Slow"))
class GIRLSAVIOR_API UGirlSaviorWeaponHitSlowEffectDefinition : public UGirlSaviorSupportEffectDefinition
{
    GENERATED_BODY()

public:
    // Доля снижения скорости ходьбы врага после попадания оружием, от 0 до 1 без крайних значений.
    UPROPERTY(EditAnywhere, Category="Support", meta=(UIMin="0", UIMax="1"))
    float SlowFraction = 0.f;

    // Длительность замедления в секундах игрового времени. Новое попадание обновляет срок.
    UPROPERTY(EditAnywhere, Category="Support", meta=(UIMin="0", Units="s"))
    float DurationSeconds = 0.f;

    virtual bool Validate(FText& OutError) const override;
    virtual FName GetEffectKey() const override;
    virtual FText BuildDetailsText() const override;

protected:
    virtual UGirlSaviorSupportEffectRuntime* CreateValidatedRuntime(UGirlSaviorSupportRun& Run) const override;
};

UCLASS(meta=(DisplayName="Weapon Rate"))
class GIRLSAVIOR_API UGirlSaviorWeaponRateEffectDefinition : public UGirlSaviorSupportEffectDefinition
{
    GENERATED_BODY()

public:
    // Множитель базового темпа выстрелов. Значение 1 сохраняет исходный темп оружия.
    UPROPERTY(EditAnywhere, Category="Support", meta=(ClampMin="1"))
    float RateMultiplier = 1.f;

    virtual bool Validate(FText& OutError) const override;
    virtual FName GetEffectKey() const override;
    virtual FText BuildDetailsText() const override;

protected:
    virtual UGirlSaviorSupportEffectRuntime* CreateValidatedRuntime(UGirlSaviorSupportRun& Run) const override;
};

UCLASS(meta=(DisplayName="Recharge Barrier"))
class GIRLSAVIOR_API UGirlSaviorRechargeBarrierEffectDefinition : public UGirlSaviorSupportEffectDefinition
{
    GENERATED_BODY()

public:
    // Доля максимального здоровья героя, определяющая ёмкость барьера. Значение 1 равно полному здоровью.
    UPROPERTY(EditAnywhere, Category="Support", meta=(UIMin="0", ClampMax="1"))
    float HealthFraction = 0.f;

    // Время без урона в секундах игрового времени, после которого барьер пополняется.
    UPROPERTY(EditAnywhere, Category="Support", meta=(UIMin="0", Units="s"))
    float RechargeDelaySeconds = 0.f;

    virtual bool Validate(FText& OutError) const override;
    virtual FName GetEffectKey() const override;
    virtual FText BuildDetailsText() const override;

protected:
    virtual UGirlSaviorSupportEffectRuntime* CreateValidatedRuntime(UGirlSaviorSupportRun& Run) const override;
};
