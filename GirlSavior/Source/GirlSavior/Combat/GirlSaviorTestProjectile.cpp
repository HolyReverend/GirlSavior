#include "Combat/GirlSaviorTestProjectile.h"

#include "Combat/GirlSaviorCombatChannels.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include <cmath>

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorProjectile, Log, All);

// Equal full and max distances use a hard cutoff, including contact at the endpoint.
float FGirlSaviorProjectileLaunch::GetDamageMultiplierAtDistance(double Distance) const
{
	if (!bApplyDamageFalloff || FullDamageDistance >= MaxDistance)
	{
		return 1.f;
	}
	return static_cast<float>(FMath::Clamp((MaxDistance - Distance) / (MaxDistance - FullDamageDistance), 0.0, 1.0));
}

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
	const FGirlSaviorProjectileLaunch& Launch, AGirlSaviorCombatEncounter& Encounter, UObject* SlotHolder,
	FGirlSaviorProjectileCompleted::FDelegate Completion)
{
	const auto Faction = Launch.Damage.SourceFaction;
	if (!IsValid(Owner) || Owner->GetWorld() != &World || Encounter.GetWorld() != &World
		|| Encounter.GetRunState() != EGirlSaviorRunState::Active
		|| !Class || Class->HasAnyClassFlags(CLASS_Abstract) || Direction.ContainsNaN()
		|| Launch.Damage.Origin.ContainsNaN() || Launch.Damage.Delivery != EGirlSaviorDamageDelivery::DirectProjectile
		|| (Faction != EGirlSaviorFaction::Hero && Faction != EGirlSaviorFaction::Enemy)
		|| !FMath::IsFinite(Launch.Radius) || Launch.Radius <= 0.f || !FMath::IsFinite(Launch.Speed) || Launch.Speed <= 0.f
		|| !FMath::IsFinite(Launch.MaxDistance) || Launch.MaxDistance <= 0.f
		|| (Launch.bApplyDamageFalloff && (!FMath::IsFinite(Launch.FullDamageDistance)
			|| Launch.FullDamageDistance < 0.f || Launch.FullDamageDistance > Launch.MaxDistance))
		|| (!Launch.Damage.WeaponId.IsNone()
			&& (Faction != EGirlSaviorFaction::Hero || Launch.Damage.ShotGroupId <= 0 || Launch.Damage.AttackId <= 0))
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
	if (Faction == EGirlSaviorFaction::Enemy
		&& (!IsValid(SlotHolder) || !Encounter.TransferDangerousAttack(Launch.Damage.AttackId, *SlotHolder, *Projectile)))
	{
		Encounter.UnregisterProjectile(Projectile);
		Projectile->Destroy();
		return nullptr;
	}
	if (Completion.IsBound())
	{
		Projectile->OnCompleted.Add(MoveTemp(Completion));
	}

	Projectile->FinishSpawning(Transform);
	if (IsValid(Projectile))
	{
		Projectile->StopAtInitialBlocker();
	}
	return Projectile;
}

// Share the exact safety lifetime with listeners registered before BeginPlay.
float AGirlSaviorTestProjectile::GetCombatLifetimeSeconds() const
{
	return FMath::Max(1.5f, 4.f * (LaunchSnapshot.MaxDistance / LaunchSnapshot.Speed) + 0.25f);
}

// Drive the stock sweep at a bounded timestep while preserving the legacy native subobject.
void AGirlSaviorTestProjectile::ConfigureCombatShot()
{
	InitialLifeSpan = GetCombatLifetimeSeconds();
	CollisionSphere->bReturnMaterialOnMove = true;
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
	if (LaunchSnapshot.Damage.SourceFaction == EGirlSaviorFaction::Enemy
		|| (CombatEncounter.IsValid() && CombatEncounter->HasFeedbackPresenter()))
	{
		FlightTrail = NewObject<ULineBatchComponent>(this);
		FlightTrail->SetCanEverAffectNavigation(false);
		FlightTrail->PrimaryComponentTick.bStartWithTickEnabled = false;
		FlightTrail->RegisterComponent();
	}

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
	TArray<FOverlapResult> Overlaps;
	if (!bInsideBoundary && !GetWorld()->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity,
		CollisionSphere->GetCollisionObjectType(), FCollisionShape::MakeSphere(Radius), Query,
		FCollisionResponseParams(CollisionSphere->GetCollisionResponseToChannels())))
	{
		return;
	}

	const FOverlapResult* Blocker = Overlaps.FindByPredicate([](const FOverlapResult& Overlap)
	{
		return Overlap.bBlockingHit;
	});
	// An initial overlap has an identity but no swept surface material.
	FHitResult Hit(Blocker ? Blocker->GetActor() : nullptr, Blocker ? Blocker->GetComponent() : nullptr,
		Origin - TravelDirection * Radius, -TravelDirection);
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

	if (bRangeLimited && !bBoundaryLimited && GetSourceFaction() == EGirlSaviorFaction::Hero)
	{
		SweepRemainingHeroRange(Start + TravelDirection * StepDistance);
	}
	else
	{
		float StepSeconds = static_cast<float>(StepDistance / Speed);
		if (double(StepSeconds) * Speed > StepDistance)
		{
			StepSeconds = std::nextafter(StepSeconds, 0.f);
		}
		if (StepSeconds > 0.f)
		{
			ProjectileMovement->TickComponent(StepSeconds, LEVELTICK_All, nullptr);
		}
	}
	if (bCompleted)
	{
		return;
	}
	DistanceTraveled += FVector::Dist(Start, GetActorLocation());
	if (FlightTrail)
	{
		FlightTrail->Flush();
		const FVector Head = GetActorLocation();
		const bool bEnemy = LaunchSnapshot.Damage.SourceFaction == EGirlSaviorFaction::Enemy;
		const auto& Trail = GetTrail();
		const double Length = FMath::Min(Trail ? double(Trail->Length) : (bEnemy ? 82.0 : 68.0), DistanceTraveled);
		const FLinearColor Color = Trail ? Trail->Color
			: (bEnemy ? FLinearColor(1.f, 0.15f, 0.045f) : FLinearColor(1.f, 0.88f, 0.57f));
		const float TailWidth = Trail ? Trail->Width * (2.f / 3.f) : (bEnemy ? 5.f : 2.f);
		const float HeadWidth = Trail ? Trail->Width : (bEnemy ? 7.f : 3.f);
		const FVector Middle = Head - TravelDirection * (Length * 0.4);
		FlightTrail->DrawLine(Head - TravelDirection * Length, Middle, Color * 0.52f, SDPG_World, TailWidth, 0.f);
		FlightTrail->DrawLine(Middle, Head, Color, SDPG_World, HeadWidth, 0.f);
	}

	if (bBoundaryLimited)
	{
		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.ImpactPoint = GetActorLocation() + TravelDirection * Radius;
		Hit.ImpactNormal = -TravelDirection;
		Complete(EGirlSaviorProjectileCompletion::Impact, &Hit);
	}
	else if (bRangeLimited && !TryResolveClippedEndContact())
	{
		Complete(EGirlSaviorProjectileCompletion::RangeExpired);
	}
}

// Sweep the final step directly so float time quantization cannot shorten its range.
void AGirlSaviorTestProjectile::SweepRemainingHeroRange(const FVector& Endpoint)
{
	const FVector Position = GetActorLocation();
	const double StepDistance = FVector::DotProduct(Endpoint - Position, TravelDirection);
	if (StepDistance <= 0.0)
	{
		return;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorProjectileExactEnd), false, this);
	FCollisionResponseParams Response;
	CollisionSphere->InitSweepCollisionParams(Query, Response);
	if (const AActor* Shooter = GetOwner())
	{
		Query.AddIgnoredActor(Shooter);
	}
	FHitResult Hit;
	// Chaos can omit contacts at the exact sweep end. Probe past it and accept only range-roundoff hits.
	constexpr double QueryPadding = 0.01;
	constexpr double RangeRoundoff = 0.001;
	const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, Position,
		Endpoint + TravelDirection * QueryPadding, FQuat::Identity,
		CollisionSphere->GetCollisionObjectType(), CollisionSphere->GetCollisionShape(), Query, Response)
		&& double(Hit.Distance) <= StepDistance + RangeRoundoff;
	SetActorLocation(bBlocked && Hit.Distance < StepDistance ? Hit.Location : Endpoint);
	if (bBlocked)
	{
		Complete(EGirlSaviorProjectileCompletion::Impact, &Hit);
	}
}

// A sweep stopping exactly at its warned range can finish just short of the contact numerically.
bool AGirlSaviorTestProjectile::TryResolveClippedEndContact()
{
	if (!LaunchSnapshot.bResolveClippedEndContact)
	{
		return false;
	}
	const FVector Position = GetActorLocation();
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorProjectileClippedEnd), false, this);
	FCollisionResponseParams Response(CollisionSphere->GetCollisionResponseToChannels());
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, Position, Position + TravelDirection * 0.25, FQuat::Identity,
		CollisionSphere->GetCollisionObjectType(), CollisionSphere->GetCollisionShape(), Query, Response))
	{
		return false;
	}
	Complete(EGirlSaviorProjectileCompletion::Impact, &Hit);
	return true;
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
		ResolveContact(*Hit);
	}

	if (auto* Encounter = CombatEncounter.Get())
	{
		Encounter->ReleaseDangerousAttack(GetAttackId(), *this);
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

// Resolve damage once at the projectile center. A spent round still stops on contact.
void AGirlSaviorTestProjectile::ResolveContact(const FHitResult& Hit)
{
	if (bCombatShot)
	{
		DistanceTraveled = FMath::Clamp(FVector::Dist(LaunchSnapshot.Damage.Origin, GetActorLocation()),
			0.0, double(LaunchSnapshot.MaxDistance));
	}
	AActor* HitActor = Hit.GetActor();
	auto* Receiver = HitActor && !(bCombatShot && Hit.bStartPenetrating)
		? HitActor->FindComponentByClass<UGirlSaviorDamageReceiverComponent>() : nullptr;
	FGirlSaviorProjectileImpact Impact;
	Impact.Point = Hit.ImpactPoint;
	Impact.Normal = Hit.ImpactNormal;
	Impact.WeaponId = LaunchSnapshot.Damage.WeaponId;
	Impact.ShotGroupId = GetShotGroupId();
	Impact.SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
	Impact.BlockingActor = HitActor;
	Impact.BlockingComponent = Hit.GetComponent();
	Impact.GeometryKeyPoint = Hit.bStartPenetrating ? GetActorLocation() : FVector(Hit.ImpactPoint);
	const auto* Encounter = CombatEncounter.Get();
	Impact.bWardContact = (HitActor && HitActor->ActorHasTag(TEXT("CombatSafeWard")))
		|| (Encounter && Encounter->CrossesSafeArea(Impact.Point, Impact.Point, CollisionSphere->GetScaledSphereRadius() + 1.f));
	if (Receiver)
	{
		FGirlSaviorDamageSpec Damage = LaunchSnapshot.Damage;
		if (!bCombatShot)
		{
			Damage.Physical = DamageAmount;
			Damage.Origin = Hit.TraceStart;
		}
		else if (LaunchSnapshot.bApplyDamageFalloff)
		{
			// Movement can pull the actor back from a blocker. The sweep gives its true center at contact.
			const double ContactDistance = FMath::Min(FVector::Dist(Damage.Origin, Hit.Location),
				double(LaunchSnapshot.MaxDistance));
			const float Multiplier = LaunchSnapshot.GetDamageMultiplierAtDistance(ContactDistance);
			Damage.Physical *= Multiplier;
			Damage.Magical *= Multiplier;
			if (Damage.Physical == 0.f && Damage.Magical == 0.f)
			{
				return;
			}
		}
		Damage.Contact = Impact.Point;
		Damage.ContactNormal = Impact.Normal;
		Damage.bHasContact = true;
		Damage.Causer = this;
		Impact.ContactType = EGirlSaviorProjectileContact::Target;
		Impact.bHasDamageResult = true;
		Impact.DamageResult = Receiver->ResolveDamage(Damage);
		// Support consumes only the final committed weapon hit, after structural death listeners.
		if (auto* HitEncounter = CombatEncounter.Get(); !Damage.WeaponId.IsNone() && HitEncounter)
		{
			HitEncounter->NotifyHeroWeaponDamage(HitActor, Impact.DamageResult);
		}
	}

	OnResolvedImpact.Broadcast(Impact);
	if (bCombatShot && (!Encounter || !Encounter->HasFeedbackPresenter()))
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
	else if (!bCombatShot)
	{
		const bool bHitTarget = Impact.ContactType == EGirlSaviorProjectileContact::Target;
		UE_LOG(LogTemp, Log, TEXT("Prototype projectile impact: %s at %s"),
			bHitTarget ? TEXT("Target") : TEXT("Obstacle"), *Impact.Point.ToString());
		OnImpactCue(Impact.Point, bHitTarget);
	}
}
