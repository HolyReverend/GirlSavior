#pragma once

#include "CoreMinimal.h"
#include "Combat/GirlSaviorDamageTypes.h"
#include "Components/ActorComponent.h"
#include "GirlSaviorDamageReceiverComponent.generated.h"

struct FGirlSaviorVitals;
class UGirlSaviorVitalsSettings;

UENUM(BlueprintType)
enum class EGirlSaviorDamageKind : uint8
{
	DirectProjectile
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGirlSaviorAcceptedDamage, EGirlSaviorDamageKind, Kind, float, Amount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGirlSaviorDamageResolved, const FGirlSaviorDamageResult&, Result);
DECLARE_MULTICAST_DELEGATE_OneParam(FGirlSaviorDamageDeath, const FGirlSaviorDamageResult&);
DECLARE_DELEGATE_RetVal_OneParam(EGirlSaviorDamageOutcome, FGirlSaviorDamageAdmission, const FGirlSaviorDamageSpec&);

UCLASS(ClassGroup = (Prototype), meta = (BlueprintSpawnableComponent))
class GIRLSAVIOR_API UGirlSaviorDamageReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGirlSaviorDamageReceiverComponent();

	// Keep the serialized Stage 4 Blueprint entry point and forward it to the resolver.
	UFUNCTION(BlueprintCallable, Category = "Prototype|Damage")
	bool TryApplyDamage(EGirlSaviorDamageKind Kind, float Amount);

	bool InitializeVitals(const UGirlSaviorVitalsSettings* Settings, EGirlSaviorFaction InFaction);
	bool InitializeVitals(const FGirlSaviorVitals& Settings, EGirlSaviorFaction InFaction);
	FGirlSaviorDamageResult ResolveDamage(const FGirlSaviorDamageSpec& Spec);

	UFUNCTION(BlueprintPure, Category = "Prototype|Damage")
	bool HasVitals() const { return bHasVitals; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Damage")
	bool IsAlive() const { return bHasVitals && Health > 0.0; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Damage")
	EGirlSaviorFaction GetFaction() const { return Faction; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Damage")
	double GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Damage")
	double GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Damage")
	double GetBarrier() const { return Barrier; }

	const FGirlSaviorDamageResult& GetLastLethalResult() const { return LastLethalResult; }

	// Custom run/boundary validation. Applied admits a hit, Rejected suppresses any presentation.
	FGirlSaviorDamageAdmission DamageAdmission;

	// Structural listeners receive committed death before presentation. Defer actor teardown.
	FGirlSaviorDamageDeath OnDied;

	UPROPERTY(BlueprintAssignable, Category = "Prototype|Damage")
	FGirlSaviorDamageResolved OnDamageResolved;

	UPROPERTY(BlueprintAssignable, Category = "Prototype|Damage")
	FGirlSaviorAcceptedDamage OnDamageAccepted;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Prototype|Damage")
	double AcceptedDamageTotal = 0.0;

private:
	FGirlSaviorDamageResult ResolveDamage(const FGirlSaviorDamageSpec& Spec, bool bLegacyCall);

	UPROPERTY(Transient)
	FGirlSaviorDamageResult LastLethalResult;

	EGirlSaviorFaction Faction = EGirlSaviorFaction::Neutral;
	double MaxHealth = 0.0;
	double Health = 0.0;
	double Barrier = 0.0;
	double PhysicalResistance = 0.0;
	double MagicalResistance = 0.0;
	bool bHasVitals = false;
	bool bResolvingDamage = false;
};
