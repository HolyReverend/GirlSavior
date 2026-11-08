#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GirlSaviorCharacter.generated.h"

class UGirlSaviorAimComponent;
class UGirlSaviorFireComponent;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGirlSaviorCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Prototype|Aim")
	UGirlSaviorAimComponent* GetAimComponent() const { return AimComponent; }

	UFUNCTION(BlueprintPure, Category="Prototype|Fire")
	UGirlSaviorFireComponent* GetFireComponent() const { return FireComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype|Fire", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UGirlSaviorFireComponent> FireComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Aim", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGirlSaviorAimComponent> AimComponent;
};
