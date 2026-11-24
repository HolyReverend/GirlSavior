#include "Support/GirlSaviorSupportRun.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/World.h"
#include "Support/GirlSaviorSupportAbilityDefinition.h"
#include "Support/GirlSaviorSupportEffectDefinition.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "GirlSaviorSupportRun"

// Build a new set of immutable tuning snapshots before touching a world actor.
bool UGirlSaviorSupportRun::Configure(TConstArrayView<const UGirlSaviorSupportAbilityDefinition*> Abilities, FText& OutError)
{
    if (!Effects.IsEmpty() || Abilities.Num() != 2 || !IsValid(Abilities[0]) || !IsValid(Abilities[1]))
    {
        OutError = LOCTEXT("InvalidLoadout", "Не удалось загрузить два умения поддержки.");
        return false;
    }
    if (!UGirlSaviorSupportAbilityDefinition::ValidatePair(*Abilities[0], *Abilities[1], OutError))
    {
        return false;
    }
    for (const auto* Ability : Abilities)
    {
        for (const UGirlSaviorSupportEffectDefinition* Definition : Ability->Effects)
        {
            if (!Definition->CreateRuntime(*this, OutError))
            {
                Effects.Reset();
                return false;
            }
        }
    }
    return true;
}

// The encounter supplies an initialized hero. Roll back any partial install on failure.
bool UGirlSaviorSupportRun::Attach(AGirlSaviorCharacter& Hero, FText& OutError)
{
    if (bDispatchOpen || Effects.IsEmpty())
    {
        OutError = LOCTEXT("InvalidInstall", "Не удалось подготовить умения поддержки.");
        return false;
    }
    for (UGirlSaviorSupportEffectRuntime* Effect : Effects)
    {
        if (!Effect->Attach(Hero, OutError))
        {
            Detach();
            return false;
        }
    }
    bDispatchOpen = true;
    return true;
}

// Gate callbacks before resetting live tuning, timers and subscriptions.
void UGirlSaviorSupportRun::Detach()
{
    CloseDispatch();
    for (UGirlSaviorSupportEffectRuntime* Effect : Effects)
    {
        Effect->Detach();
    }
}

// The encounter already checks that the target is a registered enemy and belongs to this run.
void UGirlSaviorSupportRun::NotifyHeroWeaponDamage(AGirlSaviorEnemyCharacter& Enemy,
    const FGirlSaviorDamageResult& Result)
{
    if (!bDispatchOpen || Result.Outcome != EGirlSaviorDamageOutcome::Applied || Result.bKilled
        || Result.Spec.SourceFaction != EGirlSaviorFaction::Hero || Result.Spec.SourceId != TEXT("Hero.Pistol")
        || Result.Spec.Delivery != EGirlSaviorDamageDelivery::DirectProjectile
        || Result.HealthLost + Result.BarrierLost <= 0.0 || !Enemy.GetDamageReceiver()->IsAlive())
    {
        return;
    }
    for (UGirlSaviorSupportEffectRuntime* Effect : Effects)
    {
        Effect->OnHeroWeaponDamage(Enemy);
    }
}

// Retirement may happen while the terminal dispatch gate is closed.
void UGirlSaviorSupportRun::ForgetEnemy(AGirlSaviorEnemyCharacter& Enemy)
{
    for (UGirlSaviorSupportEffectRuntime* Effect : Effects)
    {
        Effect->ForgetEnemy(Enemy);
    }
}

// A successful hit refreshes one game-time timer for its victim without stacking.
void UGirlSaviorWeaponHitSlowRuntime::OnHeroWeaponDamage(AGirlSaviorEnemyCharacter& Enemy)
{
    UWorld* World = Enemy.GetWorld();
    if (!World)
    {
        return;
    }
    EffectWorld = World;
    const TWeakObjectPtr<AGirlSaviorEnemyCharacter> WeakEnemy(&Enemy);
    Enemy.SetSupportWalkSpeedMultiplier(1.f - SlowFraction);
    World->GetTimerManager().SetTimer(Slows.FindOrAdd(WeakEnemy),
        FTimerDelegate::CreateWeakLambda(this, [this, WeakEnemy] { ClearEnemySlow(WeakEnemy); }),
        DurationSeconds, false);
}

// Forget an affected enemy even if it was destroyed just before the timer expired.
void UGirlSaviorWeaponHitSlowRuntime::ClearEnemySlow(TWeakObjectPtr<AGirlSaviorEnemyCharacter> Enemy)
{
    FTimerHandle Timer;
    if (Slows.RemoveAndCopyValue(Enemy, Timer))
    {
        if (auto* World = EffectWorld.Get())
        {
            World->GetTimerManager().ClearTimer(Timer);
        }
        if (auto* Target = Enemy.Get())
        {
            Target->ClearSupportWalkSpeedMultiplier();
        }
    }
}

// Remove the modifier as soon as an enemy leaves the encounter.
void UGirlSaviorWeaponHitSlowRuntime::ForgetEnemy(AGirlSaviorEnemyCharacter& Enemy)
{
    ClearEnemySlow(&Enemy);
}

// Stop every pending timer and restore the authored speeds of surviving targets.
void UGirlSaviorWeaponHitSlowRuntime::Detach()
{
    while (!Slows.IsEmpty())
    {
        ClearEnemySlow(Slows.CreateConstIterator().Key());
    }
    EffectWorld.Reset();
}

// The fire component validates the effective rate against the equipped pistol.
bool UGirlSaviorWeaponRateRuntime::Attach(AGirlSaviorCharacter& Hero, FText& OutError)
{
    if (auto* Component = Hero.GetFireComponent(); Component && Component->SetSupportRateMultiplier(RateMultiplier))
    {
        Fire = Component;
        return true;
    }
    OutError = LOCTEXT("InvalidPistolRate", "Не удалось применить усиление темпа стрельбы.");
    return false;
}

// Reset future intervals without moving an already-committed shot deadline.
void UGirlSaviorWeaponRateRuntime::Detach()
{
    if (auto* Component = Fire.Get())
    {
        Component->SetSupportRateMultiplier(1.f);
    }
    Fire.Reset();
}

// Install the full support pool after the encounter initializes the hero's baseline vitals.
bool UGirlSaviorRechargeBarrierRuntime::Attach(AGirlSaviorCharacter& Hero, FText& OutError)
{
    auto* Component = Hero.GetDamageReceiver();
    UWorld* World = Hero.GetWorld();
    if (!Component || !World || !Component->InstallSupportBarrier(Component->GetMaxHealth() * HealthFraction))
    {
        OutError = LOCTEXT("InvalidBarrier", "Не удалось применить барьер поддержки.");
        return false;
    }
    Receiver = Component;
    EffectWorld = World;
    Component->OnDamageResolved.AddUniqueDynamic(this, &ThisClass::OnDamageResolved);
    return true;
}

// Every committed loss restarts the game-time wait, even when the barrier absorbed the entire hit.
void UGirlSaviorRechargeBarrierRuntime::OnDamageResolved(const FGirlSaviorDamageResult& Result)
{
    auto* World = EffectWorld.Get();
    if (World && GetTypedOuter<UGirlSaviorSupportRun>()->IsDispatchOpen()
        && Result.Outcome == EGirlSaviorDamageOutcome::Applied && !Result.bKilled
        && Result.HealthLost + Result.BarrierLost > 0.0)
    {
        World->GetTimerManager().SetTimer(RechargeTimer, this, &ThisClass::Recharge, RechargeDelaySeconds, false);
    }
}

// Do not refill between terminal death admission and deferred effect teardown.
void UGirlSaviorRechargeBarrierRuntime::Recharge()
{
    if (auto* Component = Receiver.Get(); Component && GetTypedOuter<UGirlSaviorSupportRun>()->IsDispatchOpen())
    {
        Component->RefillSupportBarrier();
    }
}

// Cancel the refill and disconnect before changing the receiver's displayed barrier cap.
void UGirlSaviorRechargeBarrierRuntime::Detach()
{
    if (auto* World = EffectWorld.Get())
    {
        World->GetTimerManager().ClearTimer(RechargeTimer);
    }
    if (auto* Component = Receiver.Get())
    {
        Component->OnDamageResolved.RemoveDynamic(this, &ThisClass::OnDamageResolved);
        Component->RemoveSupportBarrier();
    }
    Receiver.Reset();
    EffectWorld.Reset();
}

#undef LOCTEXT_NAMESPACE
