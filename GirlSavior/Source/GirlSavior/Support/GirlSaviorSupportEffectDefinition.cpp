#include "Support/GirlSaviorSupportEffectDefinition.h"

#include "Support/GirlSaviorSupportRun.h"
#include "Support/GirlSaviorSupportValidation.h"

#define LOCTEXT_NAMESPACE "GirlSaviorSupportEffects"

using GirlSavior::SupportValidation::Fail;

// Make a separate snapshot for every run. The source Data Asset stays read-only.
UGirlSaviorSupportEffectRuntime* UGirlSaviorSupportEffectDefinition::CreateRuntime(
    UGirlSaviorSupportRun& Run, FText& OutError) const
{
    if (!Validate(OutError))
    {
        return nullptr;
    }

    UGirlSaviorSupportEffectRuntime* Runtime = CreateValidatedRuntime(Run);
    Run.Effects.Add(Runtime);
    return Runtime;
}

// The slow must leave a positive walking speed and have a finite expiry.
bool UGirlSaviorWeaponHitSlowEffectDefinition::Validate(FText& OutError) const
{
    if (!FMath::IsFinite(SlowFraction) || SlowFraction <= 0.f || SlowFraction >= 1.f)
    {
        return Fail(*this, TEXT("SlowFraction"), LOCTEXT("SlowRange", "Use a finite value greater than 0 and less than 1."), OutError);
    }
    if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.f)
    {
        return Fail(*this, TEXT("DurationSeconds"), LOCTEXT("SlowDurationRange", "Use a finite duration greater than 0."), OutError);
    }
    return true;
}

// This key identifies the mechanic when checking for conflicts within or between cards.
FName UGirlSaviorWeaponHitSlowEffectDefinition::GetEffectKey() const
{
    return TEXT("WeaponHitSlow");
}

// Convert the authored fraction to a localized percentage for the card.
FText UGirlSaviorWeaponHitSlowEffectDefinition::BuildDetailsText() const
{
    return FText::Format(LOCTEXT("SlowDetails", "Скорость врага −{0} на {1} с"),
        FText::AsPercent(SlowFraction), FText::AsNumber(DurationSeconds));
}

// Snapshot the walking speed modifier and its game-time duration.
UGirlSaviorSupportEffectRuntime* UGirlSaviorWeaponHitSlowEffectDefinition::CreateValidatedRuntime(
    UGirlSaviorSupportRun& Run) const
{
    auto* Runtime = NewObject<UGirlSaviorWeaponHitSlowRuntime>(&Run);
    Runtime->SlowFraction = SlowFraction;
    Runtime->DurationSeconds = DurationSeconds;
    return Runtime;
}

// A rate multiplier of one leaves the base rate intact.
bool UGirlSaviorWeaponRateEffectDefinition::Validate(FText& OutError) const
{
    return (FMath::IsFinite(RateMultiplier) && RateMultiplier >= 1.f)
        || Fail(*this, TEXT("RateMultiplier"), LOCTEXT("RateRange", "Use a finite multiplier of at least 1."), OutError);
}

// Keep the cadence conflict key independent of the authored ability ID.
FName UGirlSaviorWeaponRateEffectDefinition::GetEffectKey() const
{
    return TEXT("WeaponRate");
}

// Present the change to the base rate rather than the total rate.
FText UGirlSaviorWeaponRateEffectDefinition::BuildDetailsText() const
{
    return FText::Format(LOCTEXT("RateDetails", "Темп стрельбы +{0}"),
        FText::AsPercent(RateMultiplier - 1.f));
}

// Snapshot the cadence multiplier used for this expedition.
UGirlSaviorSupportEffectRuntime* UGirlSaviorWeaponRateEffectDefinition::CreateValidatedRuntime(
    UGirlSaviorSupportRun& Run) const
{
    auto* Runtime = NewObject<UGirlSaviorWeaponRateRuntime>(&Run);
    Runtime->RateMultiplier = RateMultiplier;
    return Runtime;
}

// Cap the prototype barrier at the hero's maximum health and require a recharge delay.
bool UGirlSaviorRechargeBarrierEffectDefinition::Validate(FText& OutError) const
{
    if (!FMath::IsFinite(HealthFraction) || HealthFraction <= 0.f || HealthFraction > 1.f)
    {
        return Fail(*this, TEXT("HealthFraction"), LOCTEXT("BarrierRange", "Use a finite value greater than 0 and at most 1."), OutError);
    }
    if (!FMath::IsFinite(RechargeDelaySeconds) || RechargeDelaySeconds <= 0.f)
    {
        return Fail(*this, TEXT("RechargeDelaySeconds"), LOCTEXT("BarrierDelayRange", "Use a finite delay greater than 0."), OutError);
    }
    return true;
}

// Reserve one recharge barrier mechanic across the selected pair.
FName UGirlSaviorRechargeBarrierEffectDefinition::GetEffectKey() const
{
    return TEXT("RechargeBarrier");
}

// Show capacity as a share of maximum health and the required rest time.
FText UGirlSaviorRechargeBarrierEffectDefinition::BuildDetailsText() const
{
    return FText::Format(LOCTEXT("BarrierDetails", "Барьер: {0} от максимального здоровья. Восстановление после {1} с без урона"),
        FText::AsPercent(HealthFraction), FText::AsNumber(RechargeDelaySeconds));
}

// Snapshot capacity and recharge timing together.
UGirlSaviorSupportEffectRuntime* UGirlSaviorRechargeBarrierEffectDefinition::CreateValidatedRuntime(
    UGirlSaviorSupportRun& Run) const
{
    auto* Runtime = NewObject<UGirlSaviorRechargeBarrierRuntime>(&Run);
    Runtime->HealthFraction = HealthFraction;
    Runtime->RechargeDelaySeconds = RechargeDelaySeconds;
    return Runtime;
}

#undef LOCTEXT_NAMESPACE
