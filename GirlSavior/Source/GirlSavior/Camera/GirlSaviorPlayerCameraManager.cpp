#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Math/RotationMatrix.h"

FVector AGirlSaviorPlayerCameraManager::ToWorldMoveIntent(const FVector2D& ScreenIntent) const
{
	// Input uses the authored heading even before the first camera update.
	const FRotationMatrix Basis(FRotator(0.f, ViewRotation.Yaw, 0.f));
	return Basis.GetUnitAxis(EAxis::Y) * ScreenIntent.X
		+ Basis.GetUnitAxis(EAxis::X) * ScreenIntent.Y;
}

void AGirlSaviorPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	const AGirlSaviorCharacter* Character = Cast<AGirlSaviorCharacter>(OutVT.Target);
	if (!Character)
	{
		Super::UpdateViewTargetInternal(OutVT, DeltaTime);
		return;
	}

	OutVT.POV.Location = Character->GetCapsuleComponent()->GetComponentLocation()
		- ViewRotation.Vector() * FollowDistance;
	OutVT.POV.Rotation = ViewRotation;
	OutVT.POV.FOV = ViewFieldOfView;
	OutVT.POV.ProjectionMode = ECameraProjectionMode::Perspective;
}
