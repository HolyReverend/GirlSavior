#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "Components/CapsuleComponent.h"
#include "Combat/GirlSaviorDamageReceiverComponent.h"
#include "Combat/GirlSaviorFireComponent.h"

AGirlSaviorCharacter::AGirlSaviorCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGirlSaviorCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	AimComponent = CreateDefaultSubobject<UGirlSaviorAimComponent>(TEXT("AimComponent"));
	FireComponent = CreateDefaultSubobject<UGirlSaviorFireComponent>(TEXT("FireComponent"));
	DamageReceiver = CreateDefaultSubobject<UGirlSaviorDamageReceiverComponent>(TEXT("DamageReceiver"));
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
}

void AGirlSaviorCharacter::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
{
	// Cancel before Character computes an additive launch from the current velocity.
	CastChecked<UGirlSaviorCharacterMovementComponent>(GetCharacterMovement())->CancelDash();
	Super::LaunchCharacter(LaunchVelocity, bXYOverride, bZOverride);
}
