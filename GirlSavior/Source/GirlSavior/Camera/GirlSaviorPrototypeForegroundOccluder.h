#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GirlSaviorPrototypeForegroundOccluder.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Presentation fades independently of the persistent gameplay obstacle. */
UCLASS(Blueprintable)
class GIRLSAVIOR_API AGirlSaviorPrototypeForegroundOccluder : public AActor
{
	GENERATED_BODY()
public:
	AGirlSaviorPrototypeForegroundOccluder();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Camera")
	TObjectPtr<UBoxComponent> GameplayBlocker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Camera")
	TObjectPtr<UStaticMeshComponent> Boundary;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prototype|Camera")
	TObjectPtr<UStaticMeshComponent> Upper;
};
