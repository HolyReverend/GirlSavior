#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GirlSaviorDamageReceiverComponent.generated.h"

UENUM(BlueprintType)
enum class EGirlSaviorDamageKind : uint8
{
	DirectProjectile
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGirlSaviorAcceptedDamage, EGirlSaviorDamageKind, Kind, float, Amount);

UCLASS(ClassGroup = (Prototype), meta = (BlueprintSpawnableComponent))
class GIRLSAVIOR_API UGirlSaviorDamageReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGirlSaviorDamageReceiverComponent();

	// Keep validation and future protection rules at the damage entry point.
	UFUNCTION(BlueprintCallable, Category = "Prototype|Damage")
	bool TryApplyDamage(EGirlSaviorDamageKind Kind, float Amount);

	UPROPERTY(BlueprintAssignable, Category = "Prototype|Damage")
	FGirlSaviorAcceptedDamage OnDamageAccepted;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Prototype|Damage")
	double AcceptedDamageTotal = 0.0;
};