#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GirlSaviorEnemyCharacter.generated.h"

class AGirlSaviorCombatEncounter;
class ATriggerBox;
class UGirlSaviorDamageReceiverComponent;
class UGirlSaviorEnemyAttackComponent;
class UGirlSaviorEnemyDefinition;
class ULineBatchComponent;
class UStaticMeshComponent;
struct FGirlSaviorDamageResult;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorEnemyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGirlSaviorEnemyCharacter();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<AGirlSaviorCombatEncounter> Encounter;

    // Placed fixtures share this volume with the hero before they can engage.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<ATriggerBox> FixtureRoomBounds;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UGirlSaviorEnemyDefinition> Definition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Visual")
    TObjectPtr<UStaticMeshComponent> BodyVisual;

    bool InitializeForEncounter(AGirlSaviorCombatEncounter& InEncounter);
    void StopForEncounter();
    bool IsInFixtureRoom(const FVector& Location) const;

    UFUNCTION(BlueprintPure, Category = "Combat")
    UGirlSaviorDamageReceiverComponent* GetDamageReceiver() const { return DamageReceiver; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    UGirlSaviorEnemyAttackComponent* GetAttackComponent() const { return Attack; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void HandleDeath(const FGirlSaviorDamageResult& Result);

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UGirlSaviorDamageReceiverComponent> DamageReceiver;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UGirlSaviorEnemyAttackComponent> Attack;

    UPROPERTY()
    TObjectPtr<ULineBatchComponent> AttackVisual;

    bool bEncounterInitialized = false;
    bool bRetired = false;
};
