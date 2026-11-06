# Stage 5 — Defensive Dash Implementation Plan

> During implementation, use the project's `stage-orchestrator` workflow. Use the dash route in Step 7 for acceptance.

**Goal.** In `Demo.umap`, let the player use Left Shift to dash in the current screen-relative movement direction. The dash follows walkable ground, reacts to obstacles, and briefly protects against direct hits while aim, fire, and camera continue.

**Prerequisites.** [Stage 1](Stage01-CharacterMovement.md) supplies the movement component and screen-space input. [Stage 2](Stage02-GameplayCamera.md) supplies camera follow and the doorway. [Stage 3](Stage03-CharacterAiming.md) supplies the gameplay control gate and aim. [Stage 4](Stage04-CharacterShooting.md) supplies ordered input edges, the area permission, the firing series, and typed damage reception.

**Sources.** [Prototype plan](ImplementationPlan.md) · [Dash design](../../GameDesign/Mechanics/CharacterCharge.md) · [Movement design](../../GameDesign/Mechanics/CharacterMovement.md)

## Step 1 — Add the dash mode and tuning

- [ ] Extend `Source/GirlSavior/Characters/GirlSaviorCharacterMovementComponent.h` and `.cpp`. Add a dash submode of `MOVE_Custom` to the existing synchronous Character Movement Component (CMC). It owns dash velocity, remaining movement time, the protection deadline, and the next ready time.
- [ ] Expose the following positive values as editable defaults on `BP_PrototypeCharacter` through its movement component. Validate them at start. Capture the tuning for the accepted dash so changes take effect on the next use.

| Setting | Prototype value |
| --- | --- |
| Unobstructed horizontal distance | **300 cm** |
| Full duration | **0.22 s** |
| Initial horizontal speed | **300 / 0.22 ≈ 1363.64 cm/s** |
| Direct-hit protection | **0.12 s** from start, capped by actual dash end |
| Cooldown | **1.0 s** from successful start |
| Minimum reachable horizontal path at start | **30 cm** |

- [ ] Keep gameplay rules in the movement component. Expose read-only queries for active dash, protection, and cooldown remaining. Character and UI presentation consume these queries.

## Step 2 — Consume a fresh Left Shift press and check admission

- [ ] In `AGirlSaviorPlayerController`, extend the ordered `InputKey(const FInputKeyEventArgs&)` path from Stage 4. Latch a single request on an actual `IE_Pressed` for `EKeys::LeftShift` when the possessed hero has gameplay control, is in the expedition area, and has a ready, inactive dash. Continue to forward events to `Super`. Treat repeats as ordinary input events with no dash request. Drop the request when gameplay control is lost, keys are flushed, or the pawn is released.
- [ ] Consume the request once in `PostProcessInput` after Stage 1 has evaluated WASD and before CMC movement. Use `GetScreenMoveIntent()` and `ToWorldMoveIntent()` to obtain the current world direction. Use a unit horizontal direction for equal straight and diagonal distance. Submit ordinary movement input as usual so the CMC has the latest intent if the request fails or the dash ends during the same frame. [Epic PlayerController API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerController)
- [ ] In `TryStartDash`, require the current gameplay control gate, the expedition area, `MOVE_Walking` on valid walkable support, a nonzero direction, and a ready cooldown. Reuse the Stage 4 safe-area state as the expedition gate. Run the path query in Step 3 before changing modes. A request below **30 cm** expires on this input pass with no cooldown or protection.
- [ ] On acceptance, store the fixed initial direction as velocity, enter the dash submode, and set `NextDashReadyGameTime = StartGameTime + 1.0` and `ProtectionEndGameTime = StartGameTime + 0.12` using `UWorld::GetTimeSeconds()` and the configured values. Early stops keep the original ready time. Requests during the dash or cooldown expire, including when Left Shift stays held past readiness.

## Step 3 — Share one ground-path query between admission and motion

- [ ] Use a private, query-only dash path helper on the movement component. Start from the actual capsule center and evaluate hypothetical capsule positions with the same size, collision channel, responses, and ignore rules as ordinary movement. The start probe advances virtual positions and accumulates **horizontal path length**. It succeeds as soon as **30 cm** are reachable within the dash time budget, including legal sliding. Keep the real capsule transform, overlap events, hit callbacks, and movement state outside the probe.
- [ ] Use short sweep legs of at most **10 cm** horizontal travel as the starting resolution. Check support at each proposed segment end, including slide legs, and shorten to the last supported position before an edge. Use the CMC floor distance and perch helpers with the walking slope, step height, and support limits. Supply those limits explicitly because entering `MOVE_Custom` clears `CurrentFloor` and its movement base. On a walkable ramp, use `ComputeGroundMovementDelta`. [Epic CMC API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UCharacterMovementComponent)
- [ ] For a walkable step, use `CanStepUp` and query-only up, forward, and down capsule sweeps. Require vertical clearance, landing on a walkable floor, and rise within `MaxStepHeight`. Apply the same step decision during the start probe and active dash. Count the horizontal part of the path only. Query sweeps evaluate the step without running the engine's live `StepUp` path, which can issue impact callbacks before rollback.
- [ ] At a blocking contact, first check the supported distance up to contact. A tagged `DashHardBlocker` ends the path on contact. Use this tag on a blocking enemy fixture, the gate while it has blocking collision, and a stair face authored as impassable. For an ordinary wall, project the current horizontal velocity onto the wall tangent and use it for the remaining time. Preserve the projected magnitude. End if there is no usable horizontal contact normal, the tangent is negligible, a second surface blocks it, or floor support ends. Bound contact retries and end on repeated contacts that make no progress. An open doorway uses the same path as any clear floor.

```text
horizontalNormal = normalizeXY(hit.ImpactNormal)
slideVelocity = velocity - dot(velocity, horizontalNormal) * horizontalNormal
unusedSliceTime = sliceTime * (1 - hitFraction)
```

## Step 4 — Commit movement and end the dash

- [ ] Dispatch the dash from `PhysCustom` with the usual base call. Start each frame from the actual capsule transform and query the upcoming legs against current geometry. Limit each slice to remaining frame time, remaining dash time, and the short leg bound. Use `SafeMoveUpdatedComponent` with a sweep for the approved legs. Treat a new runtime obstruction as an actual contact. Use a deferred movement scope for a leg or multi-leg step that may need rollback. Confirm the resulting pose is supported before committing it, and revert to the last supported pose if it is unsafe. Publish hit and overlap side effects for committed movement. [Epic MovementComponent API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UMovementComponent)
- [ ] Spend only the simulation time actually used by the dash. At a contact, subtract its travel time and use the leftover slice with the projected velocity when a slide is available. On open level ground, consume exactly **0.22 s** of movement time for **300 cm**. A final partial slice completes the distance at the correct speed. Keep slices short even when a frame is long. Bound the inner loop by remaining movement time and travel, with a separate limit for zero-progress contacts. Use the CMC iteration count for dispatch and mode handoff.
- [ ] Centralize exit in `FinishDash`. End on full duration, a hard blocker, exhausted slide, loss of supported path, loss of gameplay control, or a forced mode change. Clear dash velocity and protection. Resume `MOVE_Walking` at the last supported pose and let the base transition restore the floor and movement base. If support actually disappears under the pawn, return to stock `MOVE_Falling`. When exiting during simulation, pass unused frame time into `StartNewPhysics` within the iteration budget so current WASD can take effect immediately. Preserve the Stage 1 reset of walking response state.
- [ ] Cancel through the controller's control-loss hooks for gameplay suspension, successful pause entry, focus loss, and unpossession. Recheck the live gate during dash physics. Cancel before applying a forced mode change or a new external velocity. Use the mode-change callback as a fallback to clear protection and dash state if another system has already replaced the mode. Closing a gate in the path is a collision stop. A regular doorway keeps gameplay control, camera follow, and firing active.

## Step 5 — Filter direct damage and show dash state

- [ ] Extend `UGirlSaviorDamageReceiverComponent::TryApplyDamage` from Stage 4. Attach the receiver to the hero. Keep `DirectProjectile` and add `DirectMelee`, `Area`, and `FloorHazard` damage kinds. Query `IsDashProtected()` at damage resolution. It is true only while the dash is active and current game time is **strictly before** the protection deadline. Reject only direct projectile and melee damage during that window. Keep accepted damage, physical collision, and projectile impact cues as separate outcomes.
- [ ] Give the hero a visible silhouette outline driven by the same protection query. Enable the Custom Depth stencil pass and add `M_PP_PrototypeDashOutline` to `Demo` as a post-process material. Reserve one stencil value for the hero and toggle the hero mesh's stencil participation with protection. Update on the deadline as well as at start and early end, and restore the mesh setting on teardown. [Epic post-process materials](https://dev.epicgames.com/documentation/unreal-engine/post-process-materials-in-unreal-engine)
- [ ] Add `WBP_PrototypeDashReadiness` to the existing local controller Blueprint. Show cooldown remaining or ready from the CMC query. Use current control and area availability for its disabled state. The indicator reads the same original ready time after a blocked dash. Admission still checks direction and path when the player presses Left Shift.

## Step 6 — Set up the dash route in Demo

- [ ] Reuse the Stage 1 movement lane and Stage 2 doorway. Add markers for a free **300 cm** run, a close frontal block, and an oblique approach to a wall. Add a short walkable step and a floor edge in the same lane. Use one movable blocking fixture as a closing gate or enemy stand-in.
- [ ] Add a small test emitter that can issue `DirectProjectile`, `DirectMelee`, `Area`, and `FloorHazard` damage. Feed them through the hero's receiver to observe accepted or rejected damage during the protection window, after expiry, and after an early stop. Reuse the Stage 4 projectile and firing lane to check continuous fire, and keep the camera in its normal Stage 2 view.

## Step 7 — Build and run one dash acceptance route in PIE

- [ ] Build `GirlSaviorEditor Win64 Development` with the existing prototype build command and open `Demo.umap`. Log start, end reason, horizontal path, protection state, and next ready time. Run the same route while monitoring the outline and readiness indicator.

| Route segment | Expected result |
| --- | --- |
| Open lane and fresh press | A ready Left Shift with no movement input is rejected. On free level ground, straight and diagonal dashes cover **300 cm in 0.22 s** of dash simulation. Changing WASD after launch does not steer the dash. Normal movement uses current WASD when it ends. Holding Shift past the **1.0 s** cooldown does not restart the dash. |
| Clearance, obstacle, and support | A frontal start with less than **30 cm** available is rejected without cooldown. An oblique dash slides at the tangential speed and can stop in the corner. A gate that blocks after launch ends the dash at contact. The short walkable step is traversable and the floor edge stops the dash on supported ground. |
| Protection and readiness | Direct projectile and melee damage are rejected inside **0.12 s**, while area and floor hazards still apply. Direct damage applies after the deadline or early interruption. The outline follows the same window. The readiness indicator keeps the original cooldown after interruption. |
| Integrated dash | Dash through the open doorway while tracking the cursor and holding LMB. Camera follow remains bounded, aim stays cursor-driven, and the next shot uses the current position and aim at the ongoing cadence. A control handoff during the dash ends the dash and protection and leaves the original ready time in effect. |

- [ ] Record the build result, measured range and time, and the route observations in the Stage 5 Project work report.
