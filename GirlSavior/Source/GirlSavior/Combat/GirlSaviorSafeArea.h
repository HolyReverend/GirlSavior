#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorSafeArea.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class AGirlSaviorPlayerController;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorSafeArea : public AActor
{
	GENERATED_BODY()

public:
	AGirlSaviorSafeArea();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|SafeArea")
	TObjectPtr<UBoxComponent> SafeVolume;

// Synchronize both the area and controller registries after possession inside a zone.
	void RegisterPawn(AActor* OtherActor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	TSet<TWeakObjectPtr<AGirlSaviorPlayerController>> RegisteredControllers;
};