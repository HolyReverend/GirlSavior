# Stage 1 — Character Movement Implementation Plan

> During implementation, use the project's `stage-orchestrator` workflow for the stage and this checklist for its acceptance.

**Goal.** Make a locally possessed character move relative to the screen with WASD, short inertia, and standard Unreal walking collision in `Demo`.

**Architecture.** Enhanced Input evaluates WASD on the local controller. The camera manager supplies the fixed horizontal basis. The controller submits one world-space intent per input pass. A custom Character Movement Component (CMC) defines the grounded velocity response, and stock Unreal walking moves the capsule and resolves collisions.

**Tech stack.** Unreal Engine 5.8.4, the existing `GirlSavior` runtime module, Enhanced Input, C++, Blueprint class defaults, and one greybox PIE map. Stage 1 covers movement and the rigid follow view it needs. Stage 2 owns smoothing and room framing. Stage 3 owns aiming yaw.

**Prerequisite.** [Stage 0](Stage00-ProjectPreparation.md) provides the verified Editor target and the saved `Demo` map with a floor, basic lighting, and `PlayerStart`.

**Sources.** [Prototype plan](ImplementationPlan.md) · [Movement design](../../GameDesign/Mechanics/CharacterMovement.md) · [Camera design](../../GameDesign/Mechanics/GameCamera.md) · [C++ and Blueprints](../../../Codex/Cpp.md) · [Data-driven design](../../../Codex/DataDriven.md)

## Step 1 — Set up the native classes

- [ ] Add the five C++ classes below. Paths are relative to `GirlSavior/Source/GirlSavior/`. Give each class a matching header and source file. Keep engine includes in the source file where forward declarations suffice.
- [ ] In the character constructor, use `FObjectInitializer::SetDefaultSubobjectClass<UGirlSaviorCharacterMovementComponent>(ACharacter::CharacterMovementComponentName)` in the base initializer. This installs the custom CMC as the character's built-in movement component and keeps the capsule as the updated component. [Epic subobject class API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/FObjectInitializer/SetDefaultSubobjectClass)
- [ ] Set `bUseControllerRotationYaw`, `bUseControllerRotationPitch`, `bUseControllerRotationRoll`, `bOrientRotationToMovement`, and `bUseControllerDesiredRotation` to false. Use the capsule center as the stable body point for framing and later aim.
- [ ] Set the native pawn and controller defaults in `AGirlSaviorPrototypeGameMode`. Set `PlayerCameraManagerClass` to `AGirlSaviorPlayerCameraManager` in the controller constructor. Retain the existing `Build.cs` dependencies, which already include `EnhancedInput`.

| Files | Ownership |
| --- | --- |
| `Core/GirlSaviorPrototypeGameMode.h` and `.cpp` | `AGirlSaviorPrototypeGameMode : AGameModeBase`. Select the prototype pawn and controller. |
| `Core/GirlSaviorPlayerController.h` and `.cpp` | `AGirlSaviorPlayerController : APlayerController`. Own `IA_Move` and `IMC_Gameplay` references, input lifecycle, and the current screen-space movement intent. |
| `Camera/GirlSaviorPlayerCameraManager.h` and `.cpp` | `AGirlSaviorPlayerCameraManager : APlayerCameraManager`. Own the fixed perspective view and conversion from screen axes to ground axes. |
| `Characters/GirlSaviorCharacter.h` and `.cpp`, `Characters/GirlSaviorCharacterMovementComponent.h` and `.cpp` | Compose the pawn and capsule. Own walking tuning and the grounded velocity update in `UGirlSaviorCharacterMovementComponent`. |

## Step 2 — Implement the fixed follow view

- [ ] Override `UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)` in the camera manager. For the prototype character, fill `OutVT.POV` as a perspective view at `Pivot - ViewRotation.Vector() * FollowDistance`, where `Pivot` is the current capsule center. Set the POV rotation to pitch **−35°**, yaw **45°**, roll **0°**. Use **45° FOV** and **1600 cm** distance as temporary class defaults. Update the pivot directly from the current target every frame. Use the base implementation for other view targets. [Epic camera update API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerCameraManager/UpdateViewTargetInternal)
- [ ] Add `FVector ToWorldMoveIntent(const FVector2D& ScreenIntent) const` to the camera manager. Build unit forward and right from the **same yaw setting** as the POV, with pitch and roll zero. Return `Right * ScreenIntent.X + Forward * ScreenIntent.Y`. The caller supplies an intent of length at most one. This basis is available before the first camera update and stays independent of character rotation and later camera lag.
- [ ] Use the engine's normal camera update for the view and the normal possession tick dependency for controller input before movement. The installed 5.8.4 `AController::AddPawnTickDependency` registers the pawn movement prerequisite on the controller. [Epic Actor Ticking](https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine)

## Step 3 — Author and route WASD input

- [ ] Create `Content/Input/Actions/IA_Move.uasset`. Set Value Type to **Axis2D**, Accumulation Behavior to **Cumulative**, and Trigger When Paused to false. Leave triggers empty. Create `Content/Input/IMC_Gameplay.uasset` and add the four mappings below. Key modifiers run in the order shown. [Epic Enhanced Input accumulation](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/EnhancedInput/EInputActionAccumulationBehavior)

| Key | Mapping modifiers | Result |
| --- | --- | --- |
| D | None | `(+1, 0)` |
| A | Negate | `(−1, 0)` |
| W | Swizzle Input Axis Values `YXZ` | `(0, +1)` |
| S | Negate, then Swizzle Input Axis Values `YXZ` | `(0, −1)` |

- [ ] On the controller, expose `EditDefaultsOnly` `TObjectPtr<UInputAction> MoveAction` and `TObjectPtr<UInputMappingContext> GameplayMappingContext`. After base `OnPossess` succeeds for a locally controlled prototype pawn, register the assigned context at priority **0** on the owning `UEnhancedInputLocalPlayerSubsystem`. Track the context this controller registered and remove it on unpossession and `EndPlay`. Validate the action, context, and subsystem at setup and log a specific diagnostic if any required reference is missing. [Epic Enhanced Input](https://dev.epicgames.com/documentation/unreal-engine/enhanced-input-in-unreal-engine)
- [ ] Override `PostProcessInput(float DeltaTime, bool bGamePaused)` and call `Super`. If the controller has gameplay movement input available, read `UEnhancedPlayerInput::GetActionValue(MoveAction).Get<FVector2D>()` after the mapping pass. Clamp the vector to length one and expose that frame's result through `GetScreenMoveIntent() const`. Send the camera-relative direction to `AddMovementInput(WorldIntent.GetSafeNormal(), WorldIntent.Size())` once for nonzero input. An inactive or canceled action produces zero intent. Reading after the full mapping pass also gives the later dash stage a single place to sample WASD. [Epic PlayerController API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerController) · [Epic GetActionValue API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/EnhancedInput/UEnhancedPlayerInput/GetActionValue)
- [ ] Treat pause, ignored movement input, or missing local possession as zero input. Clear cached intent and consume any pending pawn input on `FlushPressedKeys` and before unpossession. Call the base methods and keep the existing viewport focus-loss flush setting. Set a visible cursor using `FInputModeGameAndUI` with `DoNotLock` and cursor hiding disabled. Apply `NoCapture` on the viewport client after the input mode, since the input mode sets its own capture policy. Use the same input configuration for movement in the expedition and shelter.

## Step 4 — Implement the short-inertia walking response

- [ ] On the CMC, set inherited `MaxWalkSpeed` to **500 cm/s**. Add editable class defaults `MoveResponseTime` **0.10 s** and `StopResponseTime` **0.08 s**, with a positive minimum in the editor. Guard finite positive speed, response times, and acceleration scaling at the point of use. With invalid tuning, emit a diagnostic and use base CMC behavior.
- [ ] Override `CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)` for the locally controlled player's synchronous `MOVE_Walking` path. Preserve engine guards for invalid data, tiny time steps, and root motion, and route other movement paths through `Super::CalcVelocity`. Keep CMC floor checks, swept movement, stepping, and wall sliding in `PhysWalking`. Reset the stored target and rate when entering or leaving `MOVE_Walking`, so a later dash can resume with a fresh walking response. [Epic CalcVelocity API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UCharacterMovementComponent/CalcVelocity) · [Epic MoveAlongFloor API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UCharacterMovementComponent/MoveAlongFloor)
- [ ] Use the current planar `Acceleration / GetMaxAcceleration()`, clamped to unit length, as input intent. UE scales the consumed movement input by `GetMaxAcceleration()` before walking. The target is this intent times `GetMaxSpeed()`. Read the **current** horizontal `Velocity` on every walking substep, including after collision response.
- [ ] Apply a bounded vector step toward the target using the substep's `DeltaTime`. Recalculate the movement rate when the nonzero target changes beyond a small vector tolerance, and retain it across subsequent substeps for that target. Store the current target after each calculation, including zero, so the next start initializes a new rate. Use a fixed braking rate when input cancels or is released. When the remaining distance fits in the step, snap to the target. Preserve the normal CMC movement pipeline after this calculation.

```text
target = planarIntent × walkSpeed
current = actual planar Velocity at start of this substep

when target differs from previousTarget and target is nonzero:
    moveRate = max(walkSpeed, length(target - current)) / MoveResponseTime

rate = moveRate when target is nonzero, otherwise walkSpeed / StopResponseTime
step = rate × substepSeconds
next = current + clamp_length(target - current, step)
Velocity = next with Z = 0
previousTarget = target
```

| Flat-floor case | Rate at prototype defaults | Expected velocity |
| --- | --- | --- |
| Start from rest | 5000 cm/s² | Reach the 500 cm/s target in 0.10 s. |
| Reverse at full speed | 10000 cm/s² | Cross zero and reach 500 cm/s in the opposite direction within 0.10 s. |
| Release at full speed | 6250 cm/s² | Reach zero in 0.08 s. |

Stock walking can recompute `Velocity` from actual capsule displacement after a sweep. Leave that result in place for the next substep. An oblique wall hit can then keep its tangential motion, a frontal hit can stop, and new free-direction input starts from the post-impact velocity.

## Step 5 — Build and assemble the prototype map

- [ ] Build `GirlSaviorEditor Win64 Development` with the installed UE 5.8.4 before opening the new reflected classes in the Editor. Close the project Editor before running the full build.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' GirlSaviorEditor Win64 Development '-Project=E:/Projects/VibeCoding/GirlSaviorProject/GirlSavior/GirlSavior.uproject' -WaitMutex
```

- [ ] Create `BP_PrototypeCharacter` under `Content/Blueprints/Characters/` and `BP_PrototypePlayerController` plus `BP_PrototypeGameMode` under `Content/Blueprints/Core/`. Parent them to the native classes above. Assign the input assets on the controller and the Blueprint pawn and controller classes on the GameMode. Give the character a visible placeholder mesh with collision disabled so the capsule owns contact.
- [ ] Open the Stage 0 map `Content/Maps/Demo.umap` and set its **World Settings → GameMode Override** to `BP_PrototypeGameMode`. Verify capsule clearance at the existing `PlayerStart`. Add a straight wall across the camera-forward direction on the existing **2400 × 2400 cm** floor. Give the wall simple blocking collision and a height above the configured `MaxStepHeight`. Leave space along the wall for sliding. Orient floor markers to the camera basis so left, right, and forward are clear. Launch PIE from `PlayerStart` in this map.

## Step 6 — Run one focused movement acceptance scenario in PIE

- [ ] Capture evaluated screen intent, world position, and actual planar velocity after movement updates. Use temporary development logging or the debugger. Record elapsed **simulation time** from the beginning of the first movement update that consumes each changed intent. For timing, accept at most one observed update interval of sampling tolerance and use a **1 cm/s** velocity tolerance. Compare the full target vector on reversal. A speed-only reading would pass while the pawn is still moving the wrong way.
- [ ] Use the same greybox lane for the three checks below. Use a stable 60 FPS cap for the full pass and repeat only the timing sequence with a 30 FPS cap to catch frame-based velocity steps. Record the observed update intervals with the results.

| Check | Input and expected result |
| --- | --- |
| Screen directions and speed | Use WASD individually, then hold `W+D` and `W+S+D`. Direction follows the screen, straight and diagonal steady speeds both reach 500 cm/s, and `W+S+D` resolves to right. Opposites on both axes yield zero. |
| Start, turn, and stop | On open floor, hold `W` from rest, switch to `S` at full speed, then release at full speed. Verify the **0.10 s**, **≤0.10 s**, and **0.08 s** targets within sampling tolerance. Verify velocity responds on the first update after each change. |
| Collision response | Hold `W+D` against the wall and confirm lateral displacement without penetration. Approach head-on with `W` and confirm the capsule stops at contact. Switch to free input and verify motion resumes on the first eligible update. |

- [ ] Record the successful Editor build, the three PIE observations, and the actual measured timings in the Stage 1 work report. Keep the map and the camera basis as the entry point for Stage 2.
