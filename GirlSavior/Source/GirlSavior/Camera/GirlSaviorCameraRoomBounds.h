#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorCameraRoomBounds.generated.h"

class UBoxComponent;

/** Геометрические границы комнаты для общего пространства камеры. */
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Camera")
	TObjectPtr<UBoxComponent> MembershipBox;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Prototype|Camera")
	TArray<TObjectPtr<AActor>> WarningActors;
};