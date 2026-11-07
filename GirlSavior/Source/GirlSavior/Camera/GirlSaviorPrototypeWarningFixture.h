#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "GirlSaviorPrototypeWarningFixture.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

/** Hidden means inactive; the cosmetic impact has an independent lifetime. */
UCLASS(Blueprintable)
class GIRLSAVIOR_API AGirlSaviorPrototypeWarningFixture : public AActor
{
	GENERATED_BODY()
public:
	AGirlSaviorPrototypeWarningFixture();

	UFUNCTION(BlueprintCallable, Category = "Prototype|Warning")
	void TriggerWarning();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Warning")
	TObjectPtr<USceneComponent> MarkerCenter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Warning")
	TObjectPtr<UStaticMeshComponent> Marker;

	/** Hidden template copied to an independent, collision-free impact actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Warning")
	TObjectPtr<UStaticMeshComponent> ImpactCue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Warning")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prototype|Warning", meta = (ClampMin = "0.01", Units = "s"))
	float WarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prototype|Warning", meta = (ClampMin = "0.01", Units = "s"))
	float ImpactSeconds = 0.15f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnTriggerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ShowImpact();
	void FinishImpact();

	FTimerHandle WarningTimer;
	FTimerHandle ImpactTimer;
	TWeakObjectPtr<AActor> ActiveImpact;
	bool bSequenceActive = false;
};
