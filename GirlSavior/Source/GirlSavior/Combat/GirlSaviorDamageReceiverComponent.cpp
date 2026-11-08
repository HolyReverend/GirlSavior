#include "Combat/GirlSaviorDamageReceiverComponent.h"

// Damage reception is event-driven and needs no frame tick.
UGirlSaviorDamageReceiverComponent::UGirlSaviorDamageReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UGirlSaviorDamageReceiverComponent::TryApplyDamage(EGirlSaviorDamageKind Kind, float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return false;
	}

	AcceptedDamageTotal += Amount;
	OnDamageAccepted.Broadcast(Kind, Amount);
	return true;
}