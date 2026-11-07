#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorCameraDoorway.generated.h"

class UBoxComponent;
class AGirlSaviorCameraRoomBounds;

/** Local +X runs from RoomA to RoomB; position controls reversible framing. */
UCLASS(BlueprintType, Blueprintable)
class GIRLSAVIOR_API AGirlSaviorCameraDoorway : public AActor
{
	GENERATED_BODY()

public:
	AGirlSaviorCameraDoorway();

	UFUNCTION(BlueprintPure, Category = "Prototype|Camera")
	bool IsConfigurationValid() const;

	UFUNCTION(BlueprintPure, Category = "Prototype|Camera")
	bool ContainsPoint(const FVector& WorldPoint) const;

	UFUNCTION(BlueprintPure, Category = "Prototype|Camera")
	FVector GetPreferredPivot(const FVector& WorldPoint) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Camera")
	TObjectPtr<UBoxComponent> TransitionBox;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Prototype|Camera")
	TObjectPtr<AGirlSaviorCameraRoomBounds> RoomA;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Prototype|Camera")
	TObjectPtr<AGirlSaviorCameraRoomBounds> RoomB;
};