#pragma once

#include "CoreMinimal.h"
#include "Enemies/GirlSaviorEnemyAttackComponent.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorCombatWarning.generated.h"

class ULineBatchComponent;

// The warning renders the native snapshot in world depth and has no gameplay collision.
UCLASS(Blueprintable)
class GIRLSAVIOR_API AGirlSaviorCombatWarning : public AActor
{
    GENERATED_BODY()

public:
    AGirlSaviorCombatWarning();
    void ApplyCue(const FGirlSaviorAttackCue& Cue);
    void PlayFinalSwing();
    bool IsPursuerSwing() const
    {
        return Snapshot.Role == EGirlSaviorEnemyRole::Pursuer && Snapshot.Phase == EGirlSaviorEnemyAttackPhase::Active;
    }
    virtual void Tick(float DeltaSeconds) override;

private:
    void DrawWarning(float Intensity);

    UPROPERTY()
    TObjectPtr<ULineBatchComponent> Lines;

    FGirlSaviorAttackCue Snapshot;
    bool bFinalSwing = false;
};
