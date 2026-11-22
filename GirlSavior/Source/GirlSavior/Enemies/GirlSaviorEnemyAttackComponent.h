#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enemies/GirlSaviorEnemyDefinition.h"
#include "GirlSaviorEnemyAttackComponent.generated.h"

class AGirlSaviorCharacter;
class AGirlSaviorCombatEncounter;
class AGirlSaviorEnemyCharacter;
class AGirlSaviorTestProjectile;
class ULineBatchComponent;
enum class EGirlSaviorProjectileCompletion : uint8;

UENUM(BlueprintType)
enum class EGirlSaviorEnemyAttackPhase : uint8
{
    Approach,
    Preparation,
    Active,
    Recovery
};

UENUM(BlueprintType)
enum class EGirlSaviorAttackEndReason : uint8
{
    Completed,
    Cancelled
};

USTRUCT(BlueprintType)
struct FGirlSaviorAttackCue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    int64 AttackId = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    EGirlSaviorEnemyRole Role = EGirlSaviorEnemyRole::Pursuer;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    EGirlSaviorEnemyAttackPhase Phase = EGirlSaviorEnemyAttackPhase::Approach;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    double PhaseStart = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    double PhaseEnd = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    bool bLocked = false;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    FVector Origin = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    FVector Direction = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    TArray<FVector> FloorPolygon;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    FVector FloorStart = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    FVector FloorEnd = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    float HalfWidth = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGirlSaviorAttackCueEvent, const FGirlSaviorAttackCue&, Cue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGirlSaviorAttackEndEvent, int64, AttackId,
    EGirlSaviorAttackEndReason, Reason);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGirlSaviorNativeAttackCue, AGirlSaviorEnemyCharacter&, const FGirlSaviorAttackCue&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGirlSaviorChargeBlockedCue, const FVector&, const FVector&);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GIRLSAVIOR_API UGirlSaviorEnemyAttackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGirlSaviorEnemyAttackComponent();

    void ActivateForEncounter(AGirlSaviorCombatEncounter& Encounter, const UGirlSaviorEnemyDefinition& Definition,
        ULineBatchComponent& WarningVisual);
    void StopForEncounter();
    void CancelForTargetChange();
    bool ShouldHoldPosition(const AGirlSaviorCharacter& Hero) const;
    bool CanThreatenFrom(const FVector& Start, const AGirlSaviorCharacter& Hero) const;

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    EGirlSaviorEnemyAttackPhase GetAttackPhase() const { return Cue.Phase; }

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    FGirlSaviorAttackCue GetAttackCue() const { return Cue; }

    UPROPERTY(BlueprintAssignable, Category = "Combat|Attack")
    FGirlSaviorAttackCueEvent OnAttackCue;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Attack")
    FGirlSaviorAttackEndEvent OnAttackEnded;

    FGirlSaviorNativeAttackCue OnNativeAttackCue;
    FGirlSaviorChargeBlockedCue OnChargeBlocked;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool TryStartAttack(AGirlSaviorCharacter& Hero, double Now);
    void UpdatePreparation(AGirlSaviorCharacter& Hero, double Now);
    void CommitAttack(AGirlSaviorCharacter& Hero, double Now);
    void LaunchShooter(double Now);
    void AdvanceCharge(AGirlSaviorCharacter& Hero, double Now, float DeltaSeconds);
    void FinishCharge(const FHitResult* Hit, double Now);
    void OnProjectileCompleted(AGirlSaviorTestProjectile* Projectile, EGirlSaviorProjectileCompletion Reason);
    void EnterRecovery(double Now, bool bFinishCue);
    void CancelAttack();
    void NotifyAttackEnded(EGirlSaviorAttackEndReason Reason);
    void RestoreWalking();
    FVector DirectionTo(const AGirlSaviorCharacter& Hero) const;
    void UpdateUnlockedShape(const AGirlSaviorCharacter& Hero);
    void UpdateShape(bool bLimitToLockedRange);
    void BuildFloorPolygon(TConstArrayView<float> MaximumRanges, TArray<float>& OutRanges);
    bool CanSeeWarning() const;
    FVector FloorPoint(const FVector& BodyPoint) const;
    double MeasureStraightRange(const FVector& Start, const FVector& Direction, double MaxRange) const;
    bool HasChargeSupport(const FVector& Position) const;
    bool CanContact(const AGirlSaviorCharacter& Hero) const;
    void ShowShape(bool bSwing) const;
    void PublishCue();

    TWeakObjectPtr<AGirlSaviorEnemyCharacter> Enemy;
    TWeakObjectPtr<AGirlSaviorCombatEncounter> OwningEncounter;
    TWeakObjectPtr<ULineBatchComponent> Visual;
    FGirlSaviorPursuerTuning Pursuer;
    UPROPERTY()
    FGirlSaviorShooterTuning Shooter;
    FGirlSaviorChargerTuning Charger;
    EGirlSaviorEnemyRole Role = EGirlSaviorEnemyRole::Pursuer;
    FGirlSaviorAttackCue Cue;
    TArray<float> CurrentRanges;
    TArray<float> LockedRanges;
    double LockAt = 0.0;
    double NextHeavyAdmission = 0.0;
    double Preparation = 0.0;
    double LockedTail = 0.0;
    double Recovery = 0.0;
    double StraightRange = 0.0;
    double LockedStraightRange = 0.0;
    double ChargeRemaining = 0.0;
    double SwingClearAt = 0.0;
    bool bSuspendedWalking = false;
};
