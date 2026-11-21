#include "Combat/GirlSaviorDamageReceiverComponent.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Combat/GirlSaviorVitalsSettings.h"
#include "Math/NumericLimits.h"

namespace
{
	// Delivery and faction come from independent producers. Reject unknown serialized values.
	bool IsKnownFaction(EGirlSaviorFaction Faction)
	{
		return Faction == EGirlSaviorFaction::Hero || Faction == EGirlSaviorFaction::Enemy
			|| Faction == EGirlSaviorFaction::Neutral;
	}

	bool IsDirectHit(EGirlSaviorDamageDelivery Delivery)
	{
		return Delivery == EGirlSaviorDamageDelivery::DirectProjectile || Delivery == EGirlSaviorDamageDelivery::DirectMelee
			|| Delivery == EGirlSaviorDamageDelivery::DirectCharge;
	}
}

// Damage reception is event-driven and needs no frame tick.
UGirlSaviorDamageReceiverComponent::UGirlSaviorDamageReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Snapshot asset values once. A failed initialization leaves the receiver available for setup.
bool UGirlSaviorDamageReceiverComponent::InitializeVitals(const UGirlSaviorVitalsSettings* Settings, EGirlSaviorFaction InFaction)
{
	return Settings && InitializeVitals(Settings->GetVitals(), InFaction);
}

bool UGirlSaviorDamageReceiverComponent::InitializeVitals(const FGirlSaviorVitals& Settings, EGirlSaviorFaction InFaction)
{
	if (!IsInGameThread() || bHasVitals || bResolvingDamage || !Settings.IsConfigurationValid() || !IsKnownFaction(InFaction))
	{
		return false;
	}

	Faction = InFaction;
	MaxHealth = Settings.MaxHealth;
	Health = MaxHealth;
	Barrier = Settings.InitialBarrier;
	PhysicalResistance = Settings.PhysicalResistance;
	MagicalResistance = Settings.MagicalResistance;
	bHasVitals = true;
	return true;
}

// Serialized legacy callers supply one physical portion and an explicitly neutral source.
bool UGirlSaviorDamageReceiverComponent::TryApplyDamage(EGirlSaviorDamageKind Kind, float Amount)
{
	if (Kind != EGirlSaviorDamageKind::DirectProjectile)
	{
		return false;
	}

	FGirlSaviorDamageSpec Spec;
	Spec.Physical = Amount;
	Spec.Delivery = EGirlSaviorDamageDelivery::DirectProjectile;
	return ResolveDamage(Spec, true).Outcome == EGirlSaviorDamageOutcome::Applied;
}

FGirlSaviorDamageResult UGirlSaviorDamageReceiverComponent::ResolveDamage(const FGirlSaviorDamageSpec& Spec)
{
	return ResolveDamage(Spec, false);
}

// Resolve and publish exactly one immutable result. Synchronous nested calls are rejected.
FGirlSaviorDamageResult UGirlSaviorDamageReceiverComponent::ResolveDamage(const FGirlSaviorDamageSpec& InSpec, bool bLegacyCall)
{
	FGirlSaviorDamageResult Result;
	Result.Spec = InSpec;
	if (!IsInGameThread())
	{
		return Result;
	}

	Result.HealthBefore = Result.HealthAfter = Health;
	Result.BarrierBefore = Result.BarrierAfter = Barrier;
	const FGirlSaviorDamageSpec& Spec = Result.Spec;
	const AActor* Owner = GetOwner();
	const bool bDirect = IsDirectHit(Spec.Delivery);
	const double Total = double(Spec.Physical) + Spec.Magical;
	if (bResolvingDamage || !IsValid(Owner) || (bHasVitals && !IsAlive())
		|| (!bDirect && Spec.Delivery != EGirlSaviorDamageDelivery::Area && Spec.Delivery != EGirlSaviorDamageDelivery::FloorHazard)
		|| !IsKnownFaction(Spec.SourceFaction)
		|| !FMath::IsFinite(Total) || Total <= 0.0 || Spec.Physical < 0.f || Spec.Magical < 0.f
		|| Spec.Origin.ContainsNaN() || (Spec.bHasContact && (Spec.Contact.ContainsNaN() || Spec.ContactNormal.ContainsNaN()))
		|| Spec.Instigator.Get() == Owner || Spec.Causer.Get() == Owner
		|| (bHasVitals && Faction != EGirlSaviorFaction::Neutral && Faction == Spec.SourceFaction))
	{
		return Result;
	}

	TGuardValue<bool> ResolvingGuard(bResolvingDamage, true);
	if (DamageAdmission.IsBound())
	{
		const EGirlSaviorDamageOutcome Admission = DamageAdmission.Execute(Spec);
		if (Admission != EGirlSaviorDamageOutcome::Applied)
		{
			if (Admission == EGirlSaviorDamageOutcome::BlockedBySafeArea)
			{
				Result.Outcome = Admission;
				OnDamageResolved.Broadcast(Result);
			}
			return Result;
		}
	}

	if (bDirect && Faction == EGirlSaviorFaction::Hero)
	{
		const AGirlSaviorCharacter* Hero = Cast<AGirlSaviorCharacter>(Owner);
		const auto* Movement = Hero ? Cast<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement()) : nullptr;
		if (Movement && Movement->IsDashProtected())
		{
			Result.Outcome = EGirlSaviorDamageOutcome::BlockedByDash;
			OnDamageResolved.Broadcast(Result);
			return Result;
		}
	}

	Result.Outcome = EGirlSaviorDamageOutcome::Applied;
	if (bHasVitals)
	{
		// Incoming modifiers will apply after resistances and before the barrier.
		constexpr double IncomingMultiplier = 1.0;
		const double Adjusted = (Spec.Physical * (1.0 - PhysicalResistance)
			+ Spec.Magical * (1.0 - MagicalResistance)) * IncomingMultiplier;
		const double Absorbed = FMath::Min(Barrier, Adjusted);
		Barrier = FMath::Max(0.0, Barrier - Absorbed);
		Health = FMath::Clamp(Health - (Adjusted - Absorbed), 0.0, MaxHealth);
		Result.HealthAfter = Health;
		Result.BarrierAfter = Barrier;
		Result.HealthLost = Result.HealthBefore - Health;
		Result.BarrierLost = Result.BarrierBefore - Barrier;
		Result.bBarrierBroken = Result.BarrierBefore > 0.0 && Barrier == 0.0;
		Result.bKilled = Result.HealthBefore > 0.0 && Health == 0.0;
		if (Result.bKilled)
		{
			LastLethalResult = Result;
			OnDied.Broadcast(Result);
		}
	}
	else
	{
		// Stage 4 fixtures keep the full accepted amount and have no health or death latch.
		AcceptedDamageTotal += Total;
	}

	if ((bLegacyCall || !bHasVitals) && Spec.Delivery == EGirlSaviorDamageDelivery::DirectProjectile)
	{
		const float LegacyAmount = static_cast<float>(FMath::Min(Total, double(TNumericLimits<float>::Max())));
		OnDamageAccepted.Broadcast(EGirlSaviorDamageKind::DirectProjectile, LegacyAmount);
	}
	OnDamageResolved.Broadcast(Result);
	return Result;
}
