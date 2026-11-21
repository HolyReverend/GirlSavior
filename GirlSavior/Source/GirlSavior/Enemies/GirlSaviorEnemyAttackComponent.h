#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enemies/GirlSaviorEnemyDefinition.h"
#include "GirlSaviorEnemyAttackComponent.generated.h"

class AGirlSaviorCharacter;
class AGirlSaviorCombatEncounter;
class AGirlSaviorEnemyCharacter;
class ULineBatchComponent;

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
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGirlSaviorAttackCueEvent, const FGirlSaviorAttackCue&, Cue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGirlSaviorAttackEndEvent, int64, AttackId,
    EGirlSaviorAttackEndReason, Reason);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GIRLSAVIOR_API UGirlSaviorEnemyAttackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGirlSaviorEnemyAttackComponent();

    void ActivateForEncounter(AGirlSaviorCombatEncounter& Encounter, const FGirlSaviorPursuerTuning& Tuning,
        ULineBatchComponent& WarningVisual);
    void StopForEncounter();

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    EGirlSaviorEnemyAttackPhase GetAttackPhase() const { return Cue.Phase; }

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    FGirlSaviorAttackCue GetAttackCue() const { return Cue; }

    UPROPERTY(BlueprintAssignable, Category = "Combat|Attack")
    FGirlSaviorAttackCueEvent OnAttackCue;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Attack")
    FGirlSaviorAttackEndEvent OnAttackEnded;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool TryStartAttack(AGirlSaviorCharacter& Hero, double Now);
    void UpdatePreparation(AGirlSaviorCharacter& Hero, double Now);
    void CommitAttack(AGirlSaviorCharacter& Hero, double Now);
    void CancelAttack();
    void NotifyAttackEnded(EGirlSaviorAttackEndReason Reason);
    FVector DirectionTo(const AGirlSaviorCharacter& Hero) const;
    void UpdateUnlockedShape(const AGirlSaviorCharacter& Hero);
    void BuildFloorPolygon(TConstArrayView<float> MaximumRanges, TArray<float>& OutRanges);
    bool CanContact(const AGirlSaviorCharacter& Hero) const;
    void ShowShape(bool bSwing) const;
    void PublishCue();

    TWeakObjectPtr<AGirlSaviorEnemyCharacter> Enemy;
    TWeakObjectPtr<AGirlSaviorCombatEncounter> OwningEncounter;
    TWeakObjectPtr<ULineBatchComponent> Visual;
    FGirlSaviorPursuerTuning Pursuer;
    FGirlSaviorAttackCue Cue;
    TArray<float> CurrentRanges;
    TArray<float> LockedRanges;
    double LockAt = 0.0;
    double SwingClearAt = 0.0;
};
