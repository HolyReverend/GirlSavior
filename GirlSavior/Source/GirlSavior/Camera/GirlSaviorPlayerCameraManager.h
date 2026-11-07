#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "GirlSaviorPlayerCameraManager.generated.h"

UCLASS()
class GIRLSAVIOR_API AGirlSaviorPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	FVector ToWorldMoveIntent(const FVector2D& ScreenIntent) const;

protected:
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera")
	FRotator ViewRotation = FRotator(-35.f, 45.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "1", Units = "cm"))
	float FollowDistance = 1600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "5", ClampMax = "170", Units = "deg"))
	float ViewFieldOfView = 45.f;
};
