#pragma once

#include "CoreMinimal.h"
#include "Combat/GirlSaviorDamageTypes.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorTestProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class ULineBatchComponent;
class AGirlSaviorCombatEncounter;
class AGirlSaviorTestProjectile;

// Launch values are copied before Blueprint construction and survive destruction of the shooter.
struct FGirlSaviorProjectileLaunch
{
	FGirlSaviorDamageSpec Damage;
	float Radius = 0.f;
	float Speed = 0.f;
	float MaxDistance = 0.f;
	bool bResolveClippedEndContact = false;
};

UENUM(BlueprintType)
enum class EGirlSaviorProjectileContact : uint8
{
	Geometry,
	Target
};

USTRUCT(BlueprintType)
struct FGirlSaviorProjectileImpact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	EGirlSaviorProjectileContact ContactType = EGirlSaviorProjectileContact::Geometry;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FVector Point = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FVector Normal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	bool bHasDamageResult = false;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	bool bWardContact = false;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FGirlSaviorDamageResult DamageResult;
};

enum class EGirlSaviorProjectileCompletion : uint8
{
	Impact,
	RangeExpired,
	LifetimeExpired,
	Removed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGirlSaviorProjectileResolvedImpact, const FGirlSaviorProjectileImpact&, Impact);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGirlSaviorProjectileCompleted, AGirlSaviorTestProjectile*, EGirlSaviorProjectileCompletion);

UCLASS()
class GIRLSAVIOR_API AGirlSaviorTestProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGirlSaviorTestProjectile();
	virtual void Tick(float DeltaSeconds) override;

	// Returns the shot actor when consumed, including a shot stopped immediately by cover.
	static AGirlSaviorTestProjectile* SpawnCombatProjectile(UWorld& World, TSubclassOf<AGirlSaviorTestProjectile> Class,
		AActor* Owner, const FVector& Direction, const FGirlSaviorProjectileLaunch& Launch,
		AGirlSaviorCombatEncounter& Encounter, UObject* SlotHolder = nullptr,
		FGirlSaviorProjectileCompleted::FDelegate Completion = {});

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Projectile")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Projectile")
	TObjectPtr<UStaticMeshComponent> CombatVisual;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prototype|Projectile", meta = (ClampMin = "0.001"))
	float DamageAmount = 1.0f;

	UPROPERTY(BlueprintAssignable, Category = "Prototype|Projectile")
	FGirlSaviorProjectileResolvedImpact OnResolvedImpact;

	FGirlSaviorProjectileCompleted OnCompleted;

	UFUNCTION(BlueprintPure, Category = "Prototype|Projectile")
	double GetDistanceTraveled() const { return DistanceTraveled; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Projectile")
	double GetRangeRemaining() const { return FMath::Max(0.0, double(LaunchSnapshot.MaxDistance) - DistanceTraveled); }

	int64 GetAttackId() const { return LaunchSnapshot.Damage.AttackId; }
	EGirlSaviorFaction GetSourceFaction() const { return LaunchSnapshot.Damage.SourceFaction; }
	const FVector& GetTravelDirection() const { return TravelDirection; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void LifeSpanExpired() override;

	// Spawn independent presentation here: this actor is destroyed immediately afterward.
	UFUNCTION(BlueprintImplementableEvent, Category = "Prototype|Projectile")
	void OnImpactCue(FVector HitLocation, bool bHitTarget);

private:
	void ConfigureCombatShot();
	void StopAtInitialBlocker();
	bool TryResolveClippedEndContact();
	void Complete(EGirlSaviorProjectileCompletion Reason, const FHitResult* Hit = nullptr);

	// Resolve only the first blocking stop, independently of damage acceptance.
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& Hit);

	FGirlSaviorProjectileLaunch LaunchSnapshot;
	UPROPERTY()
	TObjectPtr<ULineBatchComponent> FlightTrail;
	TWeakObjectPtr<AGirlSaviorCombatEncounter> CombatEncounter;
	FVector TravelDirection = FVector::ZeroVector;
	double DistanceTraveled = 0.0;
	bool bCombatShot = false;
	bool bCompleted = false;
};
