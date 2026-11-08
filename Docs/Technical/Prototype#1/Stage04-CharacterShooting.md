# Stage 4 — Character Shooting Implementation Plan

> During implementation, use the project's `stage-orchestrator` workflow. Use the firing lane in Step 6 for acceptance.

**Goal.** In `Demo.umap`, fire one simple test projectile on a ready press and repeat at the configured rate while the button is held. Each shot uses the character's position and current horizontal aim in that frame.

**Prerequisite.** [Stage 3](Stage03-CharacterAiming.md) supplies `CanUseGameplayInput()`, `GetAimDirection()`, `IsCursorInsideGameView()`, and the visible, unlocked `CaptureDuringMouseDown` setup. [Stage 2](Stage02-GameplayCamera.md) supplies the doorway fixture and final camera update. [Stage 5](Stage05-DefensiveDash.md) will verify uninterrupted firing during dash and extend damage reception with protection.

**Sources.** [Prototype plan](ImplementationPlan.md) · [Shooting design](../../GameDesign/Mechanics/CharacterShootSystem.md) · [Dash design](../../GameDesign/Mechanics/CharacterCharge.md)

## Step 1 — Route the configured fire button

- [x] Add `UGirlSaviorFireComponent` as a default subobject on `AGirlSaviorCharacter`. The player controller passes mouse-button edges to this component. Use the component to own the current firing series and its timing.
- [x] Expose one `FKey FireButton` on `BP_PrototypePlayerController`, defaulting to **LMB**. Override `InputKey(const FInputKeyEventArgs&)` and forward the configured mouse button's `IE_Pressed` and `IE_DoubleClick` as down edges and `IE_Released` as up. Keep the normal `Super::InputKey` path. Use these ordered edges as the single firing input source. UE 5.8.4 Enhanced Input action evaluation can combine transitions received between two input ticks. The edge path preserves short clicks and distinguishes a release followed by a new press in that interval. [Epic PlayerController API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerController) · [Epic Enhanced Input trigger events](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/EnhancedInput/ETriggerEvent)
- [x] Accept a down edge for the locally possessed hero from the owning viewport while game control and the current area permit shooting. On down, read the owning viewport's current mouse sample and compare it with the same constrained game-view rectangle Stage 3 uses. Defer the live focus check to the firing tick, since the viewport forwards a focus-acquiring click before applying focus. The first in-view LMB press can count once gameplay owns focus. Let up clear the held state even if permissions changed.
- [x] Add one `CancelFireSeries()` path to clear held input and the current-frame press flag. Call it on explicit gameplay suspension, successful entry into gameplay pause, `FlushPressedKeys`, unpossession, and entry into the safe area. Reuse the controller's existing lifecycle and handoff paths. Route gameplay pause through `SetPause` and clear the series when pause begins, since the firing component stops ticking during pause. A later down edge can start a new series.

## Step 2 — Schedule a single shot after aim

- [x] Tick the component in `TG_PostUpdateWork` and use `AddTickPrerequisiteComponent` to run it after the aim component. Read the current control gate, combat permission, and `IsCursorInsideGameView()` before resolving a shot. Cancel the series when any gate fails, including when the frame samples the cursor outside the game view. Re-entering the view with LMB still held leaves the series idle. Keep an open-door crossing in the same series while control and fire permission stay available. [Epic tick dependencies](https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine#tick-dependency)
- [x] Store `NextShotGameTime` as a `double` in `UWorld::GetTimeSeconds()` units. Initialize a newly spawned character as ready. Use editable `ShotsPerSecond` on the fire component with **4.0 shots/s** as the first tuning value. Require a finite, positive rate and a valid projectile class when resolving a shot.
- [x] A down edge sets held and records a press flag for this frame only if the shot was ready at that edge. An up edge clears held but preserves an eligible press for the current firing tick. This lets a ready click that is pressed and released before the tick still shoot once. A released click from the cooldown window expires without waiting for the deadline. Keep the deadline through release, cancellation, and rate changes.
- [x] On the firing tick, consume the press flag. Fire once if the deadline is due and either the button is held or the consumed press was ready. On successful launch, compute the next deadline from **the actual shot time** and the rate used for that shot. After a hitch, this resumes from one shot at most. Game time freezes the remaining interval during pause. If projectile spawning fails, log the configuration error and cancel the series so it can be tried again on a new press. [Epic UWorld time API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UWorld)

```text
readyPress = consumeReadyPressThisFrame()
if controlUnavailable or fireDisallowed or cursorOutside:
    cancel series
else if now >= nextShotTime and (buttonHeld or readyPress):
    on successful launch:
        nextShotTime = now + 1 / shotsPerSecond
```

## Step 3 — Spawn and move the test projectile

- [x] Add `AGirlSaviorTestProjectile` with a sphere as its collision root and `UProjectileMovementComponent` driving that sphere. Enable swept collision. Set gravity to zero and bouncing off. Set initial velocity to local +X with `bInitialVelocityInLocalSpace` enabled. Set the launch yaw from the horizontal aim so Projectile Movement converts that direction on initialization. Start with **2000 cm/s** speed, a **6 cm** sphere radius, and a **1.5 s** lifetime. Author the visible placeholder in `BP_TestProjectile`. [Epic Projectile Movement API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UProjectileMovementComponent)
- [x] Create the `PrototypeProjectile` object channel with default response **Ignore**. Give the sphere Query Only collision. Make the test target and blocking surfaces in the firing lane block that channel. Give the projectile sphere matching responses to their object types. Leave the player capsule and other projectiles ignoring the channel. Use simple collision and make the cosmetic mesh collision-free.
- [x] At the moment of firing, spawn the projectile at the capsule center with the current yaw from `GetAimDirection()`. Set the firing character as owner and instigator and use `AlwaysSpawn` so the launch position is kept. Set `IgnoreActorWhenMoving(GetOwner(), true)` on the projectile sphere in `BeginPlay` before its first sweep. Keep the projectile radius smaller than the character capsule radius and use the same blocking geometry for walking and projectile contact in the test lane. The shot starts on the hero's side of a wall even when the capsule touches it. [Epic movement collision ignore API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UPrimitiveComponent/IgnoreActorWhenMoving)

## Step 4 — Stop on impact and expose the damage handoff

- [x] Bind the projectile's `OnProjectileStop` once. For the first blocking hit, use its `FHitResult` to find a damage receiver on the hit actor. Pass a small positive amount tagged `DirectProjectile` to that receiver if present. Publish a distinct target or obstacle impact cue at the hit point, then destroy the projectile. Lifetime expiry cleans up a shot that reaches empty space. [Epic Projectile Movement API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UProjectileMovementComponent)
- [x] Add a minimal `UGirlSaviorDamageReceiverComponent` with a typed entry point such as `TryApplyDamage(EGirlSaviorDamageKind Kind, float Amount)`. Validate the amount and publish accepted damage for the test target. Stage 5 can put the protection check in this same entry point before publishing accepted damage. Classify impact by the presence of the receiver, so rejection of damage and physical projectile impact can be observed separately.
- [x] Emit a shot cue when the spawn succeeds. Use Blueprint presentation for the shot and two impact outcomes. Let impact cues outlive the projectile actor so they remain visible after it is destroyed.

## Step 5 — Set up the existing prototype level

| Path, relative to `GirlSavior/` | Stage 4 work |
| --- | --- |
| `Source/GirlSavior/Combat/` | Add matching `.h` and `.cpp` files for `GirlSaviorFireComponent`, `GirlSaviorTestProjectile`, and `GirlSaviorDamageReceiverComponent`. |
| `Source/GirlSavior/Core/GirlSaviorPlayerController.h/.cpp` and `Source/GirlSavior/Characters/GirlSaviorCharacter.h/.cpp` | Wire the input edges, control lifecycle, and character components. |
| `Content/Blueprints/Core/BP_PrototypePlayerController` and `Content/Blueprints/Characters/BP_PrototypeCharacter` | Set the button, projectile class, and initial fire rate. |
| `Content/Blueprints/Core/` and `Content/Maps/Demo.umap` | Add the projectile and test-target Blueprints, a wall beside the existing lane, and a safe-area trigger. |

- [x] Put a stationary target with the receiver on a clear line from the player. Place a wall or closed test door beside it for blocked shots. Reuse the open doorway from Stage 2. Add a nonblocking safe-area trigger that disables firing on entry and restores permission on exit. Route this permission through the controller separately from general gameplay control. Place the trigger away from the default spawn.

## Step 6 — Build and run one firing scenario in PIE

- [x] Build `GirlSaviorEditor Win64 Development` with the existing prototype build command. Play `Demo.umap` in a separate PIE window. Log each actual shot's game-time timestamp, launch position, and impact kind. Use the same lane for the checks below.

| Route segment | Expected result |
| --- | --- |
| Press, hold, release | The first ready LMB press fires. A short ready tap fires once, a tap inside cooldown is discarded, and a press held across cooldown fires on the first eligible update. At 4.0 shots/s, no two launches occur less than 0.25 game seconds apart. Release stops repeats. |
| Projectile delivery | While moving, fire in open space, at the target, and against the wall from close range. Each projectile launches from the hero's current position and aim. The shooter is clear, the first blocking hit ends travel, and target and wall produce different cues at the normal cadence. |
| Interrupt and rearm | With LMB held, leave and return to the view. Fire stops and waits for a fresh press. Pause during a live interval and verify that the remaining game-time delay is preserved and a new press is required. Check that an open doorway preserves the series and entering the safe area cancels it. |
| Change rate and resume after a long frame | Change `ShotsPerSecond` on the live component while an interval is pending. Use the timestamp log to confirm that the current deadline survives the change and the next shot sets its interval from the new rate. After a controlled hitch with the button held, at most one shot launches on the next update. |

- [x] Record the build result, starting fire rate, and route observations in the Stage 4 Project work report. Carry the typed impact and cadence contracts to Stage 5.

## Execution record — 2026-11-08

- State: accepted. The user confirmed the manual check passed on 2026-11-08. Stage 5 dash excluded.
- Baseline: 1d538dd6efe4d33a6a205a63f9d732afad72cd62; untracked Archive/ and report directories predate this task. No editor running.
- Authorization: implement Stage 4 in existing checkout; no commit/push requested.
- Ownership: primary owns fire, controller/character, configuration, assets, build, checks, docs. Bounded-worker owns native projectile, receiver, safe area.
- Checks: Final Editor Win64 Development build succeeded (2fe65187a8b34b51b230c8a1fbade371); standalone cadence tests 18/18; final project Automation 2/2; final native/authored PIE combat probes 37/37. Evidence: GirlSavior/Saved/Stage4/. Separate-window ready clicks, projectile, yellow shot cue, red obstacle cue, and target/green impact smoke were observed.
- Contract: SetSafeAreaActive(AActor*, bool) tracks overlapping safe areas independently of general input.
- Usage: token/cost metadata unavailable; no savings claim.

### Implementation notes

- FireSchedule keeps ordered edge state and the double game-time deadline; FireComponent launches after AimComponent.
- Native SafeArea supplements the planned three combat types. Controller possession synchronizes both area/controller registries, covered by a real-volume regression.
- Demo target: (-350, -200, 80); wall: (-100, -420, 90); safe zone center: (-650, 450, 120). The zone is away from spawn; the existing doorway remains available.
- Blueprint development cues use timed debug spheres: yellow shot, green target, red obstacle. They survive projectile destruction but are development presentation only.

### Acceptance and continuation

- Native review found an asymmetric safe-area possession registration; fixed and verified with an actual-volume regression. Final authored-fixture checks also verified custom wall collision survives map restart; mesh-default collision inheritance is disabled on the Stage 4 wall.
- Manual route acceptance: the user confirmed on 2026-11-08 that the manual check passed. The earlier agent-side UI limitations remain historical NotRun results; this acceptance is user-reported, not a new automated test run. No individual route measurements were supplied.
- An additional 49-shot movement sequence was present in the runtime log (minimum interval 0.250004 game seconds at rate 4). Its input source was not established; it is runtime cadence evidence only and does not establish user acceptance.
- Automatic lifetime expiry was not observed; configured lifetime was verified. Blueprint cues are development-only debug primitives.
- Final handoff: Editor open on Demo, PIE stopped, no active writers. No commit or push. Existing startup Condition failed diagnostics and CommonUI viewport diagnostic remain outside Stage 4.
- Delegation: bounded combat/tests worker, independent native reviewer, and computer-use playtester; one targeted test correction and one visual-layout correction. One initial compile failure (local Player name shadowing) was corrected; final build succeeded. Requested profile settings were Sol low/high and Terra medium; execution model/token/cost metadata was unavailable.
