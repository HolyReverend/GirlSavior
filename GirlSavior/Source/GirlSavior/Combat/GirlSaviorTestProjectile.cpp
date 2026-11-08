#include "Combat/GirlSaviorTestProjectile.h"

#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

// Only the swept root participates in projectile collision; Blueprint visuals stay cosmetic.
AGirlSaviorTestProjectile::AGirlSaviorTestProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 1.5f;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(6.0f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_GameTraceChannel2);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionSphere->SetGenerateOverlapEvents(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionSphere);
	ProjectileMovement->InitialSpeed = 2000.0f;
	ProjectileMovement->MaxSpeed = 2000.0f;
	ProjectileMovement->Velocity = FVector::ForwardVector;
	ProjectileMovement->bInitialVelocityInLocalSpace = true;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AGirlSaviorTestProjectile::HandleProjectileStop);
}

// Ignore the shooter before the movement component performs its first sweep.
void AGirlSaviorTestProjectile::BeginPlay()
{
	if (AActor* Shooter = GetOwner())
	{
		CollisionSphere->IgnoreActorWhenMoving(Shooter, true);
	}
	Super::BeginPlay();
}

void AGirlSaviorTestProjectile::HandleProjectileStop(const FHitResult& Hit)
{
	if (bImpactHandled || !Hit.bBlockingHit)
	{
		return;
	}
	bImpactHandled = true;

	AActor* HitActor = Hit.GetActor();
	UGirlSaviorDamageReceiverComponent* Receiver = HitActor
		? HitActor->FindComponentByClass<UGirlSaviorDamageReceiverComponent>() : nullptr;
	const bool bHitTarget = Receiver != nullptr;
	if (Receiver)
	{
		Receiver->TryApplyDamage(EGirlSaviorDamageKind::DirectProjectile, DamageAmount);
	}

	UE_LOG(LogTemp, Log, TEXT("Prototype projectile impact: %s at %s"),
		bHitTarget ? TEXT("Target") : TEXT("Obstacle"), *Hit.ImpactPoint.ToString());
	OnImpactCue(Hit.ImpactPoint, bHitTarget);
	Destroy();
}