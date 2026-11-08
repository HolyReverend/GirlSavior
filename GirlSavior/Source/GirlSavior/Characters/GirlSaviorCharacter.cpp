#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "Components/CapsuleComponent.h"
#include "Combat/GirlSaviorFireComponent.h"

AGirlSaviorCharacter::AGirlSaviorCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGirlSaviorCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	AimComponent = CreateDefaultSubobject<UGirlSaviorAimComponent>(TEXT("AimComponent"));
	FireComponent = CreateDefaultSubobject<UGirlSaviorFireComponent>(TEXT("FireComponent"));
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
}
