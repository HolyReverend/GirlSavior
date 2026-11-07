#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GirlSaviorPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGirlSaviorPlayerController();

	UFUNCTION(BlueprintPure, Category = "Prototype|Input")
	FVector2D GetScreenMoveIntent() const { return ScreenMoveIntent; }

	virtual void FlushPressedKeys() override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

private:
	void RegisterGameplayInput();
	void RemoveGameplayInput();
	void ClearMovementIntent();

	FVector2D ScreenMoveIntent = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RegisteredMappingContext;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem;
};
