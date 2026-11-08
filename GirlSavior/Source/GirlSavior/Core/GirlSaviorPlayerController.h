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

	UFUNCTION(BlueprintPure, Category = "Prototype|Input")
	bool CanUseGameplayInput() const;

	UFUNCTION(BlueprintCallable, Category = "Prototype|Input")
	void SetGameplayInputAvailable(bool bAvailable);

	UFUNCTION(BlueprintPure, Category = "Prototype|Input")
	bool GetGameplayInputAvailable() const { return bGameplayInputAvailable; }

	// Ordered physical mouse edges are the single source for firing.
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual bool SetPause(bool bPause, FCanUnpause CanUnpauseDelegate = FCanUnpause()) override;
	virtual void FlushPressedKeys() override;
	UFUNCTION(BlueprintPure, Category="Prototype|Fire")
	bool IsFireAllowed() const;
	UFUNCTION(BlueprintCallable, Category="Prototype|Fire")
	void SetFireAllowed(bool bAllowed);
	void SetSafeAreaActive(AActor* Area, bool bActive);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input")
	FKey FireButton = EKeys::LeftMouseButton;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

private:
	void RegisterGameplayInput();
	void RemoveGameplayInput();
	void ClearMovementIntent();
	void ClearAimCursorSample();
	void CancelFireSeries();
	bool CanAcceptFirePress(const FInputKeyEventArgs& Params) const;
	bool bFireAllowed = true;
	TSet<TWeakObjectPtr<AActor>> SafeAreas;

	bool bGameplayInputAvailable = true;

	FVector2D ScreenMoveIntent = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RegisteredMappingContext;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem;
};
