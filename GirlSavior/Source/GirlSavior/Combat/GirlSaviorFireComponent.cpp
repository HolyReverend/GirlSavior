#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Components/CapsuleComponent.h"
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

void UGirlSaviorFireComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	auto* Hero = Cast<AGirlSaviorCharacter>(GetOwner());
	auto* PC = Hero ? Cast<AGirlSaviorPlayerController>(Hero->GetController()) : nullptr;
	auto* Aim = Hero ? Hero->GetAimComponent() : nullptr;
	if (!PC || !PC->CanUseGameplayInput() || !PC->IsFireAllowed() || !Aim || !Aim->IsCursorInsideGameView())
	{
		CancelFireSeries();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Schedule.ConsumeDue(Now)) return;
	if (!FMath::IsFinite(ShotsPerSecond) || ShotsPerSecond <= 0.f || !ProjectileClass
		|| ProjectileClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogGirlSaviorFire, Error, TEXT("%s: firing needs a finite positive rate and a concrete projectile class."), *GetPathName());
		CancelFireSeries();
		return;
	}
	const FVector Position = Hero->GetCapsuleComponent()->GetComponentLocation();
	const FVector Direction = Aim->GetAimDirection();
	FActorSpawnParameters Params;
	Params.Owner = Hero;
	Params.Instigator = Hero;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float Rate = ShotsPerSecond;
	auto* Projectile = GetWorld()->SpawnActor<AGirlSaviorTestProjectile>(ProjectileClass, Position,
		FRotator(0., Direction.Rotation().Yaw, 0.), Params);
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
