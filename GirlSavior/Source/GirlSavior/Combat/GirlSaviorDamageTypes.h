#pragma once

#include "CoreMinimal.h"
#include "GirlSaviorDamageTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EGirlSaviorDamageDelivery : uint8
{
	DirectProjectile,
	DirectMelee,
	DirectCharge,
	Area,
	FloorHazard
};

UENUM(BlueprintType)
enum class EGirlSaviorFaction : uint8
{
	Hero,
	Enemy,
	Neutral
};

UENUM(BlueprintType)
enum class EGirlSaviorDamageOutcome : uint8
{
	Applied,
	BlockedByDash,
	BlockedBySafeArea,
	Rejected
};

// The delivery carries the source identity even if its actor has already been destroyed.
USTRUCT(BlueprintType)
struct GIRLSAVIOR_API FGirlSaviorDamageSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Physical = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Magical = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	EGirlSaviorDamageDelivery Delivery = EGirlSaviorDamageDelivery::DirectProjectile;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	EGirlSaviorFaction SourceFaction = EGirlSaviorFaction::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FName SourceId;

	// Stable row and discharge identifiers for weapon projectiles.
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FName WeaponId;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	int64 ShotGroupId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FText Cause;

	UPROPERTY()
	TWeakObjectPtr<AActor> Instigator;

	UPROPERTY()
	TWeakObjectPtr<AActor> Causer;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	int64 AttackId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector Contact = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector ContactNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bHasContact = false;
};

// These deltas describe committed changes and can be consumed by presentation independently.
USTRUCT(BlueprintType)
struct GIRLSAVIOR_API FGirlSaviorDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FGirlSaviorDamageSpec Spec;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	EGirlSaviorDamageOutcome Outcome = EGirlSaviorDamageOutcome::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	double HealthBefore = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	double HealthAfter = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	double BarrierBefore = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	double BarrierAfter = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	double HealthLost = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	double BarrierLost = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bBarrierBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bKilled = false;
};
