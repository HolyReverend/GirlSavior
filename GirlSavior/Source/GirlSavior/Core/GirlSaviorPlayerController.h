#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GirlSaviorPlayerController.generated.h"

class FViewport;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;

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

	// Ordered physical button edges feed firing and dash requests.
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual bool SetPause(bool bPause, FCanUnpause CanUnpauseDelegate = FCanUnpause()) override;
	virtual void FlushPressedKeys() override;
	UFUNCTION(BlueprintPure, Category="Prototype|Fire")
	bool IsFireAllowed() const;
	UFUNCTION(BlueprintCallable, Category="Prototype|Fire")
	void SetFireAllowed(bool bAllowed);
	void SetSafeAreaActive(AActor* Area, bool bActive);
	bool IsInSafeArea() const;
	bool RegisterWeaponInput();
	void RemoveWeaponInput();

protected:
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;
	virtual bool IsDashAllowed() const { return true; }
	void RestoreGameplayInputMode();

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Prototype|Input")
	FKey FireButton = EKeys::LeftMouseButton;

	UPROPERTY(EditDefaultsOnly, Category = "Prototype|Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Weapons|Input")
	TObjectPtr<UInputMappingContext> ExpeditionWeaponMappingContext;

	// Each action corresponds to the matching zero-based weapon slot.
	UPROPERTY(EditDefaultsOnly, Category = "Weapons|Input")
	TArray<TObjectPtr<UInputAction>> WeaponSlotActions;

private:
	void SelectWeaponSlot(int32 SlotIndex);
	void RegisterGameplayInput();
	void RemoveGameplayInput();
	FViewport* GetGameplayViewport() const;
	bool CanAcceptFirePress(const FInputKeyEventArgs& Params) const;
	void ClearMovementAndAim();
	void ClearMovementIntent();
	void ClearAimCursorSample();
	void CancelCombatInput();
	void CancelFireSeries();
	void CancelDashInput();

	bool bFireAllowed = true;
	bool bDashRequested = false;
	TSet<TWeakObjectPtr<AActor>> SafeAreas;

	bool bGameplayInputAvailable = true;

	FVector2D ScreenMoveIntent = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RegisteredMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RegisteredWeaponMappingContext;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem;
};
