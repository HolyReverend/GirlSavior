#include "Core/GirlSaviorPlayerController.h"
#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Characters/GirlSaviorCharacterMovementComponent.h"
#include "Characters/GirlSaviorAimComponent.h"
#include "UnrealClient.h"
#include "Combat/GirlSaviorFireComponent.h"
#include "Combat/GirlSaviorSafeArea.h"
#include "InputKeyEventArgs.h"
#include "SceneView.h"
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
		// Rebuild area permissions on possession, including a pawn spawned inside a zone.
		SafeAreas.Empty();
		TArray<AActor*> Overlaps;
		InPawn->GetOverlappingActors(Overlaps, AGirlSaviorSafeArea::StaticClass());
		for (AActor* Area : Overlaps) CastChecked<AGirlSaviorSafeArea>(Area)->RegisterPawn(InPawn);
		RestoreGameplayInputMode();
	}
}

// Use the same cursor and capture rules after possession and after closing a local modal.
void AGirlSaviorPlayerController::RestoreGameplayInputMode()
{
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
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

// A physical mouse press can reach the viewport before the window gains focus.
FViewport* AGirlSaviorPlayerController::GetGameplayViewport() const
{
	const APawn* ControlledPawn = GetPawn();
	if (!bGameplayInputAvailable || !IsLocalController() || !ControlledPawn || !ControlledPawn->IsLocallyControlled()
		|| !RegisteredMappingContext || !RegisteredInputSubsystem.IsValid() || IsPaused()
		|| GetViewTarget() != ControlledPawn)
	{
		return nullptr;
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
}

bool AGirlSaviorPlayerController::CanUseGameplayInput() const
{
	const FViewport* Viewport = GetGameplayViewport();
	return Viewport && Viewport->HasFocus() && Viewport->IsForegroundWindow();
}

void AGirlSaviorPlayerController::ClearAimCursorSample()
{
	if (AGirlSaviorCharacter* Hero = Cast<AGirlSaviorCharacter>(GetPawn()))
	{
		Hero->GetAimComponent()->ClearCursorSample();
	}
}

void AGirlSaviorPlayerController::SetGameplayInputAvailable(bool bAvailable)
{
	bGameplayInputAvailable = bAvailable;
	if (!bAvailable)
	{
		ClearMovementAndAim();
		CancelCombatInput();
	}
}

void AGirlSaviorPlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();
	CancelCombatInput();
	ClearMovementAndAim();
}

void AGirlSaviorPlayerController::OnUnPossess()
{
	CancelCombatInput();
	SafeAreas.Empty();
	ClearMovementAndAim();
	RemoveGameplayInput();
	Super::OnUnPossess();
}

void AGirlSaviorPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCombatInput();
	ClearMovementAndAim();
	RemoveGameplayInput();
	Super::EndPlay(EndPlayReason);
}

// Discard transient input while keeping the hero's last facing direction.
void AGirlSaviorPlayerController::ClearMovementAndAim()
{
	ClearMovementIntent();
	ClearAimCursorSample();
}

void AGirlSaviorPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);
	const bool bTryDash = bDashRequested;
	bDashRequested = false;
	ClearMovementIntent();

	AGirlSaviorCharacter* Hero = Cast<AGirlSaviorCharacter>(GetPawn());
	const UEnhancedPlayerInput* EnhancedInput = Cast<UEnhancedPlayerInput>(PlayerInput);
	const AGirlSaviorPlayerCameraManager* Camera =
		Cast<AGirlSaviorPlayerCameraManager>(PlayerCameraManager);
	const bool bCanMove = !bGamePaused && CanUseGameplayInput() && !IsMoveInputIgnored();
	if (!bCanMove || !Hero || !EnhancedInput || !MoveAction || !Camera)
	{
		ClearAimCursorSample();
		if (!bCanMove)
		{
			CancelDashInput();
		}
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
		Hero->AddMovementInput(WorldIntent.GetSafeNormal(), WorldIntent.Size());
		if (bTryDash && IsDashAllowed())
		{
			CastChecked<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement())
				->TryStartDash(WorldIntent);
		}
	}
}

// The down-edge sample uses the same constrained rectangle as aiming, before focus acquisition completes.
bool AGirlSaviorPlayerController::CanAcceptFirePress(const FInputKeyEventArgs& Params) const
{
	FViewport* Viewport = GetGameplayViewport();
	if (!Viewport || Params.Viewport != Viewport || !IsFireAllowed())
	{
		return false;
	}

	FSceneViewProjectionData Projection;
	float X = 0.f;
	float Y = 0.f;
	if (!GetMousePosition(X, Y) || !GetLocalPlayer()->GetProjectionData(Viewport, Projection)
		|| !FMath::IsFinite(X) || !FMath::IsFinite(Y))
	{
		return false;
	}
	const FIntRect Rect = Projection.GetConstrainedViewRect();
	return Rect.Width() > 0 && Rect.Height() > 0 && X >= Rect.Min.X && Y >= Rect.Min.Y
		&& X < Rect.Max.X && Y < Rect.Max.Y;
}

bool AGirlSaviorPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Read readiness on the physical press. Repeats or presses during cooldown never queue.
	if (Params.Key == EKeys::LeftShift && Params.Event == IE_Pressed
		&& IsDashAllowed() && CanUseGameplayInput() && !IsMoveInputIgnored() && !IsInSafeArea())
	{
		if (const auto* Hero = Cast<AGirlSaviorCharacter>(GetPawn()))
		{
			const auto* Movement = CastChecked<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement());
			bDashRequested = !Movement->IsDashing() && Movement->GetDashCooldownRemaining() <= 0.0;
		}
	}
	if (Params.Key == FireButton && FireButton.IsMouseButton())
	{
		if (auto* Hero = Cast<AGirlSaviorCharacter>(GetPawn()))
		{
			if (Params.Event == IE_Released) Hero->GetFireComponent()->SetFireButtonDown(false);
			else if ((Params.Event == IE_Pressed || Params.Event == IE_DoubleClick) && CanAcceptFirePress(Params))
				Hero->GetFireComponent()->SetFireButtonDown(true);
		}
	}
	return Super::InputKey(Params);
}

void AGirlSaviorPlayerController::CancelFireSeries()
{
	if (auto* Hero = Cast<AGirlSaviorCharacter>(GetPawn())) Hero->GetFireComponent()->CancelFireSeries();
}

// Interrupt both combat inputs while leaving their cooldowns intact.
void AGirlSaviorPlayerController::CancelCombatInput()
{
	CancelFireSeries();
	CancelDashInput();
}

// Shared cleanup for focus, pause and possession changes.
void AGirlSaviorPlayerController::CancelDashInput()
{
	bDashRequested = false;
	if (auto* Hero = Cast<AGirlSaviorCharacter>(GetPawn()))
	{
		CastChecked<UGirlSaviorCharacterMovementComponent>(Hero->GetCharacterMovement())->CancelDash();
	}
}

bool AGirlSaviorPlayerController::SetPause(bool bPause, FCanUnpause CanUnpauseDelegate)
{
	const bool bSucceeded = Super::SetPause(bPause, CanUnpauseDelegate);
	if (bSucceeded && bPause)
	{
		CancelCombatInput();
	}
	return bSucceeded;
}

bool AGirlSaviorPlayerController::IsFireAllowed() const
{
	return bFireAllowed && !IsInSafeArea();
}

// The area registry is shared by fire and dash. The explicit fire switch remains fire-specific.
bool AGirlSaviorPlayerController::IsInSafeArea() const
{
	for (const auto& Area : SafeAreas) if (Area.IsValid()) return true;
	return false;
}

void AGirlSaviorPlayerController::SetFireAllowed(bool bAllowed)
{
	bFireAllowed = bAllowed;
	if (!bAllowed) CancelFireSeries();
}

void AGirlSaviorPlayerController::SetSafeAreaActive(AActor* Area, bool bActive)
{
	if (!Area) return;
	if (bActive)
	{
		SafeAreas.Add(Area);
		CancelCombatInput();
	}
	else SafeAreas.Remove(Area);
}
