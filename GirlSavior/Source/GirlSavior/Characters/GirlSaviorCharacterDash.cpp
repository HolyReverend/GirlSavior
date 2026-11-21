#include "Characters/GirlSaviorCharacterMovementComponent.h"

#include "Characters/GirlSaviorDashSettings.h"
#include "Components/CapsuleComponent.h"
#include "Core/GirlSaviorPlayerController.h"
#include "Engine/ScopedMovementUpdate.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorDash, Log, All);

namespace
{
constexpr double MaxDashLegDistance = 10.0;
constexpr double DashPositionTolerance = 0.01;
constexpr int32 EdgeRefinementSteps = 8;
const FName DashHardBlockerTag(TEXT("DashHardBlocker"));

bool IsHardDashBlocker(const FHitResult& Hit)
{
    const AActor* Actor = Hit.GetActor();
    const UPrimitiveComponent* Component = Hit.GetComponent();
    return (Actor && Actor->ActorHasTag(DashHardBlockerTag))
        || (Component && Component->ComponentHasTag(DashHardBlockerTag));
}

// Match the backoff of MoveComponent's blocking sweep so the virtual slide starts outside the wall.
double DashContactFraction(const FVector& Delta, const FHitResult& Hit)
{
    const double Distance = Delta.Size();
    const double Backoff = FMath::Clamp(0.1, 0.1 / Distance, 1.0 / Distance) + 0.001;
    return FMath::Max(0.0, double(Hit.Time) - Backoff);
}
}

void UGirlSaviorCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    if (DashSettings && !DashSettings->IsValidForDash())
    {
        UE_LOG(LogGirlSaviorDash, Warning, TEXT("%s: invalid dash settings in %s."),
            *GetNameSafe(CharacterOwner), *GetNameSafe(DashSettings));
    }
}

bool UGirlSaviorCharacterMovementComponent::IsDashing() const
{
    return MovementMode == MOVE_Custom && CustomMovementMode == DashMode;
}

bool UGirlSaviorCharacterMovementComponent::IsDashProtected() const
{
    return IsDashing() && GetWorld() && GetWorld()->GetTimeSeconds() < DashProtectionEndTime;
}

double UGirlSaviorCharacterMovementComponent::GetDashCooldownRemaining() const
{
    return GetWorld() ? FMath::Max(0.0, NextDashReadyTime - GetWorld()->GetTimeSeconds()) : 0.0;
}

bool UGirlSaviorCharacterMovementComponent::HasDashControl() const
{
    const auto* Controller = CharacterOwner ? Cast<AGirlSaviorPlayerController>(CharacterOwner->GetController()) : nullptr;
    return Controller && Controller->GetPawn() == CharacterOwner && Controller->CanUseGameplayInput()
        && !Controller->IsMoveInputIgnored() && !Controller->IsInSafeArea();
}

// Admission never moves the real capsule or spends the cooldown on a rejected request.
bool UGirlSaviorCharacterMovementComponent::TryStartDash(const FVector& WorldDirection)
{
    if (!HasValidData() || !HasDashControl() || MovementMode != MOVE_Walking
        || GetDashCooldownRemaining() > 0.0 || !DashSettings || !DashSettings->IsValidForDash()
        || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity()
        || CharacterMovementCVars::AsyncCharacterMovement != 0 || WorldDirection.ContainsNaN())
    {
        return false;
    }

    const FVector Direction = WorldDirection.GetSafeNormal2D();
    const FVector InitialVelocity = Direction * (double(DashSettings->Distance) / DashSettings->Duration);
    if (Direction.IsNearlyZero()
        || ProbeDashPath(InitialVelocity, DashSettings->Duration, DashSettings->MinimumTravel)
            + DashPositionTolerance < DashSettings->MinimumTravel)
    {
        return false;
    }

    BeginDash(InitialVelocity);
    return true;
}

void UGirlSaviorCharacterMovementComponent::BeginDash(const FVector& InitialVelocity)
{
    const double Now = GetWorld()->GetTimeSeconds();
    DashVelocity = InitialVelocity;
    Velocity = InitialVelocity;
    DashTimeRemaining = DashSettings->Duration;
    DashProtectionEndTime = Now + DashSettings->ProtectionDuration;
    NextDashReadyTime = Now + DashSettings->Cooldown;
    SetMovementMode(MOVE_Custom, DashMode);
    UE_LOG(LogGirlSaviorDash, Log, TEXT("Dash start t=%.6f speed=%.3f protected=%d protectionUntil=%.6f readyAt=%.6f."),
        Now, InitialVelocity.Size2D(), IsDashProtected(), DashProtectionEndTime, NextDashReadyTime);
}

void UGirlSaviorCharacterMovementComponent::CancelDash()
{
    FinishDash(TEXT("control interrupted"));
}

// External velocity begins from the post-dash movement mode and does not inherit dash velocity.
void UGirlSaviorCharacterMovementComponent::Launch(const FVector& LaunchVelocity)
{
    FinishDash(TEXT("external launch"));
    Super::Launch(LaunchVelocity);
}

void UGirlSaviorCharacterMovementComponent::ApplyAccumulatedForces(float DeltaSeconds)
{
    if (!PendingImpulseToApply.IsZero() || !PendingForceToApply.IsZero())
    {
        FinishDash(TEXT("external force"));
    }
    Super::ApplyAccumulatedForces(DeltaSeconds);
}

void UGirlSaviorCharacterMovementComponent::PerformMovement(float DeltaTime)
{
    // Publish each dash leg independently so a safe-area overlap can stop the remaining legs.
    if (!IsDashing() || !bEnableScopedMovementUpdates)
    {
        Super::PerformMovement(DeltaTime);
        return;
    }
    bEnableScopedMovementUpdates = false;
    Super::PerformMovement(DeltaTime);
    bEnableScopedMovementUpdates = true;
}

void UGirlSaviorCharacterMovementComponent::ClearDashState(const TCHAR* Reason)
{
    UE_LOG(LogGirlSaviorDash, Log, TEXT("Dash end (%s) path=%.3fcm simulated=%.6fs protected=0 readyAt=%.6f."),
        Reason, DashTravel, DashTimeSimulated, NextDashReadyTime);
    DashVelocity = FVector::ZeroVector;
    DashTimeRemaining = 0.0;
    DashTimeSimulated = 0.0;
    DashTravel = 0.0;
    DashProtectionEndTime = 0.0;
    bDashSliding = false;
}

void UGirlSaviorCharacterMovementComponent::FinishDash(const TCHAR* Reason)
{
    if (!IsDashing())
    {
        return;
    }
    ClearDashState(Reason);
    Velocity = FVector::ZeroVector;
    FFindFloorResult Floor;
    const bool bSupported = HasValidData() && FindDashFloor(UpdatedComponent->GetComponentLocation(), Floor)
        && Floor.GetDistanceToFloor() <= MAX_FLOOR_DIST + DashPositionTolerance;
    SetMovementMode(bSupported ? MOVE_Walking : MOVE_Falling);
}

// Use the walking floor limits explicitly because MOVE_Custom clears the cached floor and base.
bool UGirlSaviorCharacterMovementComponent::FindDashFloor(const FVector& Location, FFindFloorResult& Floor) const
{
    if (!UpdatedComponent->IsQueryCollisionEnabled())
    {
        return false;
    }
    const float SearchDistance = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER);
    const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
    // A full downward sweep preserves the tread hit at the rounded bottom of the capsule.
    // The stock floor helper still applies walking slopes, edge tolerance and perch checks.
    FHitResult Downward;
    SweepDashCapsule(Location, Location - FVector(0, 0, SearchDistance), Downward);
    ComputeFloorDist(Location, SearchDistance, SearchDistance, Floor, Radius, &Downward);
    if (Floor.bBlockingHit && !Floor.bLineTrace && ShouldComputePerchResult(Floor.HitResult))
    {
        FFindFloorResult Perch;
        if (!ComputePerchResult(GetValidPerchRadius(), Floor.HitResult,
            SearchDistance + FMath::Max(0.f, PerchAdditionalHeight), Perch))
        {
            return false;
        }
        if (!Floor.IsWalkableFloor())
        {
            Floor.SetFromLineTrace(Perch.HitResult, Floor.FloorDist, FMath::Max(Floor.FloorDist, MIN_FLOOR_DIST), true);
        }
    }
    if (!Floor.IsWalkableFloor() || Floor.HitResult.bStartPenetrating)
    {
        return false;
    }

    // Near an edge the capsule's curved bottom can touch the rim after sinking below
    // the floor surface. Measure clearance to that surface before following a drop.
    const double Bottom = Location.Z - CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float SurfaceGap = static_cast<float>(Bottom - Floor.HitResult.ImpactPoint.Z);
    if (Floor.bLineTrace)
    {
        Floor.LineDist = FMath::Min(Floor.LineDist, SurfaceGap);
    }
    else
    {
        Floor.FloorDist = FMath::Min(Floor.FloorDist, SurfaceGap);
    }
    return true;
}

bool UGirlSaviorCharacterMovementComponent::SweepDashCapsule(
    const FVector& Start, const FVector& End, FHitResult& Hit) const
{
    const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GirlSaviorDash), false, CharacterOwner);
    FCollisionResponseParams Response;
    InitCollisionParams(Query, Response);
    const auto Sweep = [&](const FVector& From)
    {
        return GetWorld()->SweepSingleByChannel(Hit, From, End, Capsule->GetComponentQuat(),
            Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Query, Response);
    };
    if (!Sweep(Start))
    {
        return false;
    }
    const FVector Delta = End - Start;
    // Chaos may classify exact tangency as initial penetration. When the sweep
    // separates from that surface, advance only the tolerance and test the rest.
    if (Hit.bStartPenetrating && Hit.PenetrationDepth <= DashPositionTolerance
        && Delta.SizeSquared() > FMath::Square(DashPositionTolerance) && (Delta | Hit.Normal) > 0.0)
    {
        const double Fraction = DashPositionTolerance / Delta.Size();
        if (!Sweep(Start + Fraction * Delta))
        {
            return false;
        }
        Hit.Time = static_cast<float>(Fraction + (1.0 - Fraction) * Hit.Time);
    }
    return true;
}

// Check the same three swept moves that will be committed later.
bool UGirlSaviorCharacterMovementComponent::QueryDashStep(const FVector& Start, const FVector& HorizontalDelta,
    const FFindFloorResult& StartFloor, const FHitResult& Contact, FDashLeg& Leg) const
{
    float Radius, HalfHeight;
    CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
    const double TargetFloorDistance = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5;
    // At exact contact the floor query can already hit the tread above the capsule's
    // base. Use the stance height to measure the required rise in that case.
    const double FloorDistance = StartFloor.GetDistanceToFloor();
    const double StartClearance = FloorDistance >= 0.0 ? FloorDistance : TargetFloorDistance;
    const double UpDistance = MaxStepHeight + TargetFloorDistance - StartClearance;
    FHitResult StepContact = Contact;
    if (StepContact.bStartPenetrating && StepContact.PenetrationDepth <= DashPositionTolerance)
    {
        StepContact.bStartPenetrating = false;
    }
    if (UpDistance <= 0.0 || !CanStepUp(StepContact) || StepContact.ImpactPoint.Z > Start.Z + HalfHeight - Radius)
    {
        return false;
    }

    const FVector Raised = Start + FVector(0, 0, UpDistance);
    const FVector Forward = Raised + HorizontalDelta;
    FHitResult Hit;
    if (SweepDashCapsule(Start, Raised, Hit) || SweepDashCapsule(Raised, Forward, Hit))
    {
        return false;
    }

    // Use a full capsule for the landing sweep. The CMC's shortened search sweep can
    // report a lower floor behind the rounded tread edge before the capsule has moved over it.
    const FVector Down = Forward - FVector(0, 0, UpDistance + MaxStepHeight + TargetFloorDistance);
    if (!SweepDashCapsule(Forward, Down, Hit) || !IsWalkable(Hit) || !CanStepUp(Hit)
        || !IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, Radius))
    {
        return false;
    }
    const double Rise = Hit.ImpactPoint.Z - (Start.Z - HalfHeight - StartClearance);
    if (Rise <= DashPositionTolerance || Rise > MaxStepHeight + DashPositionTolerance)
    {
        return false;
    }
    const FVector End(Forward.X, Forward.Y, Hit.ImpactPoint.Z + HalfHeight + TargetFloorDistance);
    FFindFloorResult Landing;
    if (SweepDashCapsule(Forward, End, Hit)
        || !FindDashFloor(End, Landing) || Landing.GetDistanceToFloor() > MAX_FLOOR_DIST + DashPositionTolerance)
    {
        return false;
    }
    Leg.Waypoints = {Raised, Forward, End};
    Leg.End = End;
    return true;
}

// One wall may redirect the remaining velocity. A second blocking surface ends the dash.
void UGirlSaviorCharacterMovementComponent::ResolveDashContact(FDashLeg& Leg) const
{
    const FVector Normal = Leg.Contact.ImpactNormal.GetSafeNormal2D();
    if (Leg.Contact.bStartPenetrating || IsHardDashBlocker(Leg.Contact) || Leg.bSliding || Normal.IsNearlyZero())
    {
        Leg.PathEnd = EDashPathEnd::Blocked;
        return;
    }
    Leg.NextVelocity = FVector::VectorPlaneProject(Leg.NextVelocity, Normal);
    Leg.bSliding = true;
    if (Leg.NextVelocity.IsNearlyZero(1.0))
    {
        Leg.PathEnd = EDashPathEnd::Blocked;
    }
}

// Plan a short grounded move without changing the capsule, floor cache or world overlaps.
UGirlSaviorCharacterMovementComponent::FDashLeg UGirlSaviorCharacterMovementComponent::QueryDashLeg(
    const FVector& Start, const FVector& PlanarVelocity, double TimeBudget, bool bSliding) const
{
    FDashLeg Result;
    Result.End = Start;
    Result.NextVelocity = PlanarVelocity;
    Result.bSliding = bSliding;

    FFindFloorResult StartFloor;
    if (!FindDashFloor(Start, StartFloor))
    {
        Result.PathEnd = EDashPathEnd::Unsupported;
        return Result;
    }

    const double Slice = FMath::Min(TimeBudget, MaxDashLegDistance / PlanarVelocity.Size2D());
    const FVector HorizontalDelta = PlanarVelocity * Slice;
    FVector Delta = ComputeGroundMovementDelta(HorizontalDelta, StartFloor.HitResult, StartFloor.bLineTrace);
    Delta *= HorizontalDelta.Size2D() / Delta.Size2D();
    FHitResult Hit;
    bool bBlocked = SweepDashCapsule(Start, Start + Delta, Hit);
    if (bBlocked && !IsHardDashBlocker(Hit) && IsWalkable(Hit))
    {
        // A new ramp can enter the sweep before it enters the vertical floor query.
        const double RampStart = Hit.Time;
        FVector RampDelta = ComputeGroundMovementDelta(HorizontalDelta, Hit, false);
        RampDelta *= HorizontalDelta.Size2D() / RampDelta.Size2D();
        Delta = Delta * RampStart + RampDelta * (1.0 - RampStart);
        bBlocked = SweepDashCapsule(Start, Start + Delta, Hit);
    }
    Result.Time = Slice;
    if (bBlocked && !IsHardDashBlocker(Hit) && QueryDashStep(Start, HorizontalDelta, StartFloor, Hit, Result))
    {
        return Result;
    }

    const double HitFraction = bBlocked ? DashContactFraction(Delta, Hit) : 1.0;
    double Fraction = HitFraction;
    FFindFloorResult EndFloor;
    if (!FindDashFloor(Start + Delta * Fraction, EndFloor))
    {
        // Refine the last supported point on the swept segment before an edge.
        double Unsupported = Fraction;
        Fraction = 0.0;
        EndFloor = StartFloor;
        for (int32 Attempt = 0; Attempt < EdgeRefinementSteps; ++Attempt)
        {
            const double Mid = (Fraction + Unsupported) * 0.5;
            FFindFloorResult MidFloor;
            if (FindDashFloor(Start + Delta * Mid, MidFloor))
            {
                Fraction = Mid;
                EndFloor = MidFloor;
            }
            else
            {
                Unsupported = Mid;
            }
        }
        Result.PathEnd = EDashPathEnd::Unsupported;
    }
    Result.End = Start + Delta * Fraction;
    Result.Time = Slice * Fraction;
    // Sweep through a reachable contact in live movement so Unreal delivers its blocking event.
    if (bBlocked && Result.PathEnd == EDashPathEnd::Continue)
    {
        Result.Contact = Hit;
        Result.Waypoints.Add(Start + Delta);
        ResolveDashContact(Result);
    }
    else if (Fraction > 0.0)
    {
        Result.Waypoints.Add(Result.End);
    }
    if (EndFloor.GetDistanceToFloor() > MAX_FLOOR_DIST + DashPositionTolerance)
    {
        const double Adjust = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5 - EndFloor.GetDistanceToFloor();
        Result.End.Z += Adjust;
        Result.Waypoints.Add(Result.End);
    }
    return Result;
}

double UGirlSaviorCharacterMovementComponent::ProbeDashPath(
    const FVector& InitialVelocity, float TimeBudget, double StopAtDistance) const
{
    FVector Position = UpdatedComponent->GetComponentLocation();
    FVector PlanarVelocity = InitialVelocity;
    FFindFloorResult Floor;
    if (!FindDashFloor(Position, Floor) || Floor.GetDistanceToFloor() > MAX_FLOOR_DIST + DashPositionTolerance)
    {
        return 0.0;
    }
    double Travel = 0.0;
    double Remaining = TimeBudget;
    bool bSliding = false;
    while (Remaining > UE_SMALL_NUMBER)
    {
        const FDashLeg Leg = QueryDashLeg(Position, PlanarVelocity, Remaining, bSliding);
        Travel += FVector::Dist2D(Position, Leg.End);
        if (Travel >= StopAtDistance || Leg.PathEnd != EDashPathEnd::Continue)
        {
            break;
        }
        Remaining -= Leg.Time;
        Position = Leg.End;
        PlanarVelocity = Leg.NextVelocity;
        bSliding = Leg.bSliding;
    }
    return Travel;
}

// Discard a step or leg whose real sweeps leave the pawn without valid support.
bool UGirlSaviorCharacterMovementComponent::CommitDashLeg(
    const FDashLeg& Leg, FHitResult& UnexpectedHit, FScopedMovementUpdate& Scope)
{
    for (int32 Index = 0; Index < Leg.Waypoints.Num(); ++Index)
    {
        FHitResult Hit;
        FVector Delta = Leg.Waypoints[Index] - UpdatedComponent->GetComponentLocation();
        if (Index == 1 && Leg.Contact.bBlockingHit)
        {
            // Keep the live sweep's collision pullback when following a drop beside a wall.
            Delta.X = Delta.Y = 0.0;
        }
        SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
        const bool bExpectedContact = Index == 0 && Leg.Contact.bBlockingHit
            && Hit.GetComponent() == Leg.Contact.GetComponent()
            && FVector::DotProduct(Hit.ImpactNormal, Leg.Contact.ImpactNormal) > 0.99;
        if (Hit.bBlockingHit && !bExpectedContact)
        {
            UnexpectedHit = Hit;
            break;
        }
    }

    FFindFloorResult Floor;
    if (UnexpectedHit.bStartPenetrating || !FindDashFloor(UpdatedComponent->GetComponentLocation(), Floor)
        || Floor.GetDistanceToFloor() > MAX_FLOOR_DIST + DashPositionTolerance
        || (Leg.Waypoints.Num() == 3 && UnexpectedHit.bBlockingHit))
    {
        Scope.RevertMove();
        return false;
    }
    return true;
}

void UGirlSaviorCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
    Super::PhysCustom(DeltaTime, Iterations);
    if (IsDashing())
    {
        PhysDash(DeltaTime, Iterations);
    }
}

void UGirlSaviorCharacterMovementComponent::PhysDash(float DeltaTime, int32 Iterations)
{
    double FrameRemaining = DeltaTime;
    while (FrameRemaining > UE_SMALL_NUMBER && IsDashing())
    {
        if (!HasValidData() || !HasDashControl() || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
        {
            FinishDash(TEXT("control lost"));
            break;
        }
        const FVector Start = UpdatedComponent->GetComponentLocation();
        FDashLeg Leg = QueryDashLeg(Start, DashVelocity, FMath::Min(FrameRemaining, DashTimeRemaining), bDashSliding);
        bool bCommitted = false;
        bool bStalled = false;
        {
            FScopedMovementUpdate Scope(UpdatedComponent, EScopedUpdate::DeferredUpdates);
            FHitResult UnexpectedHit;
            bCommitted = CommitDashLeg(Leg, UnexpectedHit, Scope);
            if (bCommitted)
            {
                const double Moved = FVector::Dist2D(Start, UpdatedComponent->GetComponentLocation());
                if (UnexpectedHit.bBlockingHit)
                {
                    Leg.Time = FMath::Min(Leg.Time, Moved / DashVelocity.Size2D());
                    Leg.Contact = UnexpectedHit;
                    Leg.NextVelocity = DashVelocity;
                    Leg.bSliding = bDashSliding;
                    ResolveDashContact(Leg);
                }
                DashTravel += Moved;
                DashTimeSimulated += Leg.Time;
                FrameRemaining -= Leg.Time;
                DashTimeRemaining -= Leg.Time;
                bStalled = Leg.Time <= UE_SMALL_NUMBER && Leg.bSliding == bDashSliding;
            }
        }
        // Its committed time is already spent when an overlap or hit callback ends the dash.
        if (!IsDashing())
        {
            break;
        }
        if (!bCommitted)
        {
            FinishDash(TEXT("lost support or blocked step"));
            break;
        }
        if (Leg.PathEnd != EDashPathEnd::Continue || bStalled)
        {
            FinishDash(Leg.PathEnd == EDashPathEnd::Unsupported ? TEXT("floor edge") : TEXT("blocking contact"));
            break;
        }
        if (DashTimeRemaining <= UE_SMALL_NUMBER)
        {
            FinishDash(TEXT("completed"));
            break;
        }
        DashVelocity = Leg.NextVelocity;
        bDashSliding = Leg.bSliding;
        Velocity = DashVelocity;
    }
    if (!IsDashing() && FrameRemaining >= MIN_TICK_TIME)
    {
        // Input was already evaluated this frame. A control handoff invalidates that acceleration.
        const auto* Controller = CharacterOwner
            ? Cast<AGirlSaviorPlayerController>(CharacterOwner->GetController()) : nullptr;
        if (!Controller || !Controller->CanUseGameplayInput() || Controller->IsMoveInputIgnored())
        {
            Acceleration = FVector::ZeroVector;
        }
        StartNewPhysics(static_cast<float>(FrameRemaining), Iterations + 1);
    }
}
