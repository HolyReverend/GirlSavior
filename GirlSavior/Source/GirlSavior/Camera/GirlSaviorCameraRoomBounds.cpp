#include "Camera/GirlSaviorCameraRoomBounds.h"
#include "Components/BoxComponent.h"

namespace GirlSaviorCameraRoomBoundsPrivate
{
	bool RoomIsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool RoomIsValidAuthoringTransform(const FTransform& Transform)
	{
		const FVector Scale = Transform.GetScale3D();
		const FQuat Rotation = Transform.GetRotation();
		return RoomIsFiniteVector(Transform.GetLocation()) && RoomIsFiniteVector(Scale)
			&& Scale.X > UE_SMALL_NUMBER && Scale.Y > UE_SMALL_NUMBER && Scale.Z > UE_SMALL_NUMBER
			&& !Rotation.ContainsNaN() && Rotation.IsNormalized()
			&& Rotation.RotateVector(FVector::UpVector).Equals(FVector::UpVector, UE_KINDA_SMALL_NUMBER);
	}
}

using namespace GirlSaviorCameraRoomBoundsPrivate;

AGirlSaviorCameraRoomBounds::AGirlSaviorCameraRoomBounds()
{
	PrimaryActorTick.bCanEverTick = false;
	MembershipBox = CreateDefaultSubobject<UBoxComponent>(TEXT("MembershipBox"));
	SetRootComponent(MembershipBox);
	MembershipBox->InitBoxExtent(FVector(1000.0, 1000.0, 300.0));
	MembershipBox->SetCollisionProfileName(TEXT("NoCollision"));
	MembershipBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MembershipBox->SetGenerateOverlapEvents(false);
	MembershipBox->SetCanEverAffectNavigation(false);
}

bool AGirlSaviorCameraRoomBounds::IsConfigurationValid() const
{
	if (!IsValid(MembershipBox) || !RoomIsValidAuthoringTransform(GetActorTransform())
		|| !RoomIsValidAuthoringTransform(MembershipBox->GetComponentTransform()))
	{
		return false;
	}
	const FVector Extent = MembershipBox->GetUnscaledBoxExtent();
	return RoomIsFiniteVector(Extent) && Extent.X > 0.0 && Extent.Y > 0.0 && Extent.Z > 0.0;
}

bool AGirlSaviorCameraRoomBounds::ContainsPoint(const FVector& WorldPoint) const
{
	if (!RoomIsFiniteVector(WorldPoint) || !IsConfigurationValid())
	{
		return false;
	}
	const FVector Local = MembershipBox->GetComponentTransform().InverseTransformPosition(WorldPoint);
	const FVector Extent = MembershipBox->GetUnscaledBoxExtent();
	return RoomIsFiniteVector(Local) && FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}
