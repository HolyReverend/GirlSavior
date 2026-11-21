#pragma once

#include "CoreMinimal.h"
#include "Combat/GirlSaviorDamageTypes.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorCombatTarget.generated.h"

class UGirlSaviorDamageReceiverComponent;
class UGirlSaviorVitalsSettings;
class UMaterialInterface;
class UStaticMeshComponent;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorCombatTarget : public AActor
{
    GENERATED_BODY()

public:
    AGirlSaviorCombatTarget();
    UGirlSaviorDamageReceiverComponent* GetDamageReceiver() const { return DamageReceiver; }
    bool InitializeForEncounter();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(EditInstanceOnly, Category = "Combat")
    TObjectPtr<UGirlSaviorVitalsSettings> Vitals;

    UPROPERTY(EditInstanceOnly, Category = "Combat")
    TObjectPtr<UMaterialInterface> HitMaterial;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void OnHit(const FGirlSaviorDamageResult& Result);
    void OnDied(const FGirlSaviorDamageResult& Result);
    void RestoreMaterial();

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UGirlSaviorDamageReceiverComponent> DamageReceiver;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> RestingMaterial;

    FTimerHandle HitFlashTimer;
};
