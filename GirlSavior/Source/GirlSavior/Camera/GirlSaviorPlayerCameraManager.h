#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "GirlSaviorPlayerCameraManager.generated.h"

class AGirlSaviorCameraRoomBounds;
class FGirlSaviorCameraRoomFraming;
class UPrimitiveComponent;
class UGirlSaviorCameraSettings;
struct FSceneViewProjectionData;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	const UGirlSaviorCameraSettings& GetCameraSettings() const;
	FVector ToWorldMoveIntent(const FVector2D& ScreenIntent) const;
	virtual void UpdateCamera(float DeltaTime) override;

	// Resolve destination and replace follow history at the next evaluation.
	UFUNCTION(BlueprintCallable, Category = "Prototype|Camera")
	void RequestFollowSnap();

protected:
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;
	virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prototype|Camera")
	TObjectPtr<UGirlSaviorCameraSettings> CameraSettings;

private:
	void RefreshBoundsCache();
	FVector ResolvePreferredPivot(const FVector& BodyCenter);
	FVector ConstrainAnchor(const FVector& Anchor, const FVector& BodyCenter, const FMinimalViewInfo& POV) const;
	bool BuildCandidateProjection(const FMinimalViewInfo& POV, FSceneViewProjectionData& Data) const;
	void UpdateOcclusion(float DeltaTime, bool bImmediate);
	void RestoreOcclusion();

	TArray<TWeakObjectPtr<ULevel>> CachedLevels;
	TArray<TWeakObjectPtr<AGirlSaviorCameraRoomBounds>> Rooms;
	TSharedPtr<FGirlSaviorCameraRoomFraming> RoomFraming;
	TArray<TArray<FVector2D>> CachedRoomPolygons;
	double CachedWallInset = -1.;
	TArray<TWeakObjectPtr<AGirlSaviorCameraRoomBounds>> ActiveRooms;
	TWeakObjectPtr<AActor> FollowTarget;
	FVector FollowAnchor = FVector::ZeroVector;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, float> Fades;
	bool bSnapRequested = true;
	bool bCutEvaluated = false;
};
