#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GirlSaviorCharacter.generated.h"

class UGirlSaviorAimComponent;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGirlSaviorCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Prototype|Aim")
	UGirlSaviorAimComponent* GetAimComponent() const { return AimComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Aim", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGirlSaviorAimComponent> AimComponent;
};
