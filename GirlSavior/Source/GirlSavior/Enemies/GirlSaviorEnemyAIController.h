#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GirlSaviorEnemyAIController.generated.h"

class AGirlSaviorCombatEncounter;

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
    bool PathCrossesSafeArea(const FNavPathSharedPtr& Path) const;

    TWeakObjectPtr<AGirlSaviorCombatEncounter> OwningEncounter;
    FVector LastRequestedGoal = FVector::ZeroVector;
    double NextQueryTime = 0.0;
};
