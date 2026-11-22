#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GirlSaviorEnemyAIController.generated.h"

class AGirlSaviorCombatEncounter;
class AGirlSaviorCharacter;
class AGirlSaviorEnemyCharacter;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    AGirlSaviorEnemyAIController();

    void ActivateForEncounter(AGirlSaviorCombatEncounter& Encounter);
    void PauseApproach();
    void StopForEncounter();

protected:
    virtual void Tick(float DeltaTime) override;

private:
    bool IsPathAdmissible(const AGirlSaviorEnemyCharacter& Body, const FNavPathSharedPtr& Path,
        int32 FirstPoint) const;
    bool FindThreatPosition(const AGirlSaviorEnemyCharacter& Body, const AGirlSaviorCharacter& Hero,
        FVector& OutGoal, bool& bOutCanThreaten) const;

    TWeakObjectPtr<AGirlSaviorCombatEncounter> OwningEncounter;
    FVector LastRequestedGoal = FVector::ZeroVector;
    FVector LastTargetPosition = FVector::ZeroVector;
    double NextQueryTime = 0.0;
    bool bFollowingPosition = false;
    bool bFollowingThreatPosition = false;
};
