#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorCameraRoomBounds.generated.h"

class UBoxComponent;

/** Query-free room membership and preferred horizontal camera anchor. */
UCLASS(BlueprintType, Blueprintable)
class GIRLSAVIOR_API AGirlSaviorCameraRoomBounds : public AActor
{
	GENERATED_BODY()

public:
	AGirlSaviorCameraRoomBounds();

	UFUNCTION(BlueprintPure, Category = "Prototype|Camera")
	bool IsConfigurationValid() const;

	UFUNCTION(BlueprintPure, Category = "Prototype|Camera")
	bool ContainsPoint(const FVector& WorldPoint) const;

	UFUNCTION(BlueprintPure, Category = "Prototype|Camera")
	FVector GetPreferredPivot(const FVector& WorldPoint) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Camera")
	TObjectPtr<UBoxComponent> MembershipBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prototype|Camera", meta = (Units = "cm"))
	FVector2D PivotCenter = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prototype|Camera", meta = (ClampMin = "0.01", Units = "cm"))
	FVector2D PivotHalfExtent = FVector2D(400.0, 400.0);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Prototype|Camera")
	TArray<TObjectPtr<AActor>> WarningActors;
};