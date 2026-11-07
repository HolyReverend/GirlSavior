#include "Core/GirlSaviorPlayerController.h"
#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Characters/GirlSaviorCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogGirlSaviorInput, Log, All);

AGirlSaviorPlayerController::AGirlSaviorPlayerController()
{
	PlayerCameraManagerClass = AGirlSaviorPlayerCameraManager::StaticClass();
}

void AGirlSaviorPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (IsLocalController() && Cast<AGirlSaviorCharacter>(GetPawn()))
	{
		RegisterGameplayInput();
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UGameViewportClient* Viewport = LocalPlayer->ViewportClient)
			{
				// SetInputMode applies its own capture policy, so override it last.
				Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
			}
		}
	}
}

void AGirlSaviorPlayerController::RegisterGameplayInput()
{
	RemoveGameplayInput();
	if (!MoveAction)
	{
		UE_LOG(LogGirlSaviorInput, Error, TEXT("%s: MoveAction is missing."), *GetName());
		return;
	}
	if (MoveAction->ValueType != EInputActionValueType::Axis2D)
	{
		UE_LOG(LogGirlSaviorInput, Error, TEXT("%s: MoveAction must use Axis2D."), *GetName());
		return;
	}
	if (!GameplayMappingContext)
	{
		UE_LOG(LogGirlSaviorInput, Error, TEXT("%s: GameplayMappingContext is missing."), *GetName());
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogGirlSaviorInput, Error, TEXT("%s: Enhanced Input local player subsystem is missing."), *GetName());
		return;
	}
	if (!Cast<UEnhancedPlayerInput>(PlayerInput))
	{
		UE_LOG(LogGirlSaviorInput, Error, TEXT("%s: PlayerInput must use EnhancedPlayerInput."), *GetName());
		return;
	}
	Subsystem->AddMappingContext(GameplayMappingContext, 0);
	RegisteredMappingContext = GameplayMappingContext;
	RegisteredInputSubsystem = Subsystem;
}

void AGirlSaviorPlayerController::RemoveGameplayInput()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = RegisteredInputSubsystem.Get())
	{
		if (RegisteredMappingContext)
		{
			Subsystem->RemoveMappingContext(RegisteredMappingContext);
		}
	}
	RegisteredMappingContext = nullptr;
	RegisteredInputSubsystem.Reset();
}

void AGirlSaviorPlayerController::ClearMovementIntent()
{
	ScreenMoveIntent = FVector2D::ZeroVector;
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->ConsumeMovementInputVector();
	}
}

void AGirlSaviorPlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();
	ClearMovementIntent();
}

void AGirlSaviorPlayerController::OnUnPossess()
{
	ClearMovementIntent();
	RemoveGameplayInput();
	Super::OnUnPossess();
}

void AGirlSaviorPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMovementIntent();
	RemoveGameplayInput();
	Super::EndPlay(EndPlayReason);
}

void AGirlSaviorPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);
	ClearMovementIntent();

	AGirlSaviorCharacter* PrototypeCharacter = Cast<AGirlSaviorCharacter>(GetPawn());
	const UEnhancedPlayerInput* EnhancedInput = Cast<UEnhancedPlayerInput>(PlayerInput);
	const AGirlSaviorPlayerCameraManager* Camera =
		Cast<AGirlSaviorPlayerCameraManager>(PlayerCameraManager);
	if (bGamePaused || IsMoveInputIgnored() || !IsLocalController() || !PrototypeCharacter
		|| !PrototypeCharacter->IsLocallyControlled() || !RegisteredMappingContext
		|| !EnhancedInput || !MoveAction || !Camera)
	{
		return;
	}

	// Sample once after all signed key mappings have accumulated.
	const FVector2D EvaluatedIntent = EnhancedInput->GetActionValue(MoveAction).Get<FVector2D>();
	if (!FMath::IsFinite(EvaluatedIntent.X) || !FMath::IsFinite(EvaluatedIntent.Y))
	{
		return;
	}
	ScreenMoveIntent = EvaluatedIntent.GetClampedToMaxSize(1.f);
	const FVector WorldIntent = Camera->ToWorldMoveIntent(ScreenMoveIntent);
	if (!WorldIntent.IsNearlyZero())
	{
		PrototypeCharacter->AddMovementInput(WorldIntent.GetSafeNormal(), WorldIntent.Size());
	}
}
