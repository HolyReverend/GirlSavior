#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorTestProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorTestProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGirlSaviorTestProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Projectile")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prototype|Projectile", meta = (ClampMin = "0.001"))
	float DamageAmount = 1.0f;

protected:
	virtual void BeginPlay() override;

	// Spawn independent presentation here: this actor is destroyed immediately afterward.
	UFUNCTION(BlueprintImplementableEvent, Category = "Prototype|Projectile")
	void OnImpactCue(FVector HitLocation, bool bHitTarget);

private:
	// Resolve only the first blocking stop, independently of damage acceptance.
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& Hit);

	bool bImpactHandled = false;
};