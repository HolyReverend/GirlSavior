#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GirlSaviorCharacter.generated.h"

class UGirlSaviorAimComponent;
class UGirlSaviorDamageReceiverComponent;
class UGirlSaviorFireComponent;
class UGirlSaviorVitalsSettings;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGirlSaviorCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride) override;

	UFUNCTION(BlueprintPure, Category = "Prototype|Aim")
	UGirlSaviorAimComponent* GetAimComponent() const { return AimComponent; }

	UFUNCTION(BlueprintPure, Category="Prototype|Fire")
	UGirlSaviorFireComponent* GetFireComponent() const { return FireComponent; }

	UFUNCTION(BlueprintPure, Category = "Prototype|Damage")
	UGirlSaviorDamageReceiverComponent* GetDamageReceiver() const { return DamageReceiver; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prototype|Combat")
	TObjectPtr<UGirlSaviorVitalsSettings> CombatVitals;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Damage", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGirlSaviorDamageReceiverComponent> DamageReceiver;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Fire", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UGirlSaviorFireComponent> FireComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Aim", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGirlSaviorAimComponent> AimComponent;
};
