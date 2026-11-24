#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/HUD.h"
#include "GirlSaviorCombatHUD.generated.h"

class AGirlSaviorCombatEncounter;
class AGirlSaviorCharacter;
class UGirlSaviorCombatOutcomeWidget;
class UGirlSaviorCombatVitalsWidget;
class UGirlSaviorDamageReceiverComponent;
enum class EGirlSaviorRunState : uint8;
struct FGirlSaviorDamageResult;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorCombatHUD : public AHUD
{
    GENERATED_BODY()

public:
    void ConfigurePresentation(AGirlSaviorCombatEncounter& InEncounter,
        TSubclassOf<UGirlSaviorCombatVitalsWidget> InVitalsClass,
        TSubclassOf<UGirlSaviorCombatOutcomeWidget> InOutcomeClass);
    void ReleasePresentation();

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    UGirlSaviorCombatVitalsWidget* GetVitalsWidget() const { return VitalsWidget; }

    UFUNCTION(BlueprintPure, Category = "Combat HUD")
    UGirlSaviorCombatOutcomeWidget* GetOutcomeWidget() const { return OutcomeWidget; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    enum class EDashReadout : uint8 { Unset, Unavailable, Ready, Protected, Dashing, Cooldown };

    void RebuildPresentation();
    void RemovePresentation();
    void UnbindEncounter();
    void FocusGameplayViewport();
    void BindHero(AGirlSaviorCharacter* Hero);
    void CacheDashDeadline();
    void RefreshDash();
    void RefreshOutcome();
    void FocusOutcome();
    void HandleBarrierChanged();
    void HandleRunStateChanged(EGirlSaviorRunState State);
    void HandleRetry();

    UFUNCTION()
    void HandleMovementModeChanged(ACharacter* Character, EMovementMode PreviousMode, uint8 PreviousCustomMode);

    UFUNCTION()
    void HandleDamageResolved(const FGirlSaviorDamageResult& Result);

    UPROPERTY(Transient)
    TSubclassOf<UGirlSaviorCombatVitalsWidget> VitalsClass;

    UPROPERTY(Transient)
    TSubclassOf<UGirlSaviorCombatOutcomeWidget> OutcomeClass;

    UPROPERTY(Transient)
    TObjectPtr<UGirlSaviorCombatVitalsWidget> VitalsWidget;

    UPROPERTY(Transient)
    TObjectPtr<UGirlSaviorCombatOutcomeWidget> OutcomeWidget;

    TWeakObjectPtr<AGirlSaviorCombatEncounter> Encounter;
    TWeakObjectPtr<AGirlSaviorCharacter> BoundHero;
    TWeakObjectPtr<UGirlSaviorDamageReceiverComponent> Receiver;
    double DashReadyTime = 0.0;
    EDashReadout DashReadout = EDashReadout::Unset;
    int32 DashCooldownTenths = 0;
};
