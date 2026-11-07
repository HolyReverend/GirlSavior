#include "Characters/GirlSaviorCharacterMovementComponent.h"

#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorMovement, Log, All);

UGirlSaviorCharacterMovementComponent::UGirlSaviorCharacterMovementComponent()
{
    MaxWalkSpeed = 500.0f;
}

void UGirlSaviorCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
    // Keep unsupported paths and engine early-out guards in the stock implementation.
    if (!HasValidData() || !FMath::IsFinite(DeltaTime) || DeltaTime < MIN_TICK_TIME
        || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity()
        || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion)
        || !CharacterOwner->IsLocallyControlled() || !CharacterOwner->IsPlayerControlled()
        || MovementMode != MOVE_Walking || CharacterMovementCVars::AsyncCharacterMovement != 0)
    {
        Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
        return;
    }

    const float WalkSpeed = GetMaxSpeed();
    const float AccelerationScale = GetMaxAcceleration();
    if (!FMath::IsFinite(WalkSpeed) || WalkSpeed <= 0.0f
        || !FMath::IsFinite(AccelerationScale) || AccelerationScale <= 0.0f
        || !FMath::IsFinite(MoveResponseTime) || MoveResponseTime <= 0.0f
        || !FMath::IsFinite(StopResponseTime) || StopResponseTime <= 0.0f
        || Acceleration.ContainsNaN() || Velocity.ContainsNaN())
    {
        if (!bWarnedInvalidResponse)
        {
            UE_LOG(LogGirlSaviorMovement, Warning,
                TEXT("%s: invalid walking speed, acceleration, response time or velocity; using stock movement."),
                *GetNameSafe(CharacterOwner));
            bWarnedInvalidResponse = true;
        }
        PreviousWalkTarget = FVector::ZeroVector;
        WalkMoveRate = 0.0;
        Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
        return;
    }
    bWarnedInvalidResponse = false;

    const FVector PlanarAcceleration(Acceleration.X, Acceleration.Y, 0.0);
    const FVector Target = (PlanarAcceleration / AccelerationScale).GetClampedToMaxSize(1.0) * WalkSpeed;
    // Stock walking may replace Velocity after a sweep; start every substep from that result.
    const FVector Current(Velocity.X, Velocity.Y, 0.0);
    const FVector Difference = Target - Current;
    const bool bHasTarget = !Target.IsNearlyZero();
    if (bHasTarget && (!Target.Equals(PreviousWalkTarget, 0.01) || WalkMoveRate <= 0.0))
    {
        WalkMoveRate = FMath::Max(static_cast<double>(WalkSpeed), Difference.Size()) / MoveResponseTime;
    }

    const double Rate = bHasTarget ? WalkMoveRate : static_cast<double>(WalkSpeed) / StopResponseTime;
    const double Step = Rate * DeltaTime;
    Velocity = Difference.SizeSquared() <= FMath::Square(Step)
        ? Target : Current + Difference.GetClampedToMaxSize(Step);
    Velocity.Z = 0.0;
    PreviousWalkTarget = Target;
}

void UGirlSaviorCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    if (PreviousMovementMode != MovementMode
        && (PreviousMovementMode == MOVE_Walking || MovementMode == MOVE_Walking))
    {
        PreviousWalkTarget = FVector::ZeroVector;
        WalkMoveRate = 0.0;
    }
    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}
