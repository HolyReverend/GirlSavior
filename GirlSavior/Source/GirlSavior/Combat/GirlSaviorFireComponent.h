#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/GirlSaviorFireSchedule.h"
#include "GirlSaviorFireComponent.generated.h"

class AGirlSaviorTestProjectile;
class AGirlSaviorCombatEncounter;
class UGirlSaviorCombatPistolSettings;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGirlSaviorShotCue, FVector, Position, FVector, Direction);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGirlSaviorCombatShotCue, FVector, FVector);

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Fire")
	TObjectPtr<UGirlSaviorCombatPistolSettings> CombatPistol;

	bool SetCombatEncounter(AGirlSaviorCombatEncounter* InEncounter);

	UPROPERTY(BlueprintAssignable, Category="Prototype|Fire")
	FGirlSaviorShotCue OnShotCue;
	FGirlSaviorCombatShotCue OnCombatShotCue;
	void PublishShotCue(FVector Position, FVector Direction);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void BeginPlay() override;
private:
	FGirlSaviorFireSchedule Schedule;
	TWeakObjectPtr<AGirlSaviorCombatEncounter> CombatEncounter;
};
