#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GirlSaviorCharacterMovementComponent.generated.h"

class UGirlSaviorDashSettings;
class FScopedMovementUpdate;

UCLASS()
class GIRLSAVIOR_API UGirlSaviorCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UGirlSaviorCharacterMovementComponent();

    virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
    virtual void Launch(const FVector& LaunchVelocity) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Dash")
    TObjectPtr<UGirlSaviorDashSettings> DashSettings;

    // These are live queries for input, presentation and later damage resolution.
    UFUNCTION(BlueprintPure, Category = "Prototype|Dash")
    bool IsDashing() const;

    UFUNCTION(BlueprintPure, Category = "Prototype|Dash")
    bool IsDashProtected() const;

    UFUNCTION(BlueprintPure, Category = "Prototype|Dash")
    double GetDashCooldownRemaining() const;

    bool TryStartDash(const FVector& WorldDirection);
    void CancelDash();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Response", meta = (ClampMin = "0.001", UIMin = "0.001", Units = "s"))
    float MoveResponseTime = 0.10f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Response", meta = (ClampMin = "0.001", UIMin = "0.001", Units = "s"))
    float StopResponseTime = 0.08f;

protected:
    virtual void BeginPlay() override;
    virtual void PerformMovement(float DeltaTime) override;
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
    virtual void ApplyAccumulatedForces(float DeltaSeconds) override;
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
    static constexpr uint8 DashMode = 1;

    enum class EDashPathEnd : uint8
    {
        Continue, Blocked, Unsupported
    };

    struct FDashLeg
    {
        TArray<FVector, TInlineAllocator<3>> Waypoints;
        FVector End = FVector::ZeroVector;
        FVector NextVelocity = FVector::ZeroVector;
        double Time = 0.0;
        bool bSliding = false;
        EDashPathEnd PathEnd = EDashPathEnd::Continue;
        FHitResult Contact;
    };

    friend struct FGirlSaviorDashTestAccess;

    bool HasDashControl() const;
    void BeginDash(const FVector& InitialVelocity);
    void FinishDash(const TCHAR* Reason);
    void ClearDashState(const TCHAR* Reason);
    void PhysDash(float DeltaTime, int32 Iterations);
    double ProbeDashPath(const FVector& InitialVelocity, float TimeBudget, double StopAtDistance) const;
    FDashLeg QueryDashLeg(const FVector& Start, const FVector& PlanarVelocity, double TimeBudget, bool bSliding) const;
    bool SweepDashCapsule(const FVector& Start, const FVector& End, FHitResult& Hit) const;
    bool FindDashFloor(const FVector& Location, FFindFloorResult& Floor) const;
    bool QueryDashStep(const FVector& Start, const FVector& HorizontalDelta, const FFindFloorResult& StartFloor,
        const FHitResult& Contact, FDashLeg& Leg) const;
    bool CommitDashLeg(const FDashLeg& Leg, FHitResult& UnexpectedHit, FScopedMovementUpdate& Scope);
    void ResolveDashContact(FDashLeg& Leg) const;

    FVector PreviousWalkTarget = FVector::ZeroVector;
    double WalkMoveRate = 0.0;
    bool bWarnedInvalidResponse = false;
    FVector DashVelocity = FVector::ZeroVector;
    double DashTimeRemaining = 0.0;
    double DashTimeSimulated = 0.0;
    double DashTravel = 0.0;
    double DashProtectionEndTime = 0.0;
    double NextDashReadyTime = 0.0;
    bool bDashSliding = false;
};
