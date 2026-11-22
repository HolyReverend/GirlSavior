#pragma once

#include "CoreMinimal.h"
#include "Combat/GirlSaviorDamageTypes.h"
#include "Encounters/GirlSaviorWavePolicy.h"
#include "GameFramework/Actor.h"
#include "SceneView.h"
#include "GirlSaviorCombatEncounter.generated.h"

class AGirlSaviorCharacter;
class AGirlSaviorCombatTarget;
class AGirlSaviorCombatDoor;
class AGirlSaviorCombatFeedbackPresenter;
class AGirlSaviorCombatRoom;
class AGirlSaviorEnemyCharacter;
class AGirlSaviorSafeArea;
class AGirlSaviorTestProjectile;
class UBoxComponent;
class UGirlSaviorDamageReceiverComponent;
struct FGirlSaviorWaveEntrance;

UENUM(BlueprintType)
enum class EGirlSaviorRunState : uint8
{
    Waiting,
    Active,
    Succeeded,
    Defeated
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGirlSaviorCombatantEvent, AGirlSaviorEnemyCharacter*, Enemy);
DECLARE_MULTICAST_DELEGATE_OneParam(FGirlSaviorEncounterHeroChanged, AGirlSaviorCharacter*);
DECLARE_MULTICAST_DELEGATE_OneParam(FGirlSaviorEncounterStateChanged, EGirlSaviorRunState);
DECLARE_MULTICAST_DELEGATE_OneParam(FGirlSaviorCombatProjectileRegistered, AGirlSaviorTestProjectile&);

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

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat|Waves")
    bool bWavesEnabled = false;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat|Waves")
    TObjectPtr<UGirlSaviorWaveSettings> WaveSettings;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat|Waves")
    TArray<TObjectPtr<AGirlSaviorCombatRoom>> Rooms;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat|Waves")
    TMap<EGirlSaviorEnemyRole, TSubclassOf<AGirlSaviorEnemyCharacter>> EnemyClasses;

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FGirlSaviorCombatantEvent OnEnemyRegistered;

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FGirlSaviorCombatantEvent OnEnemyRemoved;

    FGirlSaviorEncounterHeroChanged OnHeroChanged;
    FGirlSaviorEncounterStateChanged OnRunStateChanged;
    FGirlSaviorCombatProjectileRegistered OnProjectileRegistered;

    UFUNCTION(BlueprintPure, Category = "Combat")
    EGirlSaviorRunState GetRunState() const { return RunState; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    AGirlSaviorCharacter* GetHero() const { return Hero.Get(); }

    UFUNCTION(BlueprintPure, Category = "Combat")
    int32 GetLiveEnemyCount() const { return LiveEnemies.Num(); }

    UFUNCTION(BlueprintPure, Category = "Combat|Waves")
    int32 GetReservedEnemyCount() const { return Reservation.IsSet() ? Reservation->Members.Num() : 0; }

    UFUNCTION(BlueprintPure, Category = "Combat|Waves")
    int32 GetOccupiedRoleCount(EGirlSaviorEnemyRole EnemyRole) const;

    UFUNCTION(BlueprintPure, Category = "Combat|Waves")
    AGirlSaviorCombatRoom* GetCurrentRoom() const { return CurrentRoom.Get(); }

    UFUNCTION(BlueprintPure, Category = "Combat|Waves")
    double GetLastCommittedGroupTime() const { return LastCommittedGroupTime.Get(-1.0); }

    UFUNCTION(BlueprintPure, Category = "Combat")
    int32 GetLiveProjectileCount() const { return LiveProjectiles.Num(); }

    UFUNCTION(BlueprintPure, Category = "Combat")
    const FGirlSaviorDamageResult& GetDefeatResult() const { return DefeatResult; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsOutcomeVisible() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool Retry();

    bool CanEnemyEngage(const AActor& Source) const;
    bool AttachFeedback(AGirlSaviorCombatFeedbackPresenter& Presenter);
    void DetachFeedback(const AGirlSaviorCombatFeedbackPresenter& Presenter);
    bool HasFeedbackPresenter() const { return Feedback.IsValid(); }
    bool CanPresentCombatWarning() const;
    bool IsPresentationBoundsOffscreen(const FBox& Bounds) const;
    TArray<AGirlSaviorEnemyCharacter*> GetLivingEnemies() const;
    bool IsPointInSafeArea(const FVector& Point) const;
    bool CrossesSafeArea(const FVector& Start, const FVector& End, float Radius = 0.f) const;
    bool CanTraverseCombatSegment(const FVector& Start, const FVector& End, float Radius) const;
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
    struct FEnemyRecord
    {
        EGirlSaviorEnemyRole Role = EGirlSaviorEnemyRole::Pursuer;
        TWeakObjectPtr<AGirlSaviorCombatRoom> Room;
        int32 ConsecutivePathFailures = 0;
        bool bFixture = false;
        bool bRetain = false;
        bool bPublished = false;
        bool bReportedVisible = false;
        int64 GroupId = 0;
    };

    struct FWaveMember
    {
        EGirlSaviorEnemyRole Role;
        int32 SlotIndex;
        FTransform Transform;
    };

    struct FWaveReservation
    {
        int64 Id = 0;
        TWeakObjectPtr<AGirlSaviorCombatRoom> Room;
        TWeakObjectPtr<UGirlSaviorWaveProfile> Profile;
        FName EntranceId;
        TArray<FWaveMember> Members;
        double CreatedAt = 0.0;
        double DueAt = 0.0;
        bool bIntroduction = false;
    };

    bool InitializeRun(AGirlSaviorCharacter& Pawn);
    bool BindHero(AGirlSaviorCharacter& Pawn);
    void UnbindHero();
    void BindAdmission(UGirlSaviorDamageReceiverComponent& Receiver);
    EGirlSaviorDamageOutcome CheckDamageBoundary(const AActor& Target, const FGirlSaviorDamageSpec& Spec) const;
    void CaptureView();
    bool ProjectIntoView(const FVector& Point, FVector2D& Pixel, bool bRequireClearRay) const;
    void OnHeroDied(const FGirlSaviorDamageResult& Result);
    void CloseRun(EGirlSaviorRunState TerminalState);
    bool IsHeroProtected() const;
    bool RegisterEnemy(AGirlSaviorEnemyCharacter& Enemy, AGirlSaviorCombatRoom* Room, bool bFixture, int64 GroupId);
    void ActivateRegisteredEnemy(AGirlSaviorEnemyCharacter& Enemy);
    bool ValidateWaveRoute();
    AGirlSaviorCombatRoom* FindRoomAt(const FVector& Location) const;
    void RefreshHeroRoom();
    void UpdateWaves(double Now);
    void UpdateEnemyRoomsAndRetire();
    void RecordVisibleArrivals(double Now);
    void TryReserveGroup(double Now, AGirlSaviorCombatRoom& Room, const UGirlSaviorWaveProfile& Profile);
    void RevalidateAndCommit(double Now);
    void CancelReservation(const TCHAR* Reason);
    void LogGroup(const FWaveReservation& Group, const TCHAR* Action, const TCHAR* Reason, int32 Actual = 0) const;
    bool CanAccelerateWave() const;
    bool WaveTimingAllows(const UGirlSaviorWaveProfile& Profile, double Now) const;
    FGirlSaviorWavePopulation GetLivePopulation() const;
    const AGirlSaviorEnemyCharacter* GetEnemyDefaults(EGirlSaviorEnemyRole Role) const;
    bool CheckSpawnSlot(const AGirlSaviorCombatRoom& Room, const FGirlSaviorWaveEntrance& Entrance,
        const FWaveMember& Member, double& OutPathLength, const TCHAR** OutFailure = nullptr) const;
    bool IsRoomReachable(const AGirlSaviorCombatRoom* From, const AGirlSaviorCombatRoom* To) const;

    TWeakObjectPtr<AGirlSaviorCharacter> Hero;
    TWeakObjectPtr<AGirlSaviorCombatFeedbackPresenter> Feedback;
    TMap<TWeakObjectPtr<AGirlSaviorEnemyCharacter>, FEnemyRecord> LiveEnemies;
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
    TWeakObjectPtr<AGirlSaviorCombatRoom> CurrentRoom;
    TArray<TWeakObjectPtr<const AGirlSaviorCombatDoor>> RouteDoors;
    TSet<FName> ServedIntroductions;
    TOptional<FVector> LastOutsideApproach;
    TOptional<FWaveReservation> Reservation;
    TOptional<double> LastCommittedGroupTime;
    double NextWaveQuery = 0.0;
    int64 LastGroupId = 0;
    bool bInitialized = false;
    bool bInitializationAttempted = false;
    bool bViewValid = false;
    bool bCleanupPending = false;
    bool bRetryRequested = false;
};
