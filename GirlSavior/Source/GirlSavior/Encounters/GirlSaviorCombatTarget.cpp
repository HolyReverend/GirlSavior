#include "Encounters/GirlSaviorCombatTarget.h"

#include "Combat/GirlSaviorCombatChannels.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

// A stationary target uses the same health and collision path as a live enemy.
AGirlSaviorCombatTarget::AGirlSaviorCombatTarget()
{
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->SetCollisionProfileName(TEXT("BlockAll"));
    Body->SetCollisionResponseToChannel(GirlSaviorCombatChannels::HeroProjectile, ECR_Block);
    Body->SetCollisionResponseToChannel(GirlSaviorCombatChannels::Occluder, ECR_Ignore);
    DamageReceiver = CreateDefaultSubobject<UGirlSaviorDamageReceiverComponent>(TEXT("DamageReceiver"));
}

bool AGirlSaviorCombatTarget::InitializeForEncounter()
{
    Body->SetCollisionResponseToChannel(GirlSaviorCombatChannels::HeroProjectile, ECR_Block);
    Body->SetCollisionResponseToChannel(GirlSaviorCombatChannels::Occluder, ECR_Ignore);
    if (!Body->GetStaticMesh() || !DamageReceiver->InitializeVitals(Vitals, EGirlSaviorFaction::Neutral))
    {
        return false;
    }
    RestingMaterial = Body->GetMaterial(0);
    DamageReceiver->OnDied.AddUObject(this, &ThisClass::OnDied);
    DamageReceiver->OnDamageResolved.AddDynamic(this, &ThisClass::OnHit);
    return true;
}

// Restart the flash on successive hits and leave the death pose in control on a lethal hit.
void AGirlSaviorCombatTarget::OnHit(const FGirlSaviorDamageResult& Result)
{
    if (HitMaterial && Result.HealthLost > 0.0 && !Result.bKilled)
    {
        Body->SetMaterial(0, HitMaterial);
        GetWorldTimerManager().SetTimer(HitFlashTimer, this, &ThisClass::RestoreMaterial, 0.12f);
    }
}

void AGirlSaviorCombatTarget::RestoreMaterial()
{
    Body->SetMaterial(0, RestingMaterial);
}

void AGirlSaviorCombatTarget::OnDied(const FGirlSaviorDamageResult& Result)
{
    GetWorldTimerManager().ClearTimer(HitFlashTimer);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FVector Scale = GetActorScale3D();
    const double Drop = Body->Bounds.BoxExtent.Z * 0.85;
    Scale.Z *= 0.15;
    SetActorScale3D(Scale);
    AddActorWorldOffset(FVector(0, 0, -Drop));
    RestoreMaterial();
}

void AGirlSaviorCombatTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    DamageReceiver->OnDied.RemoveAll(this);
    DamageReceiver->OnDamageResolved.RemoveAll(this);
    Super::EndPlay(EndPlayReason);
}
