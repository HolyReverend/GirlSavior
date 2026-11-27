#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Enemies/GirlSaviorEnemyAttackComponent.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorCombatFeedbackPresenter.generated.h"

class AGirlSaviorCharacter;
class AGirlSaviorCombatEncounter;
class AGirlSaviorCombatHUD;
class AGirlSaviorCombatWarning;
class AGirlSaviorTestProjectile;
class APlayerController;
class UAudioComponent;
class UGirlSaviorCombatOutcomeWidget;
class UGirlSaviorCombatVitalsWidget;
class ULineBatchComponent;
class UMaterialInterface;
class UMeshComponent;
class UPointLightComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class UStaticMesh;
class UStaticMeshComponent;
enum class EGirlSaviorRunState : uint8;
struct FGirlSaviorDamageResult;
struct FGirlSaviorProjectileImpact;
struct FGirlSaviorWeaponShotCue;

UENUM(BlueprintType)
enum class EGirlSaviorFeedbackCue : uint8
{
    Muzzle, Geometry, HealthHit, BarrierSustain, BarrierBreak, EnemyDeath, HeroDefeat, Windup, Swing, ChargeStop, Ward
};

UCLASS(Blueprintable)
class GIRLSAVIOR_API AGirlSaviorCombatFeedbackPresenter : public AActor
{
    GENERATED_BODY()

public:
    AGirlSaviorCombatFeedbackPresenter();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<AGirlSaviorCombatEncounter> Encounter;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSubclassOf<AGirlSaviorCombatWarning> WarningClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSubclassOf<UGirlSaviorCombatVitalsWidget> VitalsWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TSubclassOf<UGirlSaviorCombatOutcomeWidget> OutcomeWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Light", meta = (ClampMin = "0.0"))
    float TorchIntensity = 3600.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Light", meta = (ClampMin = "1.0"))
    float TorchRadius = 950.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Light")
    FLinearColor TorchColor = FLinearColor(1.f, 0.72f, 0.43f);
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Light")
    FVector TorchOffset = FVector(0, 0, 110);

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Visual")
    TObjectPtr<UMaterialInterface> DarkBodyMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundAttenuation> CueAttenuation;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> PistolShot;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundConcurrency> ShotConcurrency;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> GeometryImpact;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> GeometryStoneImpact;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> GeometryMetalImpact;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> GeometryWoodImpact;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> HealthHit;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> BarrierSustain;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> BarrierBreak;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> EnemyDeath;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> HeroDefeat;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> PursuerReady;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> ShooterReady;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> ChargerReady;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> PursuerSwing;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> ChargeStop;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Audio")
    TObjectPtr<USoundBase> SafeWard;

    UFUNCTION(BlueprintPure, Category = "Combat|Feedback")
    int32 GetCueCount(EGirlSaviorFeedbackCue Cue) const { return CueCounts.FindRef(Cue); }

    UFUNCTION(BlueprintPure, Category = "Combat|Feedback")
    int32 GetActiveWarningCount() const;

    bool HasWarningClass() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    friend class FGirlSaviorWeaponFeedbackTest;

    struct FContactCue
    {
        TWeakObjectPtr<UObject> Identity;
        FIntVector Cell = FIntVector::ZeroValue;
        EPhysicalSurface Surface = SurfaceType_Default;
        EGirlSaviorFeedbackCue Kind = EGirlSaviorFeedbackCue::Geometry;

        bool operator==(const FContactCue& Other) const
        {
            return Identity.HasSameIndexAndSerialNumber(Other.Identity)
                && Cell == Other.Cell && Surface == Other.Surface && Kind == Other.Kind;
        }
    };
    struct FShotGroup
    {
        double ExpiresAt = 0.0;
        TArray<FContactCue, TInlineAllocator<15>> Contacts;
    };
    struct FStroke
    {
        FVector From;
        FVector To;
        FVector Drift;
        FLinearColor Color;
        float Width;
        double Start;
        double Lifetime;
        TWeakObjectPtr<AActor> Anchor;
        FVector AnchorStart;
    };
    struct FBodyPose
    {
        TWeakObjectPtr<UMeshComponent> Mesh;
        FTransform Rest;
        FTransform FallStart;
        double DeathTime = -1.0;
        double SwingUntil = 0.0;
        int64 AttackId = 0;
        EGirlSaviorEnemyRole EnemyRole = EGirlSaviorEnemyRole::Pursuer;
        EGirlSaviorEnemyAttackPhase Phase = EGirlSaviorEnemyAttackPhase::Approach;
        bool bHero = false;
        bool bRestCastsShadow = false;
    };

    void HandleHeroChanged(AGirlSaviorCharacter* Pawn);
    void HandleRunState(EGirlSaviorRunState State);
    void ConfigureHUD();
    void UnbindHero();
    void BindBody(AActor& Actor, UMeshComponent* Mesh, bool bIsHero, EGirlSaviorEnemyRole EnemyRole);
    void UnbindBody(AActor& Actor);
    void BindProjectile(AGirlSaviorTestProjectile& Projectile);
    void HandleProjectileEnd(AGirlSaviorTestProjectile* Projectile, EGirlSaviorProjectileCompletion Reason);
    void HandleDamage(AActor& Target, const FGirlSaviorDamageResult& Result);
    void HandleAttack(AGirlSaviorEnemyCharacter& Enemy, const FGirlSaviorAttackCue& Cue);
    void HandleChargeStop(const FVector& Position, const FVector& Normal);
    void HandleShot(const FGirlSaviorWeaponShotCue& Cue);
    void HandleWeaponChanged(int32 SlotIndex);
    void RegisterShotGroup(int64 ShotGroupId, float SafetyLifetime);
    void PruneShotGroups(double Now);
    bool ShouldPlayContact(int64 ShotGroupId, const FContactCue& Contact);
    void ClearWarnings();
    void DrawCosmetics(double Now);
    void UpdateBodies(double Now, float DeltaSeconds);
    void UpdateWeapon(double Now);
    void PlayCue(EGirlSaviorFeedbackCue Cue, USoundBase* Sound, const FVector& Position, bool bForeground = false);
    void AddStroke(const FVector& From, const FVector& To, const FLinearColor& Color,
        float Width, float Lifetime, const FVector& Drift = FVector::ZeroVector, AActor* Anchor = nullptr);
    void AddBurst(const FVector& Center, const FVector& Normal, const FLinearColor& Color,
        float Radius, int32 Count, float Lifetime, AActor* Anchor = nullptr, bool bClipToCover = false);
    void AddBarrier(AActor& Target, bool bBroken, bool bFatal);
    FVector ClipMuzzle(const FVector& Origin, const FVector& Desired) const;

    UFUNCTION()
    void BindEnemy(AGirlSaviorEnemyCharacter* Enemy);
    UFUNCTION()
    void HandleBodyEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    UFUNCTION()
    void HandleAttackEnd(int64 AttackId, EGirlSaviorAttackEndReason Reason);
    UFUNCTION()
    void HandleProjectileImpact(const FGirlSaviorProjectileImpact& Impact);

    UPROPERTY()
    TObjectPtr<ULineBatchComponent> Lines;
    UPROPERTY()
    TObjectPtr<UPointLightComponent> Torch;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Weapon;

    TWeakObjectPtr<AGirlSaviorCharacter> Hero;
    TWeakObjectPtr<APlayerController> ListeningController;
    TWeakObjectPtr<AGirlSaviorCombatHUD> ConfiguredHUD;
    TMap<TWeakObjectPtr<AActor>, FBodyPose> BodyPoses;
    TMap<int64, TWeakObjectPtr<AGirlSaviorCombatWarning>> Warnings;
    TSet<TWeakObjectPtr<AGirlSaviorTestProjectile>> Projectiles;
    TArray<TWeakObjectPtr<UAudioComponent>> Sounds;
    TArray<FStroke> Strokes;
    TMap<EGirlSaviorFeedbackCue, int32> CueCounts;
    TMap<int64, FShotGroup> ShotGroups;
    double LastRecoil = -1.0;
    float KickDistance = 0.f;
    float KickRecoverSeconds = 1.f;
    bool bAttached = false;
};
