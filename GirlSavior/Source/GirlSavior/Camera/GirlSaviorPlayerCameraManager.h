#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "GirlSaviorPlayerCameraManager.generated.h"

class AGirlSaviorCameraRoomBounds;
class AGirlSaviorCameraDoorway;
class UPrimitiveComponent;
struct FSceneViewProjectionData;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	FVector ToWorldMoveIntent(const FVector2D& ScreenIntent) const;
	virtual void UpdateCamera(float DeltaTime) override;

	// Resolve destination and replace follow history at the next evaluation.
	UFUNCTION(BlueprintCallable, Category = "Prototype|Camera")
	void RequestFollowSnap();

protected:
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;
	virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera")
	FRotator ViewRotation = FRotator(-35.f, 45.f, 0.f);
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "1", Units = "cm"))
	float FollowDistance = 1600.f;
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "5", ClampMax = "170", Units = "deg"))
	float ViewFieldOfView = 45.f;
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "0.15", ClampMax = "0.25", Units = "s"))
	float FollowSettleSeconds = .20f;
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "0", ClampMax = "0.49"))
	FVector2D HeroScreenMargin = FVector2D(.20, .20);
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "0.001", Units = "s"))
	float FadeSeconds = .15f;
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float MinimumCoverage = .20f;
	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Camera", meta = (ClampMin = "0", Units = "cm"))
	float NearbyWarningRange = 1000.f;

private:
	void RefreshBoundsCache();
	FVector ResolvePreferredPivot(const FVector& BodyCenter);
	FVector ConstrainAnchor(const FVector& Anchor, const FVector& BodyCenter, const FMinimalViewInfo& POV) const;
	bool BuildCandidateProjection(const FMinimalViewInfo& POV, FSceneViewProjectionData& Data) const;
	void UpdateOcclusion(float DeltaTime, bool bImmediate);
	void RestoreOcclusion();

	TArray<TWeakObjectPtr<ULevel>> CachedLevels;
	TArray<TWeakObjectPtr<AGirlSaviorCameraRoomBounds>> Rooms;
	TArray<TWeakObjectPtr<AGirlSaviorCameraDoorway>> Doorways;
	TArray<TWeakObjectPtr<AGirlSaviorCameraRoomBounds>> ActiveRooms;
	TWeakObjectPtr<AActor> FollowTarget;
	FVector FollowAnchor = FVector::ZeroVector;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, float> Fades;
	bool bSnapRequested = true;
	bool bCutEvaluated = false;
};
