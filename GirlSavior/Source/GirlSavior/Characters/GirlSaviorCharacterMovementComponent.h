#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GirlSaviorCharacterMovementComponent.generated.h"

UCLASS()
class GIRLSAVIOR_API UGirlSaviorCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UGirlSaviorCharacterMovementComponent();

    virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Response", meta = (ClampMin = "0.001", UIMin = "0.001", Units = "s"))
    float MoveResponseTime = 0.10f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Response", meta = (ClampMin = "0.001", UIMin = "0.001", Units = "s"))
    float StopResponseTime = 0.08f;

protected:
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
    FVector PreviousWalkTarget = FVector::ZeroVector;
    double WalkMoveRate = 0.0;
    bool bWarnedInvalidResponse = false;
};
