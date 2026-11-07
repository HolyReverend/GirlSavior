#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "Components/CapsuleComponent.h"

AGirlSaviorCharacter::AGirlSaviorCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGirlSaviorCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	AimComponent = CreateDefaultSubobject<UGirlSaviorAimComponent>(TEXT("AimComponent"));
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
}
