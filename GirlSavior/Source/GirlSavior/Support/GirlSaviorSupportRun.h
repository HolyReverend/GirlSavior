#pragma once

#include "CoreMinimal.h"
#include "Combat/GirlSaviorDamageTypes.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "GirlSaviorSupportRun.generated.h"

class AGirlSaviorCharacter;
class AGirlSaviorEnemyCharacter;
class UGirlSaviorDamageReceiverComponent;
class UGirlSaviorFireComponent;
class UGirlSaviorSupportAbilityDefinition;
class UWorld;

UCLASS(Abstract, Transient)
class GIRLSAVIOR_API UGirlSaviorSupportEffectRuntime : public UObject
{
    GENERATED_BODY()

public:
    virtual bool Attach(AGirlSaviorCharacter& Hero, FText& OutError) { return true; }
    virtual void Detach() {}
    virtual void OnHeroWeaponDamage(AGirlSaviorEnemyCharacter& Enemy) {}
    virtual void ForgetEnemy(AGirlSaviorEnemyCharacter& Enemy) {}
};

UCLASS(Transient)
class GIRLSAVIOR_API UGirlSaviorWeaponHitSlowRuntime : public UGirlSaviorSupportEffectRuntime
{
    GENERATED_BODY()

public:
    float SlowFraction = 0.f;
    float DurationSeconds = 0.f;
    virtual void Detach() override;
    virtual void OnHeroWeaponDamage(AGirlSaviorEnemyCharacter& Enemy) override;
    virtual void ForgetEnemy(AGirlSaviorEnemyCharacter& Enemy) override;

private:
    void ClearEnemySlow(TWeakObjectPtr<AGirlSaviorEnemyCharacter> Enemy);

    TMap<TWeakObjectPtr<AGirlSaviorEnemyCharacter>, FTimerHandle> Slows;
    TWeakObjectPtr<UWorld> EffectWorld;
};

UCLASS(Transient)
class GIRLSAVIOR_API UGirlSaviorWeaponRateRuntime : public UGirlSaviorSupportEffectRuntime
{
    GENERATED_BODY()

public:
    float RateMultiplier = 1.f;
    virtual bool Attach(AGirlSaviorCharacter& Hero, FText& OutError) override;
    virtual void Detach() override;

private:
    TWeakObjectPtr<UGirlSaviorFireComponent> Fire;
};

UCLASS(Transient)
class GIRLSAVIOR_API UGirlSaviorRechargeBarrierRuntime : public UGirlSaviorSupportEffectRuntime
{
    GENERATED_BODY()

public:
    float HealthFraction = 0.f;
    float RechargeDelaySeconds = 0.f;
    virtual bool Attach(AGirlSaviorCharacter& Hero, FText& OutError) override;
    virtual void Detach() override;

private:
    UFUNCTION()
    void OnDamageResolved(const FGirlSaviorDamageResult& Result);
    void Recharge();

    TWeakObjectPtr<UGirlSaviorDamageReceiverComponent> Receiver;
    TWeakObjectPtr<UWorld> EffectWorld;
    FTimerHandle RechargeTimer;
};

UCLASS(Transient)
class GIRLSAVIOR_API UGirlSaviorSupportRun : public UObject
{
    GENERATED_BODY()

public:
    bool Configure(TConstArrayView<const UGirlSaviorSupportAbilityDefinition*> Abilities, FText& OutError);
    bool Attach(AGirlSaviorCharacter& Hero, FText& OutError);
    void CloseDispatch() { bDispatchOpen = false; }
    void Detach();
    void NotifyHeroWeaponDamage(AGirlSaviorEnemyCharacter& Enemy, const FGirlSaviorDamageResult& Result);
    void ForgetEnemy(AGirlSaviorEnemyCharacter& Enemy);
    bool IsDispatchOpen() const { return bDispatchOpen; }

private:
    friend class UGirlSaviorSupportEffectDefinition;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGirlSaviorSupportEffectRuntime>> Effects;
    bool bDispatchOpen = false;
};
