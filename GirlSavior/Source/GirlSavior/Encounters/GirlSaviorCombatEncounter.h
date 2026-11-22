#pragma once

#include "CoreMinimal.h"
#include "Combat/GirlSaviorDamageTypes.h"
#include "GameFramework/Actor.h"
#include "SceneView.h"
#include "GirlSaviorCombatEncounter.generated.h"

class AGirlSaviorCharacter;
class AGirlSaviorCombatTarget;
class AGirlSaviorEnemyCharacter;
class AGirlSaviorSafeArea;
class AGirlSaviorTestProjectile;
class UBoxComponent;
class UGirlSaviorDamageReceiverComponent;

UENUM(BlueprintType)
enum class EGirlSaviorRunState : uint8
{
    Waiting,
    Active,
    Succeeded,
    Defeated
};

UCLASS()
class GIRLSAVIOR_API AGirlSaviorCombatEncounter : public AActor
{
    GENERATED_BODY()

public:
    AGirlSaviorCombatEncounter();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UBoxComponent> FirstRoomVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UBoxComponent> ExitVolume;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<AGirlSaviorSafeArea> SafeArea;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat")
    TArray<TObjectPtr<AGirlSaviorEnemyCharacter>> PlacedEnemies;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat")
    TArray<TObjectPtr<AGirlSaviorCombatTarget>> PlacedTargets;

    UFUNCTION(BlueprintPure, Category = "Combat")
    EGirlSaviorRunState GetRunState() const { return RunState; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    AGirlSaviorCharacter* GetHero() const { return Hero.Get(); }

    UFUNCTION(BlueprintPure, Category = "Combat")
    int32 GetLiveEnemyCount() const { return LiveEnemies.Num(); }

    UFUNCTION(BlueprintPure, Category = "Combat")
    int32 GetLiveProjectileCount() const { return LiveProjectiles.Num(); }

    UFUNCTION(BlueprintPure, Category = "Combat")
    const FGirlSaviorDamageResult& GetDefeatResult() const { return DefeatResult; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsOutcomeVisible() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool Retry();

    bool CanEnemyEngage(const AActor& Source) const;
    bool IsPointInSafeArea(const FVector& Point) const;
    bool CrossesSafeArea(const FVector& Start, const FVector& End, float Radius = 0.f) const;
    bool IsWarningVisible(TConstArrayView<FVector> FloorPolygon) const;
    bool IsBodyInView(const FVector& Center, float Radius, float HalfHeight) const;
    bool IsStraightWarningVisible(const FVector& Start, const FVector& End, float StripeHalfWidth) const;
    int64 AllocateAttackId();
    bool HasDangerousAttack() const { return DangerousAttackId != 0; }
    bool TryAcquireDangerousAttack(int64 AttackId, UObject& Holder);
    bool TransferDangerousAttack(int64 AttackId, const UObject& Holder, UObject& NewHolder);
    void ReleaseDangerousAttack(int64 AttackId, const UObject& Holder);
    void UnregisterEnemy(AGirlSaviorEnemyCharacter* Enemy);
    bool RegisterProjectile(AGirlSaviorTestProjectile* Projectile);
    void UnregisterProjectile(AGirlSaviorTestProjectile* Projectile);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool InitializeRun(AGirlSaviorCharacter& Pawn);
    bool BindHero(AGirlSaviorCharacter& Pawn);
    void UnbindHero();
    void BindAdmission(UGirlSaviorDamageReceiverComponent& Receiver);
    EGirlSaviorDamageOutcome CheckDamageBoundary(const AActor& Target, const FGirlSaviorDamageSpec& Spec) const;
    void CaptureView();
    bool ProjectIntoView(const FVector& Point, FVector2D& Pixel, bool bRequireClearRay) const;
    void OnHeroDied(const FGirlSaviorDamageResult& Result);
    void CloseRun(EGirlSaviorRunState TerminalState);

    TWeakObjectPtr<AGirlSaviorCharacter> Hero;
    TSet<TWeakObjectPtr<AGirlSaviorEnemyCharacter>> LiveEnemies;
    TSet<TWeakObjectPtr<AGirlSaviorTestProjectile>> LiveProjectiles;
    TArray<TWeakObjectPtr<AGirlSaviorEnemyCharacter>> PendingRetirements;
    FSceneViewProjectionData View;
    FVector ViewOrigin = FVector::ZeroVector;
    FGirlSaviorDamageResult DefeatResult;
    EGirlSaviorRunState RunState = EGirlSaviorRunState::Waiting;
    double OutcomeVisibleAt = 0.0;
    int64 LastAttackId = 0;
    int64 DangerousAttackId = 0;
    TWeakObjectPtr<UObject> DangerousAttackHolder;
    bool bInitialized = false;
    bool bInitializationAttempted = false;
    bool bViewValid = false;
    bool bCleanupPending = false;
    bool bRetryRequested = false;
};
