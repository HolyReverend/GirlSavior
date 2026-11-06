# Prototype 1 Implementation Plan

**Goal.** In one playable Unreal level, move, aim, shoot, and use a defensive dash under the fixed-angle gameplay camera. Stage 0 establishes a reproducible Editor and PIE baseline. Stages 1–5 each add one mechanic.

**Baseline.** UE 5.8.4. The existing `GirlSavior` runtime module already includes Enhanced Input. The blank project contains `Content/Maps/Demo.umap`, and the project default map points to the engine's Open World template. Stage 0 designates the existing `Content/Maps/Demo.umap` as the startup map and the shared working level for the prototype.

**Design scope.** The five linked mechanics documents define the gameplay. Two greybox rooms, a doorway, a safe-mode area, a target, and authored damage and warning fixtures are enough to exercise those rules. Full expedition flow, enemy AI, shelter progression, dialogue content, and an attack catalog belong to later plans.

## Shared architecture

| Owner | Responsibility and interface |
| --- | --- |
| `AGirlSaviorPrototypeGameMode` and `AGirlSaviorPlayerController` | The map's GameMode selects the pawn and controller. The controller owns the local Enhanced Input mapping context, visible absolute cursor in viewport pixels, and gameplay input availability. Keep the expedition/shelter combat permission separate from temporary loss of control. Dispatch cancellation on pause, focus loss, unpossession, or other control handoff. |
| `AGirlSaviorCharacter` and `UGirlSaviorCharacterMovementComponent` | The character composes the gameplay components and provides a fixed body reference. The movement component owns capsule movement, floor and collision handling, and later the dash mode. Movement reads the camera's horizontal basis and ignores the character's aiming yaw. |
| `AGirlSaviorPlayerCameraManager` and room bounds actors | The camera manager owns the perspective view, the fixed heading, and the follow and transition state. Room actors provide editable framing bounds and transition regions. Camera framing reads pawn position, independent of the pawn's yaw. |
| `UGirlSaviorAimComponent` and `UGirlSaviorFireComponent` | Aim owns the last valid horizontal facing. Fire owns the held-button series and the next eligible shot time. Fire spawns the test projectile from the character using the current aim. The projectile handles travel and impact. |
| Damage receiver and presentation | The receiver applies typed damage and reads dash protection. Native gameplay exposes read-only state and events for Blueprint assets, UMG, and effects. Presentation uses those events for the dash outline, readiness, and shot and impact cues. |

- Use a gameplay mouse capture mode that preserves absolute cursor input and delivers the first LMB press. Keep these responsibilities in feature folders within `Source/GirlSavior/`. Keep the existing module and dependencies, adding a private `UMG` dependency when native widget creation is needed. Keep internal types private to the module, use Unreal reflection where needed for assets and Blueprint access, and use `UPROPERTY` references for owned UObject assets.
- Use Blueprint class defaults for single-variant tuning, including the projectile class and fire rate. Use instance properties for room geometry. Validate required references and positive timing values where consumed. Use centimeters and seconds in native gameplay. Bind input and gameplay events on possession or component initialization as appropriate, and remove bindings or clear held state on teardown.
- Evaluate local input before character movement. Update the final camera view after movement, then update aim and launch eligible shots in `TG_PostUpdateWork` with an explicit aim-before-fire dependency. A press is consumed on that frame when eligible. Keep repeats and dash deadlines on game time. Loss of control clears pending actions before gameplay resumes. [Epic Actor Ticking](https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine)

## Stages

### 0. Project preparation

[Detailed Stage 0 plan and audit](Stage00-ProjectPreparation.md)

- **Prepare.** Verify the existing `GirlSaviorEditor` build and Enhanced Input setup. Prepare the existing `Demo` level with a flat floor, basic lighting, and one `PlayerStart`. Set it as both the Editor Startup Map and Game Default Map, and build the prototype in that level.
- **Accept in PIE.** Reopen the project and run one local session from the prototype map with the stock pawn. Confirm the map opens, the player spawns and is possessed, and PIE exits cleanly.

### 1. Character movement

[Detailed Stage 1 plan](Stage01-CharacterMovement.md) · [Movement design](../../GameDesign/Mechanics/CharacterMovement.md)

- **Build.** Use the Stage 0 map to set up the prototype GameMode, pawn, camera manager with a rigid follow view at the Stage 2 heading, and the greybox movement lane. Author `IA_Move` and `IMC_Gameplay` in Enhanced Input. Set accumulation to `Cumulative` for signed WASD mappings, clamp diagonal magnitude, and transform by the fixed camera basis. Keep capsule sweeps, wall sliding, and walkable-floor handling in Character Movement. Tune grounded speed to **500 cm/s**, full acceleration and reversal to **0.10 s** or less, and stopping from full speed to **0.08 s**. [Epic Enhanced Input accumulation](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/EnhancedInput/EInputActionAccumulationBehavior)
- **Accept in PIE.** Use one lane with a wall to compare straight and diagonal speed, confirm opposing keys cancel by axis, measure start/reverse/stop timing, and check that an oblique approach slides while a frontal stop still allows immediate movement away.

### 2. Gameplay camera

[Detailed Stage 2 plan](Stage02-GameplayCamera.md) · [Camera design](../../GameDesign/Mechanics/GameCamera.md)

- **Build.** Extend the Stage 1 view to a fixed narrow-FOV perspective, starting at **35° down** and **45° yaw**, with shared FOV and distance. Follow with **0.15–0.25 s** settling and limit screen lag using an initial **20% margin** on each side. Blend authored room constraints through doorways, prioritize the hero, nearby warnings, and exits in small rooms, and snap on spawn or reposition. Keep aim offset disabled. Use a camera-only occlusion query to fade marked foreground meshes that hide the hero or a nearby danger zone while preserving obstacle boundaries. Place a warning fixture where it stays readable before impact, including when its source is out of frame.
- **Accept in PIE.** At a narrow aspect ratio, run and reverse through a small room and doorway, then reposition the pawn. Check stable angle and scale, bounded lag without overshoot, smooth handoff and immediate snap, and visibility of the hero and warning through a fading occluder. The dash-speed check belongs to Stage 5.

### 3. Character aiming

[Detailed Stage 3 plan](Stage03-CharacterAiming.md) · [Aim design](../../GameDesign/Mechanics/CharacterAIM.md)

- **Build.** Project a stable body point and deproject the cursor using the same current view and constrained game-view rectangle. Intersect the ray with the horizontal plane through that point. Apply yaw only, with screen-distance hysteresis that enters hold at **≤1%** of view height and exits at **>1.5%**. Use spawn yaw until the first valid direction. Preserve the last yaw when the cursor leaves the view, projection fails, or gameplay input is unavailable. Expose cursor-in-view separately from the stored direction. [Epic projection data API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FSceneViewProjectionData)
- **Accept in PIE.** Move the hero and camera beneath a stationary cursor across the body and a wall or floor gap. Verify independent movement and yaw, both hold thresholds, and preservation and recovery when the cursor leaves and re-enters the view.

### 4. Character shooting

[Detailed Stage 4 plan](Stage04-CharacterShooting.md) · [Shooting design](../../GameDesign/Mechanics/CharacterShootSystem.md)

- **Build.** Route the configured LMB press and release edges from the player controller. Fire immediately when ready, repeat while held, and drop clicks that end before readiness. Set the next deadline from the actual shot and preserve it through release and rate changes. Pause freezes the remaining game-time interval. Allow at most one shot on recovery from a hitch. After cursor exit or control loss, require a fresh press. Use the safe-mode area to exercise shelter's disabled firing rule. Keep the series through a doorway when control is retained. Spawn one test actor with `UProjectileMovementComponent` from a fixed point inside the character capsule. Launch along the current horizontal aim, ignore the shooter, and stop on the first blocking hit. Keep the rate tunable in shots/s. Publish distinct shot, target hit, and obstacle hit cues. Pass direct projectile hits to the receiver that Stage 5 extends for protection.
- **Accept in PIE.** In one firing lane, compare press, hold, and early taps using shot timestamps. Leave the frame with fire held and confirm that re-entry requires a new press. Verify that the projectile travels from the character along the current aim and stops at a target or wall. A blocked shot consumes the normal interval and gives obstacle feedback. Use the same timestamps to confirm that resuming from a pause or hitch does not replay missed shots.

### 5. Defensive dash

[Detailed Stage 5 plan](Stage05-DefensiveDash.md) · [Dash design](../../GameDesign/Mechanics/CharacterCharge.md)

- **Build.** Route a fresh Left Shift press through the existing controller input path. Read current WASD intent after input evaluation and lock the initial dash direction. Admit only from permitted grounded gameplay with a nonzero direction and at least **30 cm** of available swept path, accounting for a valid slide. Use a grounded custom Character Movement mode with capsule sweeps and shared walkable-floor rules. Travel **300 cm in 0.22 s** on level open ground. Slide along oblique walls for the remaining time. End on frontal or blocking enemy contact, a blocked corner, unsupported path, or control loss. Clear dash velocity and resume normal movement from current input.
- **Build.** Start the **1 s** cooldown on successful admission. Expose protection for the first **0.12 s**, ending sooner on cancellation. At damage resolution, query the current deadline and filter direct projectile and melee hits during that window. Area and floor hazards retain their own behavior. Drive the outline and readiness indicator from the same state, and keep camera, aim, and the current firing series live during a dash.
- **Accept in PIE.** Dash in the existing lane on open floor, against a frontal blocker, and along an angled wall. Verify range, rejection with insufficient clearance, collision interruption after launch, cooldown from the original start, and fresh-press behavior. During a dash with fire held, compare direct and area damage, verify the protection cutoff, and check that aim, firing cadence, and camera framing continue.

## Tuning decisions for the stage documents

Choose camera FOV and distance in Stage 2 and a starting fire rate in Stage 4. The greybox map, the native class split, and the stage order are technical choices for this plan. Use the Stage 0 smoke check and the five mechanic-specific PIE scenarios above as the acceptance baseline. Specify detailed assets, signatures, and any focused automation in each stage document where it is needed to verify that mechanic.
