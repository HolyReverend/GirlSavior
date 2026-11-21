#include "Encounters/GirlSaviorCombatEncounter.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorCombatChannels.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorSafeArea.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Encounters/GirlSaviorCombatTarget.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorEncounter, Log, All);

namespace
{
void ConfigureTrigger(UBoxComponent& Volume)
{
    Volume.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Volume.SetCollisionResponseToAllChannels(ECR_Ignore);
    Volume.SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Volume.SetGenerateOverlapEvents(true);
    Volume.SetCanEverAffectNavigation(false);
}
}

// Poll authored volumes once per late frame so possession and overlapping triggers share one order.
AGirlSaviorCombatEncounter::AGirlSaviorCombatEncounter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    FirstRoomVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("FirstRoomVolume"));
    FirstRoomVolume->SetupAttachment(GetRootComponent());
    ConfigureTrigger(*FirstRoomVolume);
    ExitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitVolume"));
    ExitVolume->SetupAttachment(GetRootComponent());
    ConfigureTrigger(*ExitVolume);
}

// Wait until the local pawn exists, then bind receiver admission before opening combat.
bool AGirlSaviorCombatEncounter::InitializeRun(AGirlSaviorCharacter& Pawn)
{
    auto* Controller = Cast<AGirlSaviorPlayerController>(Pawn.GetController());
    if (!Controller)
    {
        return false;
    }
    Controller->SetFireAllowed(false);
    if (!IsValid(SafeArea) || PlacedEnemies.IsEmpty()
        || !Pawn.GetFireComponent()->SetCombatEncounter(this)
        || !Pawn.GetDamageReceiver()->InitializeVitals(Pawn.CombatVitals, EGirlSaviorFaction::Hero))
    {
        UE_LOG(LogGirlSaviorEncounter, Error, TEXT("%s is missing a safe area, enemy, hero vitals or configured pistol."), *GetName());
        return false;
    }

    Hero = &Pawn;
    Pawn.GetCapsuleComponent()->SetCollisionResponseToChannel(GirlSaviorCombatChannels::EnemyProjectile, ECR_Block);
    SafeArea->SafeVolume->SetCollisionResponseToChannel(GirlSaviorCombatChannels::HeroProjectile, ECR_Block);
    SafeArea->SafeVolume->SetCollisionResponseToChannel(GirlSaviorCombatChannels::EnemyProjectile, ECR_Block);
    SafeArea->SafeVolume->SetCollisionResponseToChannel(GirlSaviorCombatChannels::Occluder, ECR_Block);
    SafeArea->RegisterPawn(SafeArea->SafeVolume->IsOverlappingActor(&Pawn) ? &Pawn : nullptr);
    BindAdmission(*Pawn.GetDamageReceiver());
    Pawn.GetDamageReceiver()->OnDied.AddUObject(this, &ThisClass::OnHeroDied);

    for (AGirlSaviorCombatTarget* Target : PlacedTargets)
    {
        if (!IsValid(Target) || !Target->InitializeForEncounter())
        {
            UE_LOG(LogGirlSaviorEncounter, Error, TEXT("Invalid combat target %s."), *GetNameSafe(Target));
            return false;
        }
        BindAdmission(*Target->GetDamageReceiver());
    }
    for (AGirlSaviorEnemyCharacter* Enemy : PlacedEnemies)
    {
        if (!IsValid(Enemy) || LiveEnemies.Contains(Enemy) || !Enemy->InitializeForEncounter(*this))
        {
            UE_LOG(LogGirlSaviorEncounter, Error, TEXT("Invalid combat enemy %s."), *GetNameSafe(Enemy));
            return false;
        }
        BindAdmission(*Enemy->GetDamageReceiver());
        LiveEnemies.Add(Enemy);
    }
    UE_LOG(LogGirlSaviorEncounter, Display, TEXT("First fight standing by. Enemies=%d"), LiveEnemies.Num());
    return true;
}

void AGirlSaviorCombatEncounter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bCleanupPending)
    {
        bCleanupPending = false;
        // A lethal projectile may still be dispatching when the run first closes.
        const auto Projectiles = MoveTemp(LiveProjectiles);
        LiveProjectiles.Reset();
        for (const auto& Projectile : Projectiles)
        {
            if (Projectile.IsValid())
            {
                Projectile->Destroy();
            }
        }
        const auto Enemies = MoveTemp(PendingRetirements);
        PendingRetirements.Reset();
        for (const auto& Enemy : Enemies)
        {
            if (Enemy.IsValid())
            {
                Enemy->Destroy();
            }
        }
    }
    if (!bInitializationAttempted)
    {
        if (auto* Pawn = Cast<AGirlSaviorCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            bInitializationAttempted = true;
            bInitialized = InitializeRun(*Pawn);
        }
    }
    if (!bInitialized || !Hero.IsValid())
    {
        return;
    }

    CaptureView();
    switch (RunState)
    {
    case EGirlSaviorRunState::Waiting:
        if (FirstRoomVolume->IsOverlappingActor(Hero.Get()) && IsValid(SafeArea)
            && !SafeArea->SafeVolume->IsOverlappingActor(Hero.Get()) && !IsPointInSafeArea(Hero->GetActorLocation()))
        {
            RunState = EGirlSaviorRunState::Active;
            if (auto* Controller = Cast<AGirlSaviorPlayerController>(Hero->GetController()))
            {
                Controller->SetFireAllowed(true);
            }
            UE_LOG(LogGirlSaviorEncounter, Display, TEXT("First fight active."));
        }
        break;
    case EGirlSaviorRunState::Active:
        if (ExitVolume->IsOverlappingActor(Hero.Get()))
        {
            CloseRun(EGirlSaviorRunState::Succeeded);
        }
        break;
    default:
        break;
    }
}

void AGirlSaviorCombatEncounter::BindAdmission(UGirlSaviorDamageReceiverComponent& Receiver)
{
    const TWeakObjectPtr<AActor> Target(Receiver.GetOwner());
    Receiver.DamageAdmission.BindWeakLambda(this, [this, Target](const FGirlSaviorDamageSpec& Spec)
    {
        return Target.IsValid() ? CheckDamageBoundary(*Target.Get(), Spec) : EGirlSaviorDamageOutcome::Rejected;
    });
}

// A hit can arrive at the edge of the pocket on the same update as the overlap begins.
EGirlSaviorDamageOutcome AGirlSaviorCombatEncounter::CheckDamageBoundary(
    const AActor& Target, const FGirlSaviorDamageSpec& Spec) const
{
    if (RunState == EGirlSaviorRunState::Defeated || RunState == EGirlSaviorRunState::Succeeded)
    {
        return EGirlSaviorDamageOutcome::Rejected;
    }
    if (IsPointInSafeArea(Target.GetActorLocation())
        || (SafeArea && SafeArea->SafeVolume->IsOverlappingActor(&Target))
        || (Spec.bHasContact && CrossesSafeArea(Spec.Origin, Spec.Contact)))
    {
        return EGirlSaviorDamageOutcome::BlockedBySafeArea;
    }
    return RunState == EGirlSaviorRunState::Active ? EGirlSaviorDamageOutcome::Applied : EGirlSaviorDamageOutcome::Rejected;
}

bool AGirlSaviorCombatEncounter::CanEnemyEngage(const AActor& Source) const
{
    return RunState == EGirlSaviorRunState::Active && Hero.IsValid() && IsValid(SafeArea)
        && Hero->GetDamageReceiver()->IsAlive() && !IsPointInSafeArea(Source.GetActorLocation())
        && !IsPointInSafeArea(Hero->GetActorLocation())
        && !SafeArea->SafeVolume->IsOverlappingActor(Hero.Get());
}

bool AGirlSaviorCombatEncounter::IsPointInSafeArea(const FVector& Point) const
{
    return CrossesSafeArea(Point, Point);
}

// Work in the actual box's rotated frame. Zero-length segments cover containment.
bool AGirlSaviorCombatEncounter::CrossesSafeArea(const FVector& Start, const FVector& End, float Radius) const
{
    if (!IsValid(SafeArea))
    {
        return false;
    }
    const UBoxComponent& Volume = *SafeArea->SafeVolume;
    const FVector Extent = Volume.GetScaledBoxExtent() + FVector(FMath::Max(0.f, Radius));
    const FBox Bounds(-Extent, Extent);
    const FTransform& Transform = Volume.GetComponentTransform();
    const FVector LocalStart = Transform.InverseTransformPositionNoScale(Start);
    const FVector LocalEnd = Transform.InverseTransformPositionNoScale(End);
    return Bounds.IsInsideOrOn(LocalStart) || Bounds.IsInsideOrOn(LocalEnd)
        || FMath::LineBoxIntersection(Bounds, LocalStart, LocalEnd, LocalEnd - LocalStart);
}

void AGirlSaviorCombatEncounter::CaptureView()
{
    const auto* Controller = Cast<APlayerController>(Hero->GetController());
    const auto* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
    FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
    bViewValid = Viewport && LocalPlayer->GetProjectionData(Viewport, View)
        && View.GetConstrainedViewRect().Width() > 0 && View.GetConstrainedViewRect().Height() > 0;
    ViewOrigin = bViewValid ? View.ViewOrigin : FVector::ZeroVector;
}

// A warned floor patch needs a sample inside the constrained view with a clear camera ray.
bool AGirlSaviorCombatEncounter::IsWarningVisible(TConstArrayView<FVector> FloorPolygon) const
{
    if (!bViewValid)
    {
        return false;
    }
    const FIntRect& Rect = View.GetConstrainedViewRect();
    const FMatrix ViewProjection = View.ComputeViewProjectionMatrix();
    for (const FVector& Point : FloorPolygon)
    {
        const FVector Sample = Point + FVector(0, 0, 3);
        FVector2D Pixel;
        if (FSceneView::ProjectWorldToScreen(Sample, Rect, ViewProjection, Pixel)
            && Pixel.X >= Rect.Min.X && Pixel.X < Rect.Max.X && Pixel.Y >= Rect.Min.Y && Pixel.Y < Rect.Max.Y
            && !GetWorld()->LineTraceTestByChannel(ViewOrigin, Sample, GirlSaviorCombatChannels::Occluder))
        {
            return true;
        }
    }
    return false;
}

int64 AGirlSaviorCombatEncounter::AllocateAttackId()
{
    return ++LastAttackId;
}

void AGirlSaviorCombatEncounter::UnregisterEnemy(AGirlSaviorEnemyCharacter* Enemy)
{
    LiveEnemies.Remove(Enemy);
}

bool AGirlSaviorCombatEncounter::RegisterProjectile(AGirlSaviorTestProjectile* Projectile)
{
    if (RunState != EGirlSaviorRunState::Active || !IsValid(Projectile))
    {
        return false;
    }
    LiveProjectiles.Add(Projectile);
    return true;
}

void AGirlSaviorCombatEncounter::UnregisterProjectile(AGirlSaviorTestProjectile* Projectile)
{
    LiveProjectiles.Remove(Projectile);
}

void AGirlSaviorCombatEncounter::OnHeroDied(const FGirlSaviorDamageResult& Result)
{
    if (RunState == EGirlSaviorRunState::Active)
    {
        DefeatResult = Result;
        CloseRun(EGirlSaviorRunState::Defeated);
    }
}

// Freeze combat immediately. Projectile removal waits for the dispatch frame to finish.
void AGirlSaviorCombatEncounter::CloseRun(EGirlSaviorRunState TerminalState)
{
    if (RunState != EGirlSaviorRunState::Active)
    {
        return;
    }
    RunState = TerminalState;
    OutcomeVisibleAt = GetWorld()->GetTimeSeconds() + (TerminalState == EGirlSaviorRunState::Defeated ? 1.25 : 0.0);
    if (auto* Controller = Cast<AGirlSaviorPlayerController>(Hero->GetController()))
    {
        Controller->SetGameplayInputAvailable(false);
    }
    Hero->GetCharacterMovement()->DisableMovement();
    const auto Enemies = MoveTemp(LiveEnemies);
    LiveEnemies.Reset();
    for (const auto& Enemy : Enemies)
    {
        if (Enemy.IsValid())
        {
            Enemy->StopForEncounter();
            Enemy->SetActorEnableCollision(false);
            PendingRetirements.Add(Enemy);
        }
    }
    bCleanupPending = true;
    UE_LOG(LogGirlSaviorEncounter, Display, TEXT("First fight ended (%s)."),
        TerminalState == EGirlSaviorRunState::Defeated ? TEXT("defeat") : TEXT("success"));
}

bool AGirlSaviorCombatEncounter::IsOutcomeVisible() const
{
    return (RunState == EGirlSaviorRunState::Defeated || RunState == EGirlSaviorRunState::Succeeded)
        && GetWorld()->GetTimeSeconds() >= OutcomeVisibleAt;
}

bool AGirlSaviorCombatEncounter::Retry()
{
    if (bRetryRequested || !IsOutcomeVisible())
    {
        return false;
    }
    bRetryRequested = true;
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
    return true;
}

void AGirlSaviorCombatEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Hero.IsValid())
    {
        Hero->GetDamageReceiver()->OnDied.RemoveAll(this);
    }
    LiveProjectiles.Reset();
    LiveEnemies.Reset();
    PendingRetirements.Reset();
    Super::EndPlay(EndPlayReason);
}
