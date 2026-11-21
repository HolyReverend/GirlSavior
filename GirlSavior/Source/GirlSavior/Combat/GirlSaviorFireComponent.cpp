#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorCombatPistolSettings.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorFire, Log, All);

UGirlSaviorFireComponent::UGirlSaviorFireComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.bRunOnAnyThread = false;
}

void UGirlSaviorFireComponent::BeginPlay()
{
	Super::BeginPlay();
	// Every launch consumes the final aim sample from this frame.
	if (auto* Hero = Cast<AGirlSaviorCharacter>(GetOwner()))
		AddTickPrerequisiteComponent(Hero->GetAimComponent());
}

void UGirlSaviorFireComponent::SetFireButtonDown(bool bDown)
{
	if (bDown && GetWorld()) Schedule.Press(GetWorld()->GetTimeSeconds());
	else Schedule.Release();
}

void UGirlSaviorFireComponent::CancelFireSeries()
{
	Schedule.Cancel();
}

// Check the combat loadout before the encounter admits live fire.
bool UGirlSaviorFireComponent::SetCombatEncounter(AGirlSaviorCombatEncounter* InEncounter)
{
	if (InEncounter)
	{
		const AActor* Owner = GetOwner();
		const auto* Capsule = Owner ? Owner->FindComponentByClass<UCapsuleComponent>() : nullptr;
		if (!CombatPistol || !CombatPistol->IsConfigurationValid() || !Capsule
			|| CombatPistol->ProjectileRadius >= Capsule->GetScaledCapsuleRadius()
			|| !ProjectileClass || ProjectileClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return false;
		}
	}
	CombatEncounter = InEncounter;
	CancelFireSeries();
	return true;
}

void UGirlSaviorFireComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	auto* Hero = Cast<AGirlSaviorCharacter>(GetOwner());
	auto* PC = Hero ? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
	auto* Aim = Hero ? Hero->GetAimComponent() : nullptr;
	auto* Encounter = CombatEncounter.Get();
	const bool bCombatMode = CombatPistol || Encounter;
	if (!PC || !PC->CanUseGameplayInput() || !PC->IsFireAllowed() || !Aim || !Aim->IsCursorInsideGameView()
		|| (bCombatMode && (!CombatPistol || !Encounter || Encounter->GetRunState() != EGirlSaviorRunState::Active)))
	{
		CancelFireSeries();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Schedule.ConsumeDue(Now)) return;
	const float Rate = bCombatMode ? CombatPistol->ShotsPerSecond : ShotsPerSecond;
	if (!FMath::IsFinite(Rate) || Rate <= 0.f || !ProjectileClass
		|| (bCombatMode && !CombatPistol->IsConfigurationValid())
		|| ProjectileClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogGirlSaviorFire, Error, TEXT("%s: firing needs a finite positive rate and a concrete projectile class."), *GetPathName());
		CancelFireSeries();
		return;
	}
	const FVector Position = Hero->GetCapsuleComponent()->GetComponentLocation();
	const FVector Direction = Aim->GetAimDirection();
	AGirlSaviorTestProjectile* Projectile;
	if (bCombatMode)
	{
		FGirlSaviorProjectileLaunch Launch;
		Launch.Radius = CombatPistol->ProjectileRadius;
		Launch.Speed = CombatPistol->ProjectileSpeed;
		Launch.MaxDistance = CombatPistol->MaxDistance;
		Launch.Damage.Physical = CombatPistol->Damage;
		Launch.Damage.SourceFaction = EGirlSaviorFaction::Hero;
		Launch.Damage.SourceId = TEXT("Hero.Pistol");
		Launch.Damage.Cause = NSLOCTEXT("GirlSaviorDamage", "HeroPistol", "Выстрел пистолета");
		Launch.Damage.Instigator = Hero;
		Launch.Damage.AttackId = Encounter->AllocateAttackId();
		Launch.Damage.Origin = Position;
		Projectile = AGirlSaviorTestProjectile::SpawnCombatProjectile(*GetWorld(), ProjectileClass, Hero, Direction, Launch, *Encounter);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = Hero;
		Params.Instigator = Hero;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Projectile = GetWorld()->SpawnActor<AGirlSaviorTestProjectile>(ProjectileClass, Position,
			FRotator(0., Direction.Rotation().Yaw, 0.), Params);
	}
	if (!Projectile)
	{
		UE_LOG(LogGirlSaviorFire, Error, TEXT("%s: projectile spawn failed."), *GetPathName());
		CancelFireSeries();
		return;
	}
	Schedule.Launched(Now, Rate);
	UE_LOG(LogGirlSaviorFire, Display, TEXT("Shot time=%.6f position=%s direction=%s rate=%.3f"),
		Now, *Position.ToString(), *Direction.ToString(), Rate);
	OnShotCue.Broadcast(Position, Direction);
}
