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
#include "Encounters/GirlSaviorCombatRoom.h"
#include "Enemies/GirlSaviorEnemyAttackComponent.h"
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
    if (!IsValid(SafeArea) || (bWavesEnabled ? !ValidateWaveRoute() : PlacedEnemies.IsEmpty()) || !BindHero(Pawn))
    {
        UE_LOG(LogGirlSaviorEncounter, Error, TEXT("%s is missing a safe area, enemy, hero vitals or configured pistol."), *GetName());
        return false;
    }

    SafeArea->SafeVolume->SetCollisionResponseToChannel(GirlSaviorCombatChannels::HeroProjectile, ECR_Block);
    SafeArea->SafeVolume->SetCollisionResponseToChannel(GirlSaviorCombatChannels::EnemyProjectile, ECR_Block);
    SafeArea->SafeVolume->SetCollisionResponseToChannel(GirlSaviorCombatChannels::Occluder, ECR_Block);

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
        if (!IsValid(Enemy) || LiveEnemies.Contains(Enemy))
        {
            UE_LOG(LogGirlSaviorEncounter, Error, TEXT("Invalid combat enemy %s."), *GetNameSafe(Enemy));
            return false;
        }
        if (!RegisterEnemy(*Enemy, FindRoomAt(Enemy->GetActorLocation()), true, 0))
        {
            UE_LOG(LogGirlSaviorEncounter, Warning, TEXT("Removing invalid fixture %s."), *Enemy->GetName());
            Enemy->Destroy();
            continue;
        }
        ActivateRegisteredEnemy(*Enemy);
    }
    UE_LOG(LogGirlSaviorEncounter, Display, TEXT("First fight standing by. Enemies=%d"), LiveEnemies.Num());
    return bWavesEnabled || !LiveEnemies.IsEmpty();
}

// Bind a newly possessed prototype hero without resetting an already initialized receiver.
bool AGirlSaviorCombatEncounter::BindHero(AGirlSaviorCharacter& Pawn)
{
    auto* Controller = Cast<AGirlSaviorPlayerController>(Pawn.GetController());
    auto* Receiver = Pawn.GetDamageReceiver();
    if (!Controller || !Controller->IsLocalController()
        || !Pawn.GetFireComponent()->SetCombatEncounter(this))
    {
        return false;
    }
    if (Receiver->HasVitals()
        ? Receiver->GetFaction() != EGirlSaviorFaction::Hero || !Receiver->IsAlive()
        : !Receiver->InitializeVitals(Pawn.CombatVitals, EGirlSaviorFaction::Hero))
    {
        Pawn.GetFireComponent()->SetCombatEncounter(nullptr);
        return false;
    }

    Hero = &Pawn;
    Pawn.GetCapsuleComponent()->SetCollisionResponseToChannel(GirlSaviorCombatChannels::EnemyProjectile, ECR_Block);
    SafeArea->RegisterPawn(SafeArea->SafeVolume->IsOverlappingActor(&Pawn) ? &Pawn : nullptr);
    BindAdmission(*Receiver);
    Receiver->OnDied.AddUObject(this, &ThisClass::OnHeroDied);
    Controller->SetFireAllowed(RunState == EGirlSaviorRunState::Active && !SafeArea->SafeVolume->IsOverlappingActor(&Pawn));
    return true;
}

void AGirlSaviorCombatEncounter::UnbindHero()
{
    if (AGirlSaviorCharacter* Previous = Hero.Get())
    {
        Previous->GetDamageReceiver()->OnDied.RemoveAll(this);
        Previous->GetDamageReceiver()->DamageAdmission.Unbind();
        Previous->GetFireComponent()->SetCombatEncounter(nullptr);
        Previous->GetCapsuleComponent()->SetCollisionResponseToChannel(GirlSaviorCombatChannels::EnemyProjectile, ECR_Ignore);
    }
    Hero.Reset();
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
    if (!bInitialized)
    {
        return;
    }

    if (RunState == EGirlSaviorRunState::Waiting || RunState == EGirlSaviorRunState::Active)
    {
        APawn* Current = UGameplayStatics::GetPlayerPawn(this, 0);
        if (Current != Hero.Get())
        {
            CancelReservation(TEXT("hero-changed"));
            const auto Enemies = LiveEnemies;
            for (const auto& Entry : Enemies)
            {
                if (Entry.Key.IsValid())
                {
                    Entry.Key->GetAttackComponent()->CancelForTargetChange();
                }
            }
            UnbindHero();
            if (auto* Pawn = Cast<AGirlSaviorCharacter>(Current))
            {
                BindHero(*Pawn);
            }
        }
    }
    if (!Hero.IsValid())
    {
        bViewValid = false;
        return;
    }
    CaptureView();
    RefreshHeroRoom();
    switch (RunState)
    {
    case EGirlSaviorRunState::Waiting:
        if (!IsHeroProtected() && (bWavesEnabled ? CurrentRoom.Get() == Rooms[0].Get()
            : FirstRoomVolume->IsOverlappingActor(Hero.Get())))
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
    if (bWavesEnabled && RunState == EGirlSaviorRunState::Active)
    {
        UpdateWaves(GetWorld()->GetTimeSeconds());
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
    const auto* Enemy = Cast<AGirlSaviorEnemyCharacter>(&Source);
    if (!(RunState == EGirlSaviorRunState::Active && Hero.IsValid() && IsValid(SafeArea)
        && Hero->IsLocallyControlled() && Hero->GetDamageReceiver()->IsAlive() && !IsPointInSafeArea(Source.GetActorLocation())
        && !IsHeroProtected()
        && (!Enemy || (Enemy->IsInFixtureRoom(Source.GetActorLocation())
            && Enemy->IsInFixtureRoom(Hero->GetActorLocation())))))
    {
        return false;
    }
    if (!bWavesEnabled || !Enemy)
    {
        return true;
    }
    const FEnemyRecord* Record = LiveEnemies.Find(Enemy);
    if (!Record)
    {
        return false;
    }
    const AGirlSaviorCombatRoom* Room = FindRoomAt(Enemy->GetActorLocation());
    return IsRoomReachable(Room ? Room : Record->Room.Get(), CurrentRoom.Get());
}

bool AGirlSaviorCombatEncounter::IsHeroProtected() const
{
    return !Hero.IsValid() || !IsValid(SafeArea) || IsPointInSafeArea(Hero->GetActorLocation())
        || SafeArea->SafeVolume->IsOverlappingActor(Hero.Get());
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
    for (const FVector& Point : FloorPolygon)
    {
        FVector2D Pixel;
        if (ProjectIntoView(Point + FVector(0, 0, 3), Pixel, true))
        {
            return true;
        }
    }
    return false;
}

bool AGirlSaviorCombatEncounter::ProjectIntoView(const FVector& Point, FVector2D& Pixel, bool bRequireClearRay) const
{
    if (!bViewValid)
    {
        return false;
    }
    const FIntRect& Rect = View.GetConstrainedViewRect();
    return FSceneView::ProjectWorldToScreen(Point, Rect, View.ComputeViewProjectionMatrix(), Pixel)
        && Pixel.X >= Rect.Min.X && Pixel.X < Rect.Max.X && Pixel.Y >= Rect.Min.Y && Pixel.Y < Rect.Max.Y
        && (!bRequireClearRay
            || !GetWorld()->LineTraceTestByChannel(ViewOrigin, Point, GirlSaviorCombatChannels::Occluder));
}

bool AGirlSaviorCombatEncounter::IsBodyInView(const FVector& Center, float Radius, float HalfHeight) const
{
    FVector2D Pixel;
    for (const FVector& Offset : {FVector::ZeroVector, FVector(Radius, 0, 0), FVector(-Radius, 0, 0),
        FVector(0, Radius, 0), FVector(0, -Radius, 0), FVector(0, 0, HalfHeight), FVector(0, 0, -HalfHeight)})
    {
        if (ProjectIntoView(Center + Offset, Pixel, true))
        {
            return true;
        }
    }
    return false;
}

// Require a continuous visible section, including the edges of a stripe at one sample.
bool AGirlSaviorCombatEncounter::IsStraightWarningVisible(
    const FVector& Start, const FVector& End, float StripeHalfWidth) const
{
    const FVector Edge = FVector::CrossProduct(FVector::UpVector, (End - Start).GetSafeNormal2D()) * StripeHalfWidth;
    FVector2D PreviousPixel = FVector2D::ZeroVector;
    bool bPreviousVisible = false;
    bool bPreviousEdgesVisible = false;
    for (int32 Index = 0; Index <= 4; ++Index)
    {
        const FVector Point = FMath::Lerp(Start, End, Index * 0.25) + FVector(0, 0, 3);
        FVector2D Pixel;
        FVector2D EdgePixel;
        const bool bVisible = ProjectIntoView(Point, Pixel, true);
        const bool bEdgesVisible = bVisible && (StripeHalfWidth <= 0.f
            || (ProjectIntoView(Point - Edge, EdgePixel, true) && ProjectIntoView(Point + Edge, EdgePixel, true)));
        if (bVisible && bPreviousVisible && (bEdgesVisible || bPreviousEdgesVisible)
            && FVector2D::DistSquared(PreviousPixel, Pixel) >= FMath::Square(24.0))
        {
            return true;
        }
        if (bVisible)
        {
            PreviousPixel = Pixel;
        }
        bPreviousVisible = bVisible;
        bPreviousEdgesVisible = bEdgesVisible;
    }
    return false;
}

int64 AGirlSaviorCombatEncounter::AllocateAttackId()
{
    return ++LastAttackId;
}

bool AGirlSaviorCombatEncounter::TryAcquireDangerousAttack(int64 AttackId, UObject& Holder)
{
    if (RunState != EGirlSaviorRunState::Active || AttackId <= 0 || HasDangerousAttack() || !IsValid(&Holder))
    {
        return false;
    }
    DangerousAttackId = AttackId;
    DangerousAttackHolder = &Holder;
    UE_LOG(LogGirlSaviorEncounter, Log, TEXT("Dangerous attack %lld acquired by %s."), AttackId, *Holder.GetName());
    return true;
}

bool AGirlSaviorCombatEncounter::TransferDangerousAttack(int64 AttackId, const UObject& Holder, UObject& NewHolder)
{
    if (RunState != EGirlSaviorRunState::Active || AttackId <= 0 || DangerousAttackId != AttackId
        || !DangerousAttackHolder.HasSameIndexAndSerialNumber(TWeakObjectPtr<const UObject>(&Holder))
        || !IsValid(&NewHolder))
    {
        return false;
    }
    DangerousAttackHolder = &NewHolder;
    UE_LOG(LogGirlSaviorEncounter, Log, TEXT("Dangerous attack %lld transferred to %s."), AttackId, *NewHolder.GetName());
    return true;
}

void AGirlSaviorCombatEncounter::ReleaseDangerousAttack(int64 AttackId, const UObject& Holder)
{
    // Compare weak object identity even if the holder is already being destroyed.
    if (AttackId > 0 && DangerousAttackId == AttackId
        && DangerousAttackHolder.HasSameIndexAndSerialNumber(TWeakObjectPtr<const UObject>(&Holder)))
    {
        DangerousAttackId = 0;
        DangerousAttackHolder.Reset();
        UE_LOG(LogGirlSaviorEncounter, Log, TEXT("Dangerous attack %lld released by %s."), AttackId, *Holder.GetName());
    }
}

void AGirlSaviorCombatEncounter::UnregisterEnemy(AGirlSaviorEnemyCharacter* Enemy)
{
    FEnemyRecord Removed;
    if (Enemy && LiveEnemies.RemoveAndCopyValue(Enemy, Removed) && Removed.bPublished)
    {
        OnEnemyRemoved.Broadcast(Enemy);
    }
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
    CancelReservation(TEXT("terminal"));
    DangerousAttackId = 0;
    DangerousAttackHolder.Reset();
    OutcomeVisibleAt = GetWorld()->GetTimeSeconds() + (TerminalState == EGirlSaviorRunState::Defeated ? 1.25 : 0.0);
    if (auto* Controller = Cast<AGirlSaviorPlayerController>(Hero->GetController()))
    {
        Controller->SetGameplayInputAvailable(false);
    }
    Hero->GetCharacterMovement()->DisableMovement();
    const auto Enemies = MoveTemp(LiveEnemies);
    LiveEnemies.Reset();
    for (const auto& Entry : Enemies)
    {
        if (AGirlSaviorEnemyCharacter* Enemy = Entry.Key.Get())
        {
            Enemy->StopForEncounter();
            Enemy->SetActorEnableCollision(false);
            PendingRetirements.Add(Enemy);
            if (Entry.Value.bPublished)
            {
                OnEnemyRemoved.Broadcast(Enemy);
            }
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
    CancelReservation(TEXT("end-play"));
    UnbindHero();
    LiveProjectiles.Reset();
    LiveEnemies.Reset();
    PendingRetirements.Reset();
    CurrentRoom.Reset();
    RouteDoors.Reset();
    ServedIntroductions.Reset();
    LastOutsideApproach.Reset();
    LastCommittedGroupTime.Reset();
    DangerousAttackId = 0;
    DangerousAttackHolder.Reset();
    Super::EndPlay(EndPlayReason);
}
