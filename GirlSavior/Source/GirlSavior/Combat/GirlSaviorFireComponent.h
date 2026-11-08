#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/GirlSaviorFireSchedule.h"
#include "GirlSaviorFireComponent.generated.h"

class AGirlSaviorTestProjectile;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGirlSaviorShotCue, FVector, Position, FVector, Direction);

UCLASS(ClassGroup=(Prototype), meta=(BlueprintSpawnableComponent))
class GIRLSAVIOR_API UGirlSaviorFireComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UGirlSaviorFireComponent();
	// The controller validates each down edge; up edges must always reach the series.
	void SetFireButtonDown(bool bDown);
	UFUNCTION(BlueprintCallable, Category="Prototype|Fire")
	void CancelFireSeries();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prototype|Fire", meta=(ClampMin="0.001"))
	float ShotsPerSecond = 4.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fire")
	TSubclassOf<AGirlSaviorTestProjectile> ProjectileClass;
	UPROPERTY(BlueprintAssignable, Category="Prototype|Fire")
	FGirlSaviorShotCue OnShotCue;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void BeginPlay() override;
private:
	FGirlSaviorFireSchedule Schedule;
};
