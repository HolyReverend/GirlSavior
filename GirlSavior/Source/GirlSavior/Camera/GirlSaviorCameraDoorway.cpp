#include "Camera/GirlSaviorCameraDoorway.h"
#include "Camera/GirlSaviorCameraRoomBounds.h"
#include "Components/BoxComponent.h"

namespace GirlSaviorCameraDoorwayPrivate
{
	bool DoorwayIsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool DoorwayIsValidAuthoringTransform(const FTransform& Transform)
	{
		const FVector Scale = Transform.GetScale3D();
		const FQuat Rotation = Transform.GetRotation();
		return DoorwayIsFiniteVector(Transform.GetLocation()) && DoorwayIsFiniteVector(Scale)
			&& Scale.X > UE_SMALL_NUMBER && Scale.Y > UE_SMALL_NUMBER && Scale.Z > UE_SMALL_NUMBER
			&& !Rotation.ContainsNaN() && Rotation.IsNormalized()
			&& Rotation.RotateVector(FVector::UpVector).Equals(FVector::UpVector, UE_KINDA_SMALL_NUMBER);
	}
}

using namespace GirlSaviorCameraDoorwayPrivate;

AGirlSaviorCameraDoorway::AGirlSaviorCameraDoorway()
{
	PrimaryActorTick.bCanEverTick = false;
	TransitionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TransitionBox"));
	SetRootComponent(TransitionBox);
	TransitionBox->InitBoxExtent(FVector(200.0, 200.0, 300.0));
	TransitionBox->SetCollisionProfileName(TEXT("NoCollision"));
	TransitionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TransitionBox->SetGenerateOverlapEvents(false);
	TransitionBox->SetCanEverAffectNavigation(false);
}

bool AGirlSaviorCameraDoorway::IsConfigurationValid() const
{
	if (!IsValid(TransitionBox) || !IsValid(RoomA) || !IsValid(RoomB) || RoomA == RoomB
		|| !RoomA->IsConfigurationValid() || !RoomB->IsConfigurationValid()
		|| !DoorwayIsValidAuthoringTransform(GetActorTransform())
		|| !DoorwayIsValidAuthoringTransform(TransitionBox->GetComponentTransform()))
	{
		return false;
	}
	const FVector Extent = TransitionBox->GetUnscaledBoxExtent();
	return DoorwayIsFiniteVector(Extent) && Extent.X > 0.0 && Extent.Y > 0.0 && Extent.Z > 0.0;
}

bool AGirlSaviorCameraDoorway::ContainsPoint(const FVector& WorldPoint) const
{
	if (!DoorwayIsFiniteVector(WorldPoint) || !IsConfigurationValid())
	{
		return false;
	}
	const FVector Local = TransitionBox->GetComponentTransform().InverseTransformPosition(WorldPoint);
	const FVector Extent = TransitionBox->GetUnscaledBoxExtent();
	return DoorwayIsFiniteVector(Local) && FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

FVector AGirlSaviorCameraDoorway::GetPreferredPivot(const FVector& WorldPoint) const
{
	if (!DoorwayIsFiniteVector(WorldPoint) || !IsConfigurationValid())
	{
		return WorldPoint;
	}
	const double LocalX = TransitionBox->GetComponentTransform().InverseTransformPosition(WorldPoint).X;
	const double HalfLength = TransitionBox->GetUnscaledBoxExtent().X;
	const double T = FMath::Clamp((LocalX + HalfLength) / (2.0 * HalfLength), 0.0, 1.0);
	// Smoothstep gives zero blend slope at both room boundaries in either direction.
	const double Alpha = T * T * (3.0 - 2.0 * T);
	FVector Pivot = FMath::Lerp(RoomA->GetPreferredPivot(WorldPoint), RoomB->GetPreferredPivot(WorldPoint), Alpha);
	Pivot.Z = WorldPoint.Z;
	return DoorwayIsFiniteVector(Pivot) ? Pivot : WorldPoint;
}