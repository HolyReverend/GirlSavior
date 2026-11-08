# Stage 3 — Character Aiming Implementation Plan

Completed-stage checklist. Use [current architecture](../Architecture.md) for shared contracts. Read dated audit/execution sections only for historical evidence; a recorded pass is not a current test result.

> During implementation, use the project's `stage-orchestrator` workflow. Use the aiming scenario in Step 6 for acceptance.

**Goal.** In `Demo.umap`, turn the hero horizontally toward the mouse cursor on a plane through the body center. The hero can move in another direction while the view follows, and keeps the last facing near the body or while aim input is unavailable.

**Prerequisites.** [Stage 1](Stage01-CharacterMovement.md) supplies the possessed character, screen-relative WASD, and a fixed body reference at the capsule center. [Stage 2](Stage02-GameplayCamera.md) supplies the final gameplay POV and viewport projection. Stage 4 reads the current facing for projectile launch. Stage 5 exercises aim while dashing.

**Sources.** [Prototype plan](ImplementationPlan.md) · [Aim design](../../GameDesign/Mechanics/CharacterAIM.md) · [Shooting design](../../GameDesign/Mechanics/CharacterShootSystem.md)

## Step 1 — Add the aim component and define its ownership

- [x] Add `UGirlSaviorAimComponent : UActorComponent` in `Source/GirlSavior/Characters/GirlSaviorAimComponent.h` and `.cpp`. Create it as a default subobject in `AGirlSaviorCharacter`. Keep the last horizontal direction, the hold flag, and the cursor-in-view sample on the component.
- [x] In `BeginPlay`, seed the direction from a yaw-only rotator at the character's spawn and start with hold and cursor-in-view false. A new character starts from its own spawn yaw. A control interruption or a same-pawn camera reposition preserves the current direction and hold mode.
- [x] Tick on the game thread in `TG_PostUpdateWork` for the locally possessed character. Keep the standard frame order of movement → final camera update → aim. Recompute every active frame, including when the cursor is stationary. Read the position of the capsule after movement, so the plane height follows the hero. [Epic Actor Ticking](https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine)

## Step 2 — Configure mouse input and sample the current gameplay view

- [x] In `AGirlSaviorPlayerController`, keep `FInputModeGameAndUI`, a visible cursor, `DoNotLock`, and cursor hiding disabled. At this stage use `CaptureDuringMouseDown`, which `FInputModeGameAndUI` sets when applied. Remove the Stage 1 `NoCapture` override from the controller implementation. In the installed UE 5.8.4 viewport code, the temporary-capture path delivers the initial mouse press and can reacquire viewport focus on click. This also prepares the pointer path for Stage 4.
- [x] Use the owning local controller and its own `ULocalPlayer` and `FViewport`. Allow gameplay aiming while the pawn is possessed locally, the game is unpaused, gameplay input is available, the game viewport has focus and its window is in the foreground, and the camera is on the gameplay view. Expose the shared check as `AGirlSaviorPlayerController::CanUseGameplayInput() const`. Reuse the controller's gameplay input availability state for full handoffs, with the pause, focus, and foreground checks evaluated live. Keep movement-specific locks in the movement path.
- [x] Read `APlayerController::GetMousePosition` each active tick. It supplies the viewport's cursor sample in pixels. From the same viewport, call `ULocalPlayer::GetProjectionData` once and take `GetConstrainedViewRect()` and `ComputeViewProjectionMatrix()`. Use viewport pixel coordinates for both the cursor and projection. The constrained rectangle sets the origin and size for all screen-space comparisons. [Epic PlayerController API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerController) · [Epic projection data API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FSceneViewProjectionData)
- [x] Set the current cursor-in-view sample true only when the mouse read succeeds and the cursor is inside the valid view rectangle, with inclusive minima and exclusive maxima. Clear the sample at the start of each aim tick. Have the controller also clear it on input suspension, `FlushPressedKeys`, and before `OnUnPossess` releases the pawn. Use the same controller gate to reject gameplay actions while paused, even when the aim component's tick is suspended. A cursor over letterboxing or past the viewport edge is outside the game view. Keep the cursor-in-view sample independent of the result of the aim geometry. Keep the last direction and hold flag when sampling or control is unavailable.

## Step 3 — Resolve the cursor on the body-height plane

- [x] Let `BodyOrigin` be the current capsule center. Project it to `BodyScreen` using `FSceneView::ProjectWorldToScreen` with the sampled matrix and rectangle. Deproject the cursor with `FSceneView::DeprojectScreenToWorld` using the inverse of the same matrix and the same rectangle. The resulting ray has origin `RayOrigin` and unit direction `RayDirection`. [Epic FSceneView API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FSceneView)
- [x] Intersect the ray analytically with the horizontal plane at `BodyOrigin.Z`. Reject failed body projection, invalid view or inverse matrices, non-finite ray values, a nearly horizontal ray (`abs(RayDirection.Z) ≤ 1e−6`), an intersection behind the ray (`t < 0`), or a non-finite hit. Preserve the current direction and hold mode on failure. Use the plane result for all cursor positions, including over walls, targets, floor edges, and gaps.

```text
t = (BodyOrigin.Z - RayOrigin.Z) / RayDirection.Z
hit = RayOrigin + t * RayDirection
planar = (hit.X - BodyOrigin.X, hit.Y - BodyOrigin.Y, 0)
```

## Step 4 — Apply screen-space hysteresis and yaw

- [x] Expose editable defaults on the aim component for `HoldEnterFraction = 0.010` and `HoldExitFraction = 0.015`. Validate `0 < HoldEnterFraction < HoldExitFraction < 0.5`, with a diagnostic and fallback to the prototype defaults for invalid values. Multiply both fractions by the **height** of the constrained view rectangle. Use squared Euclidean distance between the cursor and `BodyScreen` in the comparisons.
- [x] With valid aim geometry, choose the threshold based on the prior hold state. Enter hold at or below the inner radius. While holding, keep the last direction through the outer radius. Resume tracking only beyond the outer radius. A cursor exactly at the projected center is inside the hold zone. A planar vector too short to normalize keeps the prior state when it occurs outside the hold zone.

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

- [x] Apply the accepted yaw with `SetActorRotation`, preserving the character actor's pitch and roll. Retain the Stage 1 rotation settings so Character Movement and the controller leave yaw ownership to aim. Screen-relative WASD still uses the camera basis. When control returns or the cursor re-enters, the first valid sample applies the same hysteresis rules using the preserved hold state.

## Step 5 — Expose the handoff to firing

- [x] Provide `FVector GetAimDirection() const`, returning the stored unit vector in world XY. It is usable from spawn and throughout a valid hold. Provide `bool IsCursorInsideGameView() const` as the latest aim tick's cursor-in-view sample. An in-frame cursor with unusable plane geometry still leaves the stored facing usable. Keep gameplay control availability as a separate controller check.
- [x] Stage 4 will place its firing tick in `TG_PostUpdateWork` and make the aim component its explicit tick prerequisite. A shot reads the updated direction and the character's current launch position. The firing series also checks control availability and cursor-in-view, including when control loss suspends aim updates. Stage 5 keeps aim updates active during a dash while movement owns the dash direction. [Epic tick dependencies](https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine#tick-dependency)

## Step 6 — Build and run one aiming scenario in PIE

- [x] Build `GirlSaviorEditor Win64 Development` with the existing prototype build command and open `Demo.umap` in a separate PIE window. Use a temporary forward arrow and log the projected body point, cursor distance, view height, and hold state if visual checks need measurements.

| Route segment | Expected result |
| --- | --- |
| Spawn and pass through center | Start with the cursor near the spawn body center and confirm spawn yaw is retained. Establish a direction, then cross the center and pass back through the band between thresholds. Facing freezes at **≤1%** of view height, holds through **1.5%**, and changes beyond **1.5%**. The band preserves the preceding mode in both directions. |
| Move beneath a stationary cursor | Walk past the cursor while the camera follows. Confirm WASD remains screen-relative as yaw changes. Continue past a wall or floor gap so its silhouette passes beneath the cursor. The facing follows the body-height plane as the view moves, without snapping at the geometry edge. |
| Leave and return to the view | Move the cursor outside the game frame and back, including once while a mouse button is held. Facing holds outside. It uses the fresh in-frame sample on return, subject to the hold thresholds. |
| Suspend and resume | Pause, then take focus away and move the cursor before returning to the focused gameplay view and resuming. The last facing and hold mode survive the interruption. The next valid sample updates using the current frame and cursor. |

- [x] Record the Editor build result, the view height and threshold measurements, and the observations from this route in the Stage 3 Project work report. Stage 4 verifies LMB delivery with the actual fire action. Stage 5 verifies aim during dash.

## Execution on 2026-11-07

- **State:** accepted on 2026-11-07 after technical verification and the user's manual confirmation: «Пройдено — всё работает». User requested Stage 3 and chose the existing checkout; no commit/push authorized or planned.
- **Baseline:** main at `8fd94e0fd49bc0b49004280400b9438524d5bc94`; accepted Stage 2 code, assets, configuration and documents are dirty/untracked and preserved. Archive and report folders also pre-exist. Live Editor identity: UE 5.8.4, this project's `GirlSavior.uproject`; zero dirty packages before work.
- **Scope:** Steps 1–6 above; no shooting/dash, animation or map redesign. Keep one aim component and one controller control-availability gate.
- **Work units:** primary owns component, character/controller integration, tests, editor/build and records. Sol low bounded worker investigates installed engine APIs read-only. Independent Sol high review follows implementation. No child delegation; one shared Editor/build owner.
- **Acceptance:** closed-editor Development build; deterministic plane/hysteresis/lifecycle probes, live viewport/tick and movement regressions; manual Step 6 route explicitly confirmed passed by the user.
- **Initial evidence:** native-class probe fails because `/Script/GirlSavior.GirlSaviorAimComponent` does not exist. Controller still forces NoCapture. No implementation in baseline.
- **Continuation:** implementation, builds, independent review and automated checks are integrated. The user explicitly confirmed the supplied manual route passed. No active child writers or callbacks. Primary model/effort and token use unavailable.
- **Review corrections:** installed UE `DeprojectScreenToWorld` truncates screen coordinates to integer pixels. Independent world-point checks now allow at most two projected pixels, rather than incorrectly demanding a subpixel inverse. Synthetic 960×720 and 1280×720 rects reuse the live camera matrix and include a translated rect origin; these are not claims of native window size. Exact inner boundary failed twice at 720 px before correction (`Saved/Stage3/BoundaryRed.txt`); promote the configured fraction to double before scaling. Added horizontal/behind-ray rejection and same-pawn unpossess/repossess checks.
- **Precision fix:** final implementation now scales the fractions in double precision. Production source and corrected tests built successfully (UBT exit 0, build `21a5f8e44f594e048918cacb59ce31c2`). Whitespace cleanup was followed by the final verification build listed below.
- **Deterministic verification — Pass:** final source build `47dd61b3b4ec44c9ace8b55fb6664a76` succeeded with exit 0, zero compiler/linker error lines and no source changes during build. `GirlSavior.TestAim 960 720` and `GirlSavior.TestAim 1280 720` each recorded **58 checks, 0 failures**, including exact inner/outer thresholds, offset rectangles, finite/singular inputs, parallel and behind-plane rays, height changes, flush/suspension and same-pawn unpossess/repossess. Evidence: `Saved/Stage3/Aim960x720.txt`, `Aim1280x720.txt`. At a 720 px test height the radii are **7.2 / 10.8 px**; the real embedded viewport was 1546×744, where default radii are **7.44 / 11.16 px**.
- **Live frame/input verification — Pass:** `Tests/VerifyStage03Aim.py` observed 40 actual aim ticks under a stationary cursor while the pawn moved and the camera followed, matching independently queried current-cache mouse rays (minimum direction dot product rounded to 1.0). Five suspended-input samples and four paused samples preserved facing and rejected gameplay input; resuming used the new cursor sample. Evidence: `Saved/Stage3/LiveAim.json`; `passed=true`, no failures. The observer unregisters itself; callback tracker subsequently reported zero. This uses viewport cursor positioning and controlled pawn repositioning, not physical keyboard/mouse acceptance.
- **Regression — Pass:** Stage 2 camera probe at 960×720 recorded 101 checks / 0 failures. Fresh Stage 1 movement captures plus `Tests/VerifyStage01Movement.py` passed at 60 and 30 FPS with no evaluator failures. Start/reverse/stop timings: 60 FPS = 0.100000501 / 0.100000486 / 0.083333798 s; 30 FPS = 0.100000307 / 0.100000396 / 0.100000203 s. Existing one-update tolerances were retained.
- **Visual/editor state:** inspected native game-viewport capture `Saved/Screenshots/Stage3_AimTracking.png`; the temporary green aim arrow is visible. `GirlSavior.AimDebug 1` enables green tracking / yellow hold arrows (0 disables); no asset was added for it. Current PIE viewport readback was 1125×553 after window activation, giving nominal radii 5.53 / 8.295 px; captures can differ by one raster pixel from the logical viewport width. View height remains the scale source. Zero dirty packages and zero callbacks at the final automated readback. Editor and PIE remain available for the user.
- **Review/attempts:** independent Sol high source/adequacy review found integer-pixel oracle mismatch and fraction scaling precision; both corrected by primary and reverified. The boundary regression was observed failing before the fix and passing afterward. Four actual Editor builds succeeded; last build took 7.90 s. One Python helper API mismatch (`get_pawn`) failed before mutating the scene, corrected to GameplayStatics.get_player_character, then the live route passed. Expected invalid-tuning warnings and exploratory assertion/API errors remain in logs; no error-free-log claim. Visual agent needed one targeted correction to activate the game viewport; no settings or content changed. Physical-input acceptance was subsequently confirmed by the user; packaging, actual firing and dash remain NotRun.
- **Manual acceptance — Pass (user reported):** the user answered «Пройдено — всё работает» to the supplied separate-window, center/band, stationary-cursor/WASD, leave/re-enter with held LMB, Alt+Tab and pause route. Step 6 is accepted; no additional manual measurements were provided. No commit or push. Rough observed task duration about 25 minutes; usage/token totals unavailable, no savings claim.

### Manual acceptance route

1. Stop the embedded PIE session, then choose **Play → New Editor Window (PIE)** for Demo. Enable `GirlSavior.AimDebug 1` in the console if needed; the cvar lasts only for the Editor session.
2. Sweep the mouse through the projected capsule center and back through the hysteresis band. Tracking is green; hold is yellow. At 720 px view height the defaults are 7.2 / 10.8 px; other heights scale proportionally. Walk with WASD beneath a stationary cursor, including near the existing wall/floor edge: aim follows the body plane and movement remains camera-relative.
3. Leave and re-enter the game view, including with LMB held. Facing holds outside and resumes with the current in-view sample. Actual firing will be implemented and checked in Stage 4.
4. Use the console `pause` command, close the console, move the mouse and Alt+Tab away/back. Resume with `pause`. Facing and hold survive interruptions; fresh valid input resumes under the same thresholds. Report pass, failure details, or defer.
