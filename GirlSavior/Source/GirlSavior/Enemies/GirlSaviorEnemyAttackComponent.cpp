#include "Enemies/GirlSaviorEnemyAttackComponent.h"

#include "Characters/GirlSaviorCharacter.h"
#include "Combat/GirlSaviorCombatChannels.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorTestProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Encounters/GirlSaviorCombatEncounter.h"
#include "DrawDebugHelpers.h"
#include "Enemies/GirlSaviorEnemyAIController.h"
#include "Enemies/GirlSaviorEnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorEnemyAttack, Log, All);

namespace
{
constexpr int32 FanSegments = 16;
constexpr double WarningFloorOffset = 3.0;
constexpr double SwingDisplaySeconds = 0.14;
constexpr double ChargeLegLength = 20.0;
constexpr ECollisionChannel CombatOccluder = GirlSaviorCombatChannels::Occluder;

// A center inside the fan or any touching edge counts as a capsule contact.
bool IntersectsDisc(TConstArrayView<FVector> Polygon, const FVector& Center, const float Radius)
{
    if (Polygon.Num() < 3)
    {
        return false;
    }
    bool bInside = false;
    const FVector2D Point(Center.X, Center.Y);
    for (int32 Index = 0, Previous = Polygon.Num() - 1; Index < Polygon.Num(); Previous = Index++)
    {
        const FVector2D A(Polygon[Previous].X, Polygon[Previous].Y);
        const FVector2D B(Polygon[Index].X, Polygon[Index].Y);
        const FVector2D Edge = B - A;
        const double LengthSquared = Edge.SizeSquared();
        const double Alpha = LengthSquared > UE_SMALL_NUMBER
            ? FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / LengthSquared, 0.0, 1.0) : 0.0;
        if (FVector2D::DistSquared(Point, A + Alpha * Edge) <= FMath::Square(double(Radius)))
        {
            return true;
        }
        if ((A.Y > Point.Y) != (B.Y > Point.Y) && Point.X < A.X + Edge.X * (Point.Y - A.Y) / Edge.Y)
        {
            bInside = !bInside;
        }
    }
    return bInside;
}
}

UGirlSaviorEnemyAttackComponent::UGirlSaviorEnemyAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UGirlSaviorEnemyAttackComponent::ActivateForEncounter(AGirlSaviorCombatEncounter& Encounter,
    const UGirlSaviorEnemyDefinition& Definition, ULineBatchComponent& WarningVisual)
{
    Enemy = CastChecked<AGirlSaviorEnemyCharacter>(GetOwner());
    OwningEncounter = &Encounter;
    Visual = &WarningVisual;
    Role = Definition.Role;
    Cue.Role = Role;
    Pursuer = Definition.Pursuer;
    Shooter = Definition.Shooter;
    Charger = Definition.Charger;
    switch (Role)
    {
    case EGirlSaviorEnemyRole::Pursuer:
        Preparation = Pursuer.Preparation;
        LockedTail = Pursuer.LockedTail;
        Recovery = Pursuer.Recovery;
        break;
    case EGirlSaviorEnemyRole::Shooter:
        Preparation = Shooter.Preparation;
        LockedTail = Shooter.LockedTail;
        Recovery = Shooter.Recovery;
        break;
    case EGirlSaviorEnemyRole::Charger:
        Preparation = Charger.Preparation;
        LockedTail = Charger.LockedTail;
        Recovery = Charger.Recovery;
        break;
    }
    AddTickPrerequisiteActor(&Encounter);
    SetComponentTickEnabled(true);
}

void UGirlSaviorEnemyAttackComponent::StopForEncounter()
{
    SetComponentTickEnabled(false);
    CancelAttack();
    if (AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get())
    {
        RemoveTickPrerequisiteActor(Encounter);
    }
    OwningEncounter.Reset();
}

void UGirlSaviorEnemyAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopForEncounter();
    Super::EndPlay(EndPlayReason);
}

void UGirlSaviorEnemyAttackComponent::CancelForTargetChange()
{
    if (Cue.Phase == EGirlSaviorEnemyAttackPhase::Preparation || Cue.Phase == EGirlSaviorEnemyAttackPhase::Active)
    {
        CancelAttack();
    }
}

void UGirlSaviorEnemyAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    AGirlSaviorEnemyCharacter* Body = Enemy.Get();
    AGirlSaviorCharacter* Hero = Encounter ? Encounter->GetHero() : nullptr;
    if (!GetWorld() || !Hero || !Body || !Visual.IsValid() || !Encounter->CanEnemyEngage(*Body))
    {
        CancelAttack();
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (SwingClearAt > 0.0 && Now >= SwingClearAt)
    {
        Visual->Flush();
        SwingClearAt = 0.0;
    }
    switch (Cue.Phase)
    {
    case EGirlSaviorEnemyAttackPhase::Approach:
        TryStartAttack(*Hero, Now);
        break;
    case EGirlSaviorEnemyAttackPhase::Preparation:
        UpdatePreparation(*Hero, Now);
        break;
    case EGirlSaviorEnemyAttackPhase::Recovery:
        if (Now >= Cue.PhaseEnd)
        {
            Cue = {};
            Cue.Role = Role;
            PublishCue();
        }
        break;
    case EGirlSaviorEnemyAttackPhase::Active:
        if (Role == EGirlSaviorEnemyRole::Charger)
        {
            AdvanceCharge(*Hero, Now, DeltaTime);
        }
        break;
    }
}

FVector UGirlSaviorEnemyAttackComponent::DirectionTo(const AGirlSaviorCharacter& Hero) const
{
    const FVector ToHero = (Hero.GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
    return ToHero.IsNearlyZero() ? Enemy->GetActorForwardVector().GetSafeNormal2D() : ToHero;
}

bool UGirlSaviorEnemyAttackComponent::ShouldHoldPosition(const AGirlSaviorCharacter& Hero) const
{
    const auto* Body = Enemy.Get();
    if (!Body || !Visual.IsValid())
    {
        return false;
    }
    if (Role == EGirlSaviorEnemyRole::Pursuer)
    {
        const double Reach = Pursuer.Reach + Hero.GetCapsuleComponent()->GetScaledCapsuleRadius();
        return FVector::DistSquared2D(Body->GetActorLocation(), Hero.GetActorLocation()) <= FMath::Square(Reach);
    }
    return CanThreatenFrom(Body->GetActorLocation(), Hero);
}

// Use the same geometry for AI candidate positions and actual admission.
bool UGirlSaviorEnemyAttackComponent::CanThreatenFrom(const FVector& Start, const AGirlSaviorCharacter& Hero) const
{
    const auto* Encounter = OwningEncounter.Get();
    const auto* Body = Enemy.Get();
    if (!Encounter || !Body || !Visual.IsValid() || !Body->IsInFixtureRoom(Start))
    {
        return false;
    }
    const auto* Capsule = Body->GetCapsuleComponent();
    const auto* HeroCapsule = Hero.GetCapsuleComponent();
    const FVector Target = HeroCapsule->GetComponentLocation();
    const double Distance = FVector::Dist2D(Start, Target);
    const bool bShooter = Role == EGirlSaviorEnemyRole::Shooter;
    const double MinRange = bShooter ? 0.0 : Charger.MinRange;
    const double MaxRange = bShooter ? Shooter.Range : Charger.MaxRange;
    if (Distance < MinRange || Distance > MaxRange || Distance <= UE_SMALL_NUMBER
        || FMath::Abs(Target.Z - Start.Z) > HeroCapsule->GetScaledCapsuleHalfHeight() - HeroCapsule->GetScaledCapsuleRadius()
        || !Encounter->IsBodyInView(Start, Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()))
    {
        return false;
    }
    const FVector Direction = (Target - Start).GetSafeNormal2D();
    const double Range = MeasureStraightRange(Start, Direction, bShooter ? Shooter.ProjectileRange : Charger.MaxTravel);
    const float StripeHalfWidth = bShooter ? 0.f : Capsule->GetScaledCapsuleRadius();
    return Range >= Distance - HeroCapsule->GetScaledCapsuleRadius() - (bShooter ? Shooter.ProjectileRadius : StripeHalfWidth)
        && Encounter->IsStraightWarningVisible(FloorPoint(Start), FloorPoint(Start + Direction * Range), StripeHalfWidth);
}

FVector UGirlSaviorEnemyAttackComponent::FloorPoint(const FVector& BodyPoint) const
{
    return BodyPoint - FVector(0, 0, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - WarningFloorOffset);
}

bool UGirlSaviorEnemyAttackComponent::HasChargeSupport(const FVector& Position) const
{
    const FVector Foot = FloorPoint(Position);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorChargeFloor), false, Enemy.Get());
    FHitResult Floor;
    const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::InitType::AllStaticObjects);
    return GetWorld()->LineTraceSingleByObjectType(Floor, Foot + FVector(0, 0, 4), Foot - FVector(0, 0, 10),
        Objects, Query) && Enemy->GetCharacterMovement()->IsWalkable(Floor);
}

double UGirlSaviorEnemyAttackComponent::MeasureStraightRange(const FVector& Start, const FVector& Direction,
    double MaxRange) const
{
    const bool bShooter = Role == EGirlSaviorEnemyRole::Shooter;
    const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
    const float Radius = bShooter ? Shooter.ProjectileRadius : Capsule->GetScaledCapsuleRadius();
    const FCollisionShape Shape = bShooter ? FCollisionShape::MakeSphere(Radius)
        : FCollisionShape::MakeCapsule(Radius, Capsule->GetScaledCapsuleHalfHeight());
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorStraightWarning), false, Enemy.Get());
    const FVector End = Start + Direction * MaxRange;
    FHitResult Wall;
    if (MaxRange > 0.0 && GetWorld()->SweepSingleByChannel(Wall, Start, End, FQuat::Identity, CombatOccluder, Shape, Query))
    {
        MaxRange *= Wall.Time;
    }
    const auto* Encounter = OwningEncounter.Get();
    if (Encounter->CrossesSafeArea(Start, Start + Direction * MaxRange, Radius))
    {
        double Clear = 0.0;
        double Blocked = MaxRange;
        for (int32 Index = 0; Index < 12; ++Index)
        {
            const double Mid = (Clear + Blocked) * 0.5;
            if (Encounter->CrossesSafeArea(Start, Start + Direction * Mid, Radius))
            {
                Blocked = Mid;
            }
            else
            {
                Clear = Mid;
            }
        }
        MaxRange = Clear;
    }
    if (!bShooter)
    {
        double Supported = 0.0;
        while (Supported < MaxRange)
        {
            const double Next = FMath::Min(Supported + ChargeLegLength, MaxRange);
            const FVector Position = Start + Direction * Next;
            if (!Enemy->IsInFixtureRoom(Position) || !HasChargeSupport(Position))
            {
                return Supported;
            }
            Supported = Next;
        }
    }
    return MaxRange;
}

void UGirlSaviorEnemyAttackComponent::UpdateShape(bool bLimitToLockedRange)
{
    if (Role == EGirlSaviorEnemyRole::Pursuer)
    {
        BuildFloorPolygon(bLimitToLockedRange ? TConstArrayView<float>(LockedRanges) : TConstArrayView<float>(),
            CurrentRanges);
        return;
    }
    const bool bShooter = Role == EGirlSaviorEnemyRole::Shooter;
    const double MaxRange = bLimitToLockedRange ? LockedStraightRange
        : (bShooter ? Shooter.ProjectileRange : Charger.MaxTravel);
    StraightRange = MeasureStraightRange(Cue.Origin, Cue.Direction, MaxRange);
    Cue.FloorStart = FloorPoint(Cue.Origin);
    Cue.FloorEnd = FloorPoint(Cue.Origin + Cue.Direction * StraightRange);
    Cue.HalfWidth = bShooter ? 0.f : Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

bool UGirlSaviorEnemyAttackComponent::CanSeeWarning() const
{
    if (Role == EGirlSaviorEnemyRole::Pursuer)
    {
        return OwningEncounter->IsWarningVisible(Cue.FloorPolygon);
    }
    return OwningEncounter->IsStraightWarningVisible(Cue.FloorStart, Cue.FloorEnd, Cue.HalfWidth);
}

bool UGirlSaviorEnemyAttackComponent::TryStartAttack(AGirlSaviorCharacter& Hero, const double Now)
{
    const AGirlSaviorEnemyCharacter* Body = Enemy.Get();
    AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    if (!Body->GetCharacterMovement()->IsMovingOnGround())
    {
        return false;
    }
    if (Role != EGirlSaviorEnemyRole::Pursuer)
    {
        if (Now < NextHeavyAdmission)
        {
            return false;
        }
        NextHeavyAdmission = Now + 0.20;
        if (Encounter->HasDangerousAttack())
        {
            return false;
        }
    }
    if (!ShouldHoldPosition(Hero))
    {
        return false;
    }

    Cue = {};
    Cue.Role = Role;
    Cue.Origin = Body->GetActorLocation();
    Cue.Direction = DirectionTo(Hero);
    UpdateShape(false);
    if ((Role == EGirlSaviorEnemyRole::Pursuer && !CanContact(Hero)) || !CanSeeWarning())
    {
        Cue = {};
        return false;
    }

    const int64 AttackId = Encounter->AllocateAttackId();
    if (Role != EGirlSaviorEnemyRole::Pursuer && !Encounter->TryAcquireDangerousAttack(AttackId, *this))
    {
        Cue = {};
        return false;
    }
    Cue.AttackId = AttackId;
    Cue.Phase = EGirlSaviorEnemyAttackPhase::Preparation;
    Cue.PhaseStart = Now;
    Cue.PhaseEnd = Now + Preparation;
    LockAt = Cue.PhaseEnd - LockedTail;
    if (AGirlSaviorEnemyAIController* AI = Cast<AGirlSaviorEnemyAIController>(Enemy->GetController()))
    {
        AI->PauseApproach();
    }
    Enemy->GetCharacterMovement()->StopMovementImmediately();
    Enemy->SetActorRotation(Cue.Direction.Rotation());
    ShowShape(false);
    PublishCue();
    UE_LOG(LogGirlSaviorEnemyAttack, Log, TEXT("Role %d attack %lld started at %.3f."),
        static_cast<int32>(Role), AttackId, Now);
    return true;
}

void UGirlSaviorEnemyAttackComponent::UpdateUnlockedShape(const AGirlSaviorCharacter& Hero)
{
    Cue.Origin = Enemy->GetActorLocation();
    Cue.Direction = DirectionTo(Hero);
    Enemy->SetActorRotation(Cue.Direction.Rotation());
    UpdateShape(false);
}

void UGirlSaviorEnemyAttackComponent::UpdatePreparation(AGirlSaviorCharacter& Hero, const double Now)
{
    // Lock the ray lengths as well as the direction, so opening cover cannot expand a warned attack.
    if (!Cue.bLocked)
    {
        UpdateUnlockedShape(Hero);
        if (Now >= LockAt)
        {
            Cue.bLocked = true;
            LockedRanges = CurrentRanges;
            LockedStraightRange = StraightRange;
            Cue.PhaseEnd = FMath::Max(Cue.PhaseEnd, Now + LockedTail);
        }
        ShowShape(false);
        PublishCue();
        return; // A late frame must present the locked cue before it can commit.
    }
    if (Now >= Cue.PhaseEnd)
    {
        CommitAttack(Hero, Now);
        return;
    }
    UpdateShape(true);
    ShowShape(false);
    PublishCue();
}

// Clip each body-height ray, then project the result onto the flat CP1 floor.
void UGirlSaviorEnemyAttackComponent::BuildFloorPolygon(TConstArrayView<float> MaximumRanges, TArray<float>& OutRanges)
{
    check(MaximumRanges.IsEmpty() || MaximumRanges.Num() == FanSegments + 1);
    const float HalfHeight = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const double FloorZ = Cue.Origin.Z - HalfHeight + WarningFloorOffset;
    Cue.FloorPolygon.Reset(FanSegments + 2);
    Cue.FloorPolygon.Emplace(Cue.Origin.X, Cue.Origin.Y, FloorZ);
    OutRanges.Reset(FanSegments + 1);

    const AGirlSaviorCombatEncounter* Encounter = OwningEncounter.Get();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorPursuerFan), false, Enemy.Get());
    for (int32 Index = 0; Index <= FanSegments; ++Index)
    {
        const double Angle = FMath::Lerp(-double(Pursuer.HalfAngleDegrees),
            double(Pursuer.HalfAngleDegrees), Index / double(FanSegments));
        const FVector Ray = Cue.Direction.RotateAngleAxis(Angle, FVector::UpVector);
        float Reach = MaximumRanges.IsEmpty() ? Pursuer.Reach : MaximumRanges[Index];
        FHitResult Occluder;
        if (GetWorld()->LineTraceSingleByChannel(Occluder, Cue.Origin, Cue.Origin + Ray * Reach, CombatOccluder, Query))
        {
            Reach *= Occluder.Time;
        }
        if (Encounter->CrossesSafeArea(Cue.Origin, Cue.Origin + Ray * Reach))
        {
            float Open = 0.f;
            float Closed = Reach;
            // Narrow to the boundary without requiring knowledge of the safe volume's transform.
            for (int32 Step = 0; Step < 8; ++Step)
            {
                const float Mid = (Open + Closed) * 0.5f;
                if (Encounter->CrossesSafeArea(Cue.Origin, Cue.Origin + Ray * Mid))
                {
                    Closed = Mid;
                }
                else
                {
                    Open = Mid;
                }
            }
            Reach = Open;
        }
        OutRanges.Add(Reach);
        FVector Point = Cue.Origin + Ray * Reach;
        Point.Z = FloorZ;
        Cue.FloorPolygon.Add(Point);
    }
}

bool UGirlSaviorEnemyAttackComponent::CanContact(const AGirlSaviorCharacter& Hero) const
{
    const UCapsuleComponent* HeroCapsule = Hero.GetCapsuleComponent();
    const FVector Center = HeroCapsule->GetComponentLocation();
    const float VerticalExtent = HeroCapsule->GetScaledCapsuleHalfHeight()
        + Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (FMath::Abs(Center.Z - Cue.Origin.Z) > VerticalExtent
        || !IntersectsDisc(Cue.FloorPolygon, Center, HeroCapsule->GetScaledCapsuleRadius())
        || OwningEncounter->CrossesSafeArea(Cue.Origin, Center))
    {
        return false;
    }
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorPursuerContact), false, Enemy.Get());
    return !GetWorld()->LineTraceTestByChannel(Cue.Origin, Center, CombatOccluder, Query);
}

void UGirlSaviorEnemyAttackComponent::CommitAttack(AGirlSaviorCharacter& Hero, const double Now)
{
    UpdateShape(true);
    if (!CanSeeWarning())
    {
        CancelAttack();
        return;
    }

    const int64 AttackId = Cue.AttackId;
    Cue.Phase = EGirlSaviorEnemyAttackPhase::Active;
    Cue.PhaseStart = Now;
    ChargeRemaining = Role == EGirlSaviorEnemyRole::Charger ? StraightRange : 0.0;
    Cue.PhaseEnd = Role == EGirlSaviorEnemyRole::Charger ? Now + ChargeRemaining / Charger.ChargeSpeed : Now;
    if (Role == EGirlSaviorEnemyRole::Charger)
    {
        Enemy->GetCharacterMovement()->DisableMovement();
        bSuspendedWalking = true;
    }
    SwingClearAt = Now + SwingDisplaySeconds;
    ShowShape(true);
    PublishCue();
    if (Cue.AttackId != AttackId)
    {
        return;
    }
    if (Role == EGirlSaviorEnemyRole::Shooter)
    {
        LaunchShooter(Now);
        return;
    }
    if (Role == EGirlSaviorEnemyRole::Charger)
    {
        SwingClearAt = 0.0;
        return; // The first movement step occurs on the next update.
    }

    EGirlSaviorDamageOutcome Outcome = EGirlSaviorDamageOutcome::Rejected;
    if (CanContact(Hero))
    {
        if (UGirlSaviorDamageReceiverComponent* Receiver = Hero.GetDamageReceiver())
        {
            FGirlSaviorDamageSpec Spec;
            Spec.Physical = Pursuer.PhysicalDamage;
            Spec.Delivery = EGirlSaviorDamageDelivery::DirectMelee;
            Spec.SourceFaction = EGirlSaviorFaction::Enemy;
            Spec.SourceId = TEXT("Enemy.Pursuer.Melee");
            Spec.Cause = NSLOCTEXT("GirlSaviorCombat", "PursuerMelee", "Удар преследователя");
            Spec.Instigator = Enemy.Get();
            Spec.Causer = Enemy.Get();
            Spec.AttackId = AttackId;
            Spec.Origin = Cue.Origin;
            const FVector ToHero = (Hero.GetActorLocation() - Cue.Origin).GetSafeNormal2D();
            Spec.Contact = Hero.GetActorLocation() - ToHero * Hero.GetCapsuleComponent()->GetScaledCapsuleRadius();
            Spec.ContactNormal = -ToHero;
            Spec.bHasContact = true;
            Outcome = Receiver->ResolveDamage(Spec).Outcome;
        }
    }
    UE_LOG(LogGirlSaviorEnemyAttack, Log, TEXT("Pursuer attack %lld swung at %.3f (outcome %d)."),
        AttackId, Now, static_cast<int32>(Outcome));
    if (Cue.AttackId != AttackId)
    {
        return; // A lethal hit may already have stopped the encounter and cancelled the cue.
    }
    EnterRecovery(Now, true);
}

void UGirlSaviorEnemyAttackComponent::LaunchShooter(double Now)
{
    const int64 AttackId = Cue.AttackId;
    const FVector Direction = Cue.Direction;
    FGirlSaviorProjectileLaunch Launch;
    Launch.Radius = Shooter.ProjectileRadius;
    Launch.Speed = Shooter.ProjectileSpeed;
    // Use the smaller commit extent, and let live movement resolve any impact inside it.
    Launch.MaxDistance = static_cast<float>(StraightRange);
    Launch.bResolveClippedEndContact = StraightRange < Shooter.ProjectileRange;
    Launch.Damage.Physical = Shooter.PhysicalDamage;
    Launch.Damage.Delivery = EGirlSaviorDamageDelivery::DirectProjectile;
    Launch.Damage.SourceFaction = EGirlSaviorFaction::Enemy;
    Launch.Damage.SourceId = TEXT("Enemy.Shooter.Projectile");
    Launch.Damage.Cause = NSLOCTEXT("GirlSaviorCombat", "ShooterProjectile", "Снаряд стрелка");
    Launch.Damage.Origin = Cue.Origin;
    Launch.Damage.Instigator = Enemy.Get();
    Launch.Damage.AttackId = AttackId;

    EnterRecovery(Now, false);
    if (Cue.AttackId != AttackId)
    {
        return;
    }
    // The detached attack ends from the projectile, including an immediate impact at launch.
    Cue.AttackId = 0;
    auto* Encounter = OwningEncounter.Get();
    if (!AGirlSaviorTestProjectile::SpawnCombatProjectile(*GetWorld(), Shooter.ProjectileClass,
        Enemy.Get(), Direction, Launch, *Encounter, this,
        FGirlSaviorProjectileCompleted::FDelegate::CreateUObject(this, &ThisClass::OnProjectileCompleted)))
    {
        Encounter->ReleaseDangerousAttack(AttackId, *this);
        OnAttackEnded.Broadcast(AttackId, EGirlSaviorAttackEndReason::Cancelled);
    }
}

void UGirlSaviorEnemyAttackComponent::OnProjectileCompleted(AGirlSaviorTestProjectile* Projectile,
    EGirlSaviorProjectileCompletion Reason)
{
    OnAttackEnded.Broadcast(Projectile->GetAttackId(), Reason == EGirlSaviorProjectileCompletion::Removed
        ? EGirlSaviorAttackEndReason::Cancelled : EGirlSaviorAttackEndReason::Completed);
}

// A low frame rate consumes more short sweeps in the same locked direction.
void UGirlSaviorEnemyAttackComponent::AdvanceCharge(AGirlSaviorCharacter& Hero, double Now, float DeltaSeconds)
{
    AGirlSaviorEnemyCharacter* Body = Enemy.Get();
    UCapsuleComponent* Capsule = Body->GetCapsuleComponent();
    double Budget = FMath::Min(ChargeRemaining, double(Charger.ChargeSpeed) * DeltaSeconds);
    while (Budget > UE_KINDA_SMALL_NUMBER)
    {
        const FVector Start = Capsule->GetComponentLocation();
        const double Step = FMath::Min(Budget, ChargeLegLength);
        const FVector End = Start + Cue.Direction * Step;
        if (!Body->IsInFixtureRoom(End) || !HasChargeSupport(End)
            || OwningEncounter->CrossesSafeArea(Start, End, Cue.HalfWidth))
        {
            FinishCharge(nullptr, Now);
            return;
        }
        FHitResult Hit;
        const int64 AttackId = Cue.AttackId;
        Capsule->MoveComponent(End - Start, Capsule->GetComponentQuat(), true, &Hit);
        if (Cue.AttackId != AttackId)
        {
            return;
        }
        if (Hit.bBlockingHit)
        {
            if (Hit.GetActor() == &Hero)
            {
                FGirlSaviorDamageSpec Spec;
                Spec.Physical = Charger.PhysicalDamage;
                Spec.Delivery = EGirlSaviorDamageDelivery::DirectCharge;
                Spec.SourceFaction = EGirlSaviorFaction::Enemy;
                Spec.SourceId = TEXT("Enemy.Charger.Charge");
                Spec.Cause = NSLOCTEXT("GirlSaviorCombat", "ChargerCharge", "Бросок таранщика");
                Spec.Instigator = Body;
                Spec.Causer = Body;
                Spec.AttackId = AttackId;
                Spec.Origin = Cue.Origin;
                Spec.Contact = Hit.ImpactPoint;
                Spec.ContactNormal = Hit.ImpactNormal;
                Spec.bHasContact = true;
                Hero.GetDamageReceiver()->ResolveDamage(Spec);
            }
            if (Cue.AttackId == AttackId)
            {
                FinishCharge(&Hit, Now);
            }
            return;
        }
        ChargeRemaining -= Step;
        Budget -= Step;
    }

    if (ChargeRemaining <= UE_KINDA_SMALL_NUMBER)
    {
        // At a preview-clipped wall the locked extent may stop exactly at first touch.
        const FVector Position = Capsule->GetComponentLocation();
        FHitResult Wall;
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorChargeEnd), false, Body);
        const bool bWall = GetWorld()->SweepSingleByChannel(Wall, Position, Position + Cue.Direction * 2.0,
            FQuat::Identity, CombatOccluder, Capsule->GetCollisionShape(), Query);
        FinishCharge(bWall ? &Wall : nullptr, Now);
        return;
    }
    const FVector Start = Capsule->GetComponentLocation();
    const double OpenRange = MeasureStraightRange(Start, Cue.Direction, ChargeRemaining);
    Cue.FloorStart = FloorPoint(Start);
    Cue.FloorEnd = FloorPoint(Start + Cue.Direction * OpenRange);
    ShowShape(false);
    PublishCue();
}

void UGirlSaviorEnemyAttackComponent::FinishCharge(const FHitResult* Hit, double Now)
{
    if (Hit && (!Hit->GetActor() || !Hit->GetActor()->IsA<APawn>()))
    {
        DrawDebugSphere(GetWorld(), Hit->ImpactPoint, 24.f, 10, FColor::Orange, false, 0.2f, 0, 2.f);
    }
    UE_LOG(LogGirlSaviorEnemyAttack, Log, TEXT("Charge attack %lld stopped against %s at %s."), Cue.AttackId,
        Hit ? *GetNameSafe(Hit->GetActor()) : TEXT("range or floor limit"), *Enemy->GetActorLocation().ToCompactString());
    EnterRecovery(Now, true);
}

void UGirlSaviorEnemyAttackComponent::RestoreWalking()
{
    if (bSuspendedWalking)
    {
        bSuspendedWalking = false;
        if (auto* Body = Enemy.Get())
        {
            Body->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
    }
}

void UGirlSaviorEnemyAttackComponent::EnterRecovery(double Now, bool bFinishCue)
{
    RestoreWalking();
    if (Role != EGirlSaviorEnemyRole::Pursuer)
    {
        Visual->Flush();
        SwingClearAt = 0.0;
    }
    const int64 AttackId = Cue.AttackId;
    Cue.Phase = EGirlSaviorEnemyAttackPhase::Recovery;
    Cue.PhaseStart = Now;
    Cue.PhaseEnd = Now + Recovery;
    Cue.FloorPolygon.Reset();
    Cue.FloorEnd = Cue.FloorStart;
    PublishCue();
    if (bFinishCue && Cue.AttackId == AttackId)
    {
        NotifyAttackEnded(EGirlSaviorAttackEndReason::Completed);
    }
}

void UGirlSaviorEnemyAttackComponent::CancelAttack()
{
    if (Cue.Phase == EGirlSaviorEnemyAttackPhase::Approach && SwingClearAt <= 0.0)
    {
        return;
    }
    if (ULineBatchComponent* Batch = Visual.Get())
    {
        Batch->Flush();
    }
    SwingClearAt = 0.0;
    const int64 AttackId = Cue.AttackId;
    Cue = {};
    Cue.Role = Role;
    RestoreWalking();
    if (AttackId != 0)
    {
        if (auto* Encounter = OwningEncounter.Get())
        {
            Encounter->ReleaseDangerousAttack(AttackId, *this);
        }
        OnAttackEnded.Broadcast(AttackId, EGirlSaviorAttackEndReason::Cancelled);
    }
}

void UGirlSaviorEnemyAttackComponent::NotifyAttackEnded(EGirlSaviorAttackEndReason Reason)
{
    const int64 AttackId = Cue.AttackId;
    if (auto* Encounter = OwningEncounter.Get())
    {
        Encounter->ReleaseDangerousAttack(AttackId, *this);
    }
    Cue.AttackId = 0;
    if (AttackId != 0)
    {
        OnAttackEnded.Broadcast(AttackId, Reason);
    }
}

void UGirlSaviorEnemyAttackComponent::PublishCue()
{
    const FGirlSaviorAttackCue Snapshot = Cue;
    OnAttackCue.Broadcast(Snapshot);
}

// World-depth batched lines make the same clipped shape visible in normal runtime and PIE.
void UGirlSaviorEnemyAttackComponent::ShowShape(const bool bSwing) const
{
    ULineBatchComponent* Batch = Visual.Get();
    Batch->Flush();
    if (Role != EGirlSaviorEnemyRole::Pursuer)
    {
        const FLinearColor Color = Role == EGirlSaviorEnemyRole::Shooter
            ? FLinearColor(1.f, Cue.bLocked ? 0.23f : 0.52f, 0.10f)
            : FLinearColor(Cue.bLocked ? 1.f : 0.75f, 0.08f, 0.44f);
        const float Width = Cue.bLocked ? 7.f : 4.f;
        const FVector Edge = FVector::CrossProduct(FVector::UpVector, Cue.Direction) * Cue.HalfWidth;
        if (Role == EGirlSaviorEnemyRole::Charger)
        {
            Batch->DrawLine(Cue.FloorStart - Edge, Cue.FloorEnd - Edge, Color, SDPG_World, Width, 0.f);
            Batch->DrawLine(Cue.FloorStart + Edge, Cue.FloorEnd + Edge, Color, SDPG_World, Width, 0.f);
            for (int32 Index = 1; Index <= 4; ++Index)
            {
                const FVector Point = FMath::Lerp(Cue.FloorStart, Cue.FloorEnd, Index * 0.25);
                Batch->DrawLine(Point - Edge, Point + Edge, Color, SDPG_World, 2.f, 0.f);
            }
        }
        else
        {
            Batch->DrawLine(Cue.FloorStart, Cue.FloorEnd, Color, SDPG_World, Width, 0.f);
        }
        return;
    }
    const FLinearColor Outline = bSwing ? FLinearColor(1.f, 0.84f, 0.55f) : Cue.bLocked
        ? FLinearColor(1.f, 0.12f, 0.035f) : FLinearColor(1.f, 0.53f, 0.085f);
    const float Thickness = bSwing ? 9.f : Cue.bLocked ? 6.f : 4.f;
    TArray<FBatchedLine, TInlineAllocator<64>> Lines;
    const FVector& Base = Cue.FloorPolygon[0];
    const int32 Count = Cue.FloorPolygon.Num();
    for (int32 Previous = Count - 1, Index = 0; Index < Count; Previous = Index++)
    {
        Lines.Emplace(Cue.FloorPolygon[Previous], Cue.FloorPolygon[Index], Outline, 0.f, Thickness, SDPG_World);
        if (!bSwing && Previous > 0 && Index > 0)
        {
            for (const float Fraction : {0.45f, 0.72f})
            {
                Lines.Emplace(FMath::Lerp(Base, Cue.FloorPolygon[Previous], Fraction),
                    FMath::Lerp(Base, Cue.FloorPolygon[Index], Fraction), Outline * 0.65f, 0.f, 2.f, SDPG_World);
            }
        }
    }
    Batch->DrawLines(Lines);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorPursuerSectorTest, "GirlSavior.Combat.Pursuer.Sector",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGirlSaviorPursuerSectorTest::RunTest(const FString& Parameters)
{
    const TArray<FVector> ClippedFan = {
        FVector(0, 0, 3), FVector(90, -65, 3), FVector(170, 0, 3), FVector(90, 65, 3)
    };
    TestTrue(TEXT("A capsule center inside the fan is hit"), IntersectsDisc(ClippedFan, FVector(100, 0, 90), 34.f));
    TestTrue(TEXT("A capsule edge meeting the fan is hit"), IntersectsDisc(ClippedFan, FVector(198, 0, 90), 34.f));
    TestFalse(TEXT("Walking sideways beyond the warning avoids the swing"),
        IntersectsDisc(ClippedFan, FVector(100, 110, 90), 34.f));
    TestFalse(TEXT("Clipped sides never use the unclipped attack radius"),
        IntersectsDisc(ClippedFan, FVector(120, -95, 90), 15.f));
    return true;
}
#endif
