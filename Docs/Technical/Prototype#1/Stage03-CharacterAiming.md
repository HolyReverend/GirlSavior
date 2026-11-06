# Stage 3 — Character Aiming Implementation Plan

> During implementation, use the project's `stage-orchestrator` workflow. Use the aiming scenario in Step 6 for acceptance.

**Goal.** In `Demo.umap`, turn the hero horizontally toward the mouse cursor on a plane through the body center. The hero can move in another direction while the view follows, and keeps the last facing near the body or while aim input is unavailable.

**Prerequisites.** [Stage 1](Stage01-CharacterMovement.md) supplies the possessed character, screen-relative WASD, and a fixed body reference at the capsule center. [Stage 2](Stage02-GameplayCamera.md) supplies the final gameplay POV and viewport projection. Stage 4 reads the current facing for projectile launch. Stage 5 exercises aim while dashing.

**Sources.** [Prototype plan](ImplementationPlan.md) · [Aim design](../../GameDesign/Mechanics/CharacterAIM.md) · [Shooting design](../../GameDesign/Mechanics/CharacterShootSystem.md)

## Step 1 — Add the aim component and define its ownership

- [ ] Add `UGirlSaviorAimComponent : UActorComponent` in `Source/GirlSavior/Characters/GirlSaviorAimComponent.h` and `.cpp`. Create it as a default subobject in `AGirlSaviorCharacter`. Keep the last horizontal direction, the hold flag, and the cursor-in-view sample on the component.
- [ ] In `BeginPlay`, seed the direction from a yaw-only rotator at the character's spawn and start with hold and cursor-in-view false. A new character starts from its own spawn yaw. A control interruption or a same-pawn camera reposition preserves the current direction and hold mode.
- [ ] Tick on the game thread in `TG_PostUpdateWork` for the locally possessed character. Keep the standard frame order of movement → final camera update → aim. Recompute every active frame, including when the cursor is stationary. Read the position of the capsule after movement, so the plane height follows the hero. [Epic Actor Ticking](https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine)

## Step 2 — Configure mouse input and sample the current gameplay view

- [ ] In `AGirlSaviorPlayerController`, keep `FInputModeGameAndUI`, a visible cursor, `DoNotLock`, and cursor hiding disabled. At this stage use `CaptureDuringMouseDown`, which `FInputModeGameAndUI` sets when applied. Remove the Stage 1 `NoCapture` override from the controller implementation. In the installed UE 5.8.4 viewport code, the temporary-capture path delivers the initial mouse press and can reacquire viewport focus on click. This also prepares the pointer path for Stage 4.
- [ ] Use the owning local controller and its own `ULocalPlayer` and `FViewport`. Allow gameplay aiming while the pawn is possessed locally, the game is unpaused, gameplay input is available, the game viewport has focus and its window is in the foreground, and the camera is on the gameplay view. Expose the shared check as `AGirlSaviorPlayerController::CanUseGameplayInput() const`. Reuse the controller's gameplay input availability state for full handoffs, with the pause, focus, and foreground checks evaluated live. Keep movement-specific locks in the movement path.
- [ ] Read `APlayerController::GetMousePosition` each active tick. It supplies the viewport's cursor sample in pixels. From the same viewport, call `ULocalPlayer::GetProjectionData` once and take `GetConstrainedViewRect()` and `ComputeViewProjectionMatrix()`. Use viewport pixel coordinates for both the cursor and projection. The constrained rectangle sets the origin and size for all screen-space comparisons. [Epic PlayerController API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerController) · [Epic projection data API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FSceneViewProjectionData)
- [ ] Set the current cursor-in-view sample true only when the mouse read succeeds and the cursor is inside the valid view rectangle, with inclusive minima and exclusive maxima. Clear the sample at the start of each aim tick. Have the controller also clear it on input suspension, `FlushPressedKeys`, and before `OnUnPossess` releases the pawn. Use the same controller gate to reject gameplay actions while paused, even when the aim component's tick is suspended. A cursor over letterboxing or past the viewport edge is outside the game view. Keep the cursor-in-view sample independent of the result of the aim geometry. Keep the last direction and hold flag when sampling or control is unavailable.

## Step 3 — Resolve the cursor on the body-height plane

- [ ] Let `BodyOrigin` be the current capsule center. Project it to `BodyScreen` using `FSceneView::ProjectWorldToScreen` with the sampled matrix and rectangle. Deproject the cursor with `FSceneView::DeprojectScreenToWorld` using the inverse of the same matrix and the same rectangle. The resulting ray has origin `RayOrigin` and unit direction `RayDirection`. [Epic FSceneView API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FSceneView)
- [ ] Intersect the ray analytically with the horizontal plane at `BodyOrigin.Z`. Reject failed body projection, invalid view or inverse matrices, non-finite ray values, a nearly horizontal ray (`abs(RayDirection.Z) ≤ 1e−6`), an intersection behind the ray (`t < 0`), or a non-finite hit. Preserve the current direction and hold mode on failure. Use the plane result for all cursor positions, including over walls, targets, floor edges, and gaps.

```text
t = (BodyOrigin.Z - RayOrigin.Z) / RayDirection.Z
hit = RayOrigin + t * RayDirection
planar = (hit.X - BodyOrigin.X, hit.Y - BodyOrigin.Y, 0)
```

## Step 4 — Apply screen-space hysteresis and yaw

- [ ] Expose editable defaults on the aim component for `HoldEnterFraction = 0.010` and `HoldExitFraction = 0.015`. Validate `0 < HoldEnterFraction < HoldExitFraction < 0.5`, with a diagnostic and fallback to the prototype defaults for invalid values. Multiply both fractions by the **height** of the constrained view rectangle. Use squared Euclidean distance between the cursor and `BodyScreen` in the comparisons.
- [ ] With valid aim geometry, choose the threshold based on the prior hold state. Enter hold at or below the inner radius. While holding, keep the last direction through the outer radius. Resume tracking only beyond the outer radius. A cursor exactly at the projected center is inside the hold zone. A planar vector too short to normalize keeps the prior state when it occurs outside the hold zone.

```text
limit = isHolding ? exitRadius : enterRadius
if distanceSquared(cursor, bodyScreen) <= limit * limit:
    isHolding = true
    keep last direction
else if planar is usable:
    direction = normalize(planar)
    yaw = degrees(atan2(direction.Y, direction.X))
    apply yaw and store direction
    isHolding = false
```

- [ ] Apply the accepted yaw with `SetActorRotation`, preserving the character actor's pitch and roll. Retain the Stage 1 rotation settings so Character Movement and the controller leave yaw ownership to aim. Screen-relative WASD still uses the camera basis. When control returns or the cursor re-enters, the first valid sample applies the same hysteresis rules using the preserved hold state.

## Step 5 — Expose the handoff to firing

- [ ] Provide `FVector GetAimDirection() const`, returning the stored unit vector in world XY. It is usable from spawn and throughout a valid hold. Provide `bool IsCursorInsideGameView() const` as the latest aim tick's cursor-in-view sample. An in-frame cursor with unusable plane geometry still leaves the stored facing usable. Keep gameplay control availability as a separate controller check.
- [ ] Stage 4 will place its firing tick in `TG_PostUpdateWork` and make the aim component its explicit tick prerequisite. A shot reads the updated direction and the character's current launch position. The firing series also checks control availability and cursor-in-view, including when control loss suspends aim updates. Stage 5 keeps aim updates active during a dash while movement owns the dash direction. [Epic tick dependencies](https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine#tick-dependency)

## Step 6 — Build and run one aiming scenario in PIE

- [ ] Build `GirlSaviorEditor Win64 Development` with the existing prototype build command and open `Demo.umap` in a separate PIE window. Use a temporary forward arrow and log the projected body point, cursor distance, view height, and hold state if visual checks need measurements.

| Route segment | Expected result |
| --- | --- |
| Spawn and pass through center | Start with the cursor near the spawn body center and confirm spawn yaw is retained. Establish a direction, then cross the center and pass back through the band between thresholds. Facing freezes at **≤1%** of view height, holds through **1.5%**, and changes beyond **1.5%**. The band preserves the preceding mode in both directions. |
| Move beneath a stationary cursor | Walk past the cursor while the camera follows. Confirm WASD remains screen-relative as yaw changes. Continue past a wall or floor gap so its silhouette passes beneath the cursor. The facing follows the body-height plane as the view moves, without snapping at the geometry edge. |
| Leave and return to the view | Move the cursor outside the game frame and back, including once while a mouse button is held. Facing holds outside. It uses the fresh in-frame sample on return, subject to the hold thresholds. |
| Suspend and resume | Pause, then take focus away and move the cursor before returning to the focused gameplay view and resuming. The last facing and hold mode survive the interruption. The next valid sample updates using the current frame and cursor. |

- [ ] Record the Editor build result, the view height and threshold measurements, and the observations from this route in the Stage 3 Project work report. Stage 4 verifies LMB delivery with the actual fire action. Stage 5 verifies aim during dash.
