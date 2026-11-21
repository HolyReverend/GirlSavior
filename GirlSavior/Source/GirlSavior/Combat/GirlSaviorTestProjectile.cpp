#include "Combat/GirlSaviorTestProjectile.h"

#include "Combat/GirlSaviorCombatChannels.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include <cmath>

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorProjectile, Log, All);

// Only the swept root participates in projectile collision; Blueprint visuals stay cosmetic.
AGirlSaviorTestProjectile::AGirlSaviorTestProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	InitialLifeSpan = 1.5f;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(6.0f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(GirlSaviorCombatChannels::HeroProjectile);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionSphere->SetGenerateOverlapEvents(false);
	CollisionSphere->SetCanEverAffectNavigation(false);

	CombatVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CombatVisual"));
	CombatVisual->SetupAttachment(CollisionSphere);
	CombatVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CombatVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
	CombatVisual->SetGenerateOverlapEvents(false);
	CombatVisual->SetCanEverAffectNavigation(false);
	CombatVisual->SetCastShadow(false);
	CombatVisual->SetVisibility(false);
	CombatVisual->SetHiddenInGame(true);

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
	if (bCombatShot)
	{
		ConfigureCombatShot();
	}
	Super::BeginPlay();
	if (bCombatShot)
	{
		// BeginPlay registers the component tick. Only the actor should drive it in combat.
		ProjectileMovement->SetComponentTickEnabled(false);
	}
	SetActorTickEnabled(bCombatShot);
}

// Register before dispatching BeginPlay so damage and terminal cleanup share one owner.
AGirlSaviorTestProjectile* AGirlSaviorTestProjectile::SpawnCombatProjectile(UWorld& World,
	TSubclassOf<AGirlSaviorTestProjectile> Class, AActor* Owner, const FVector& Direction,
	const FGirlSaviorProjectileLaunch& Launch, AGirlSaviorCombatEncounter& Encounter)
{
	const auto Faction = Launch.Damage.SourceFaction;
	if (!IsValid(Owner) || Owner->GetWorld() != &World || Encounter.GetWorld() != &World
		|| Encounter.GetRunState() != EGirlSaviorRunState::Active
		|| !Class || Class->HasAnyClassFlags(CLASS_Abstract) || Direction.ContainsNaN()
		|| Launch.Damage.Origin.ContainsNaN() || Launch.Damage.Delivery != EGirlSaviorDamageDelivery::DirectProjectile
		|| (Faction != EGirlSaviorFaction::Hero && Faction != EGirlSaviorFaction::Enemy)
		|| !FMath::IsFinite(Launch.Radius) || Launch.Radius <= 0.f || !FMath::IsFinite(Launch.Speed) || Launch.Speed <= 0.f
		|| !FMath::IsFinite(Launch.MaxDistance) || Launch.MaxDistance <= 0.f
		|| !FMath::IsFinite(4.f * (Launch.MaxDistance / Launch.Speed) + 0.25f))
	{
		return nullptr;
	}

	const FVector UnitDirection = Direction.GetSafeNormal();
	const auto* Capsule = Owner->FindComponentByClass<UCapsuleComponent>();
	if (UnitDirection.IsNearlyZero() || (Capsule && Launch.Radius >= Capsule->GetScaledCapsuleRadius()))
	{
		return nullptr;
	}

	const FTransform Transform(UnitDirection.Rotation(), Launch.Damage.Origin);
	auto* Projectile = World.SpawnActorDeferred<AGirlSaviorTestProjectile>(Class, Transform, Owner,
		Cast<APawn>(Launch.Damage.Instigator.Get()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return nullptr;
	}
	Projectile->LaunchSnapshot = Launch;
	Projectile->TravelDirection = UnitDirection;
	Projectile->bCombatShot = true;
	Projectile->CombatEncounter = &Encounter;
	if (!Encounter.RegisterProjectile(Projectile))
	{
		Projectile->Destroy();
		return nullptr;
	}

	Projectile->FinishSpawning(Transform);
	if (IsValid(Projectile))
	{
		Projectile->StopAtInitialBlocker();
	}
	return Projectile;
}

// Drive the stock sweep at a bounded timestep while preserving the legacy native subobject.
void AGirlSaviorTestProjectile::ConfigureCombatShot()
{
	InitialLifeSpan = FMath::Max(1.5f, 4.f * (LaunchSnapshot.MaxDistance / LaunchSnapshot.Speed) + 0.25f);
	CollisionSphere->SetSphereRadius(LaunchSnapshot.Radius, false);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(LaunchSnapshot.Damage.SourceFaction == EGirlSaviorFaction::Hero
		? GirlSaviorCombatChannels::HeroProjectile : GirlSaviorCombatChannels::EnemyProjectile);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CombatVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CombatVisual->SetVisibility(true);
	CombatVisual->SetHiddenInGame(false);

	ProjectileMovement->bAutoUpdateTickRegistration = false;
	ProjectileMovement->PrimaryComponentTick.bStartWithTickEnabled = false;
	ProjectileMovement->SetComponentTickEnabled(false);
	ProjectileMovement->SetUpdatedComponent(CollisionSphere);
	ProjectileMovement->InitialSpeed = LaunchSnapshot.Speed;
	ProjectileMovement->MaxSpeed = LaunchSnapshot.Speed;
	ProjectileMovement->Velocity = TravelDirection * LaunchSnapshot.Speed;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->bForceSubStepping = false;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bSimulationEnabled = true;
}

// A shot inside blocking collision stops without reaching any target past the launch point.
void AGirlSaviorTestProjectile::StopAtInitialBlocker()
{
	const FVector Origin = GetActorLocation();
	const float Radius = CollisionSphere->GetScaledSphereRadius();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorProjectileSpawn), false, this);
	if (const AActor* Shooter = GetOwner())
	{
		Query.AddIgnoredActor(Shooter);
	}
	const auto* Encounter = CombatEncounter.Get();
	const bool bInsideBoundary = Encounter && Encounter->CrossesSafeArea(Origin, Origin, Radius);
	if (!bInsideBoundary && !GetWorld()->OverlapBlockingTestByChannel(Origin, FQuat::Identity,
		CollisionSphere->GetCollisionObjectType(), FCollisionShape::MakeSphere(Radius), Query,
		FCollisionResponseParams(CollisionSphere->GetCollisionResponseToChannels())))
	{
		return;
	}

	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.bStartPenetrating = true;
	Hit.ImpactPoint = Origin - TravelDirection * Radius;
	Hit.ImpactNormal = -TravelDirection;
	Complete(EGirlSaviorProjectileCompletion::Impact, &Hit);
}

// Limit the stock sweep to the flight range and the first protected-boundary crossing.
void AGirlSaviorTestProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCombatShot || bCompleted || DeltaSeconds <= 0.f)
	{
		return;
	}
	const auto* Encounter = CombatEncounter.Get();
	if (!Encounter || Encounter->GetRunState() != EGirlSaviorRunState::Active)
	{
		Destroy();
		return;
	}

	const double Speed = LaunchSnapshot.Speed;
	const FVector Start = GetActorLocation();
	const float Radius = CollisionSphere->GetScaledSphereRadius();
	const double RemainingRange = GetRangeRemaining();
	const double FrameDistance = Speed * DeltaSeconds;
	const bool bRangeLimited = FrameDistance >= RemainingRange;
	double StepDistance = FMath::Min(FrameDistance, RemainingRange);
	const bool bBoundaryLimited = Encounter->CrossesSafeArea(Start, Start + TravelDirection * StepDistance, Radius);
	if (bBoundaryLimited)
	{
		// A physical blocker before the boundary still wins.
		double Clear = 0.0;
		double Blocked = StepDistance;
		for (int32 Iteration = 0; Iteration < 16; ++Iteration)
		{
			const double Middle = (Clear + Blocked) * 0.5;
			if (Encounter->CrossesSafeArea(Start, Start + TravelDirection * Middle, Radius))
			{
				Blocked = Middle;
			}
			else
			{
				Clear = Middle;
			}
		}
		StepDistance = Clear;
	}

	float StepSeconds = static_cast<float>(StepDistance / Speed);
	if (double(StepSeconds) * Speed > StepDistance)
	{
		StepSeconds = std::nextafter(StepSeconds, 0.f);
	}
	if (StepSeconds > 0.f)
	{
		ProjectileMovement->TickComponent(StepSeconds, LEVELTICK_All, nullptr);
	}
	if (bCompleted)
	{
		return;
	}
	DistanceTraveled += FVector::Dist(Start, GetActorLocation());

	if (bBoundaryLimited)
	{
		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.ImpactPoint = GetActorLocation() + TravelDirection * Radius;
		Hit.ImpactNormal = -TravelDirection;
		Complete(EGirlSaviorProjectileCompletion::Impact, &Hit);
	}
	else if (bRangeLimited)
	{
		Complete(EGirlSaviorProjectileCompletion::RangeExpired);
	}
}

void AGirlSaviorTestProjectile::HandleProjectileStop(const FHitResult& Hit)
{
	if (Hit.bBlockingHit)
	{
		Complete(EGirlSaviorProjectileCompletion::Impact, &Hit);
	}
}

// The world timer retains the Demo lifetime and the combat safety deadline.
void AGirlSaviorTestProjectile::LifeSpanExpired()
{
	Complete(EGirlSaviorProjectileCompletion::LifetimeExpired);
}

void AGirlSaviorTestProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Complete(EGirlSaviorProjectileCompletion::Removed);
	Super::EndPlay(EndPlayReason);
}

// Finish once and keep the resolved snapshot alive through impact and completion dispatch.
void AGirlSaviorTestProjectile::Complete(EGirlSaviorProjectileCompletion Reason, const FHitResult* Hit)
{
	if (bCompleted)
	{
		return;
	}
	bCompleted = true;
	SetActorTickEnabled(false);
	ProjectileMovement->SetComponentTickEnabled(false);
	ProjectileMovement->bSimulationEnabled = false;
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (Hit)
	{
		if (bCombatShot)
		{
			DistanceTraveled = FVector::Dist(LaunchSnapshot.Damage.Origin, GetActorLocation());
		}
		AActor* HitActor = Hit->GetActor();
		auto* Receiver = HitActor && !(bCombatShot && Hit->bStartPenetrating)
			? HitActor->FindComponentByClass<UGirlSaviorDamageReceiverComponent>() : nullptr;
		FGirlSaviorProjectileImpact Impact;
		Impact.Point = Hit->ImpactPoint;
		Impact.Normal = Hit->ImpactNormal;
		if (Receiver)
		{
			FGirlSaviorDamageSpec Damage = LaunchSnapshot.Damage;
			if (!bCombatShot)
			{
				Damage.Physical = DamageAmount;
				Damage.Origin = Hit->TraceStart;
			}
			Damage.Contact = Impact.Point;
			Damage.ContactNormal = Impact.Normal;
			Damage.bHasContact = true;
			Damage.Causer = this;
			Impact.ContactType = EGirlSaviorProjectileContact::Target;
			Impact.bHasDamageResult = true;
			Impact.DamageResult = Receiver->ResolveDamage(Damage);
		}

		OnResolvedImpact.Broadcast(Impact);
		if (bCombatShot)
		{
			// This greybox cue stays in the world after the projectile has gone.
			FColor Color = FColor::Orange;
			if (Impact.DamageResult.Outcome == EGirlSaviorDamageOutcome::Applied)
			{
				Color = Impact.DamageResult.bKilled ? FColor::Magenta : FColor::Red;
			}
			else if (Impact.DamageResult.Outcome == EGirlSaviorDamageOutcome::BlockedByDash)
			{
				Color = FColor::Cyan;
			}
			DrawDebugSphere(GetWorld(), Impact.Point, 12.f, 8, Color, false, .15f, 0, 1.5f);
		}
		else
		{
			const bool bHitTarget = Impact.ContactType == EGirlSaviorProjectileContact::Target;
			UE_LOG(LogTemp, Log, TEXT("Prototype projectile impact: %s at %s"),
				bHitTarget ? TEXT("Target") : TEXT("Obstacle"), *Impact.Point.ToString());
			OnImpactCue(Impact.Point, bHitTarget);
		}
	}

	if (auto* Encounter = CombatEncounter.Get())
	{
		Encounter->UnregisterProjectile(this);
	}
	if (bCombatShot || Reason == EGirlSaviorProjectileCompletion::LifetimeExpired)
	{
		UE_LOG(LogGirlSaviorProjectile, Log, TEXT("Projectile end attack=%lld reason=%u traveled=%.4fcm."),
			GetAttackId(), static_cast<uint32>(Reason), DistanceTraveled);
	}
	OnCompleted.Broadcast(this, Reason);
	if (Reason != EGirlSaviorProjectileCompletion::Removed && !IsActorBeingDestroyed())
	{
		Destroy();
	}
}
