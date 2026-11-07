#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GirlSaviorAimComponent.generated.h"

UCLASS(ClassGroup = (Prototype), meta = (BlueprintSpawnableComponent))
class GIRLSAVIOR_API UGirlSaviorAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGirlSaviorAimComponent();

	UFUNCTION(BlueprintPure, Category = "Prototype|Aim")
	FVector GetAimDirection() const { return AimDirection; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Aim")
	bool IsCursorInsideGameView() const { return bCursorInsideGameView; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Aim")
	bool IsHoldingAim() const { return bHolding; }

	// Invalidate only the transient sample; handoffs retain facing and hysteresis.
	void ClearCursorSample() { bCursorInsideGameView = false; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Aim", meta = (ClampMin = "0.0001", ClampMax = "0.4999"))
	float HoldEnterFraction = .010f;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Aim", meta = (ClampMin = "0.0001", ClampMax = "0.4999"))
	float HoldExitFraction = .015f;

private:
	void ValidateTuning();
	void ResolveViewSample(const FVector& BodyOrigin, const FVector2D& Cursor,
		const FIntRect& ViewRect, const FMatrix& ViewProjection);

	FVector AimDirection = FVector::ForwardVector;
	bool bHolding = false;
	bool bCursorInsideGameView = false;

#if WITH_EDITOR
	friend struct FGirlSaviorAimAcceptance;
#endif
};
