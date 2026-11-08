# Current project architecture

Verified on 2026-11-08 against source, configuration, and saved editor assets. Unreal Engine 5.8.4. Current runtime systems cover movement, camera framing, foreground fading, mouse aiming, and prototype projectile shooting.

## Entry points and assets

`Config/DefaultEngine.ini` opens `/Game/Maps/Demo` for both editor and game. The map overrides the GameMode.

```text
Demo → BP_PrototypeGameMode
       ├─ BP_PrototypeCharacter → custom movement + AimComponent + FireComponent
       └─ BP_PrototypePlayerController
          ├─ IMC_Gameplay → IA_Move
          └─ BP_PrototypeCameraManager → DA_CameraManager
```

Blueprints under `Content/Blueprints/Core/` and `Characters/` configure the matching native classes. Input lives in `Content/Input/`. `Content/Data/DataAssets/DA_CameraManager` is a `UGirlSaviorCameraSettings` asset holding shared camera tuning. `Demo` also places native Room Bounds, a foreground occluder, a warning fixture, and the Stage 4 target/wall/safe-area lane. `BP_TestProjectile` provides a collision-free sphere visual and Blueprint impact cues; `BP_TestTarget` composes a blocking mesh and damage receiver; `BP_PrototypeSafeArea` adds a floor marker to the native trigger.

## Native code map

Paths below are relative to `GirlSavior/Source/GirlSavior/`. Entries ending in `.*` cover the matching header and implementation.

| Files | Responsibility |
| --- | --- |
| [GirlSavior.cpp](../../GirlSavior/Source/GirlSavior/GirlSavior.cpp), [GirlSavior.Build.cs](../../GirlSavior/Source/GirlSavior/GirlSavior.Build.cs) | Register and build the single Runtime module. Public dependencies are Core, CoreUObject, Engine, InputCore, EnhancedInput. Private GeometryCore and GeometryAlgorithms support room geometry. |
| [Core/GirlSaviorPrototypeGameMode.*](../../GirlSavior/Source/GirlSavior/Core/GirlSaviorPrototypeGameMode.cpp) | Select the character and controller defaults. |
| [Core/GirlSaviorPlayerController.*](../../GirlSavior/Source/GirlSavior/Core/GirlSaviorPlayerController.cpp) | Own Enhanced Input registration, camera-relative movement intent, cursor/input mode, the shared gameplay-input gate, ordered fire-button edges, pause cancellation, and separate combat/safe-area permission. |
| [Characters/GirlSaviorCharacter.*](../../GirlSavior/Source/GirlSavior/Characters/GirlSaviorCharacter.cpp) | Install the custom movement component and own the aim and fire components. |
| [Characters/GirlSaviorCharacterMovementComponent.*](../../GirlSavior/Source/GirlSavior/Characters/GirlSaviorCharacterMovementComponent.cpp) | Apply planar walking acceleration/stopping response and preserve collision handling through CharacterMovementComponent. |
| [Characters/GirlSaviorAimComponent.*](../../GirlSavior/Source/GirlSavior/Characters/GirlSaviorAimComponent.cpp) | Project the cursor onto the body-height plane, hold facing near the body, and own character yaw. |
| [Camera/GirlSaviorPlayerCameraManager.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorPlayerCameraManager.cpp) | Resolve the follow pivot, smooth tracking, enforce screen margins, and fade foreground occluders. |
| [Camera/GirlSaviorCameraSettings.h](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorCameraSettings.h) | Define and validate the camera Data Asset parameters. |
| [Camera/GirlSaviorCameraRoomBounds.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorCameraRoomBounds.cpp), [GirlSaviorCameraRoomFraming.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorCameraRoomFraming.cpp) | Author noncolliding room boxes and compute connected floor regions, gap bridges, and inset-safe pivots. |
| [Camera/GirlSaviorCameraChannels.h](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorCameraChannels.h), [GirlSaviorPrototypeForegroundOccluder.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorPrototypeForegroundOccluder.cpp) | Define the occlusion channel and separate persistent blocking geometry from the fadeable upper mesh. |
| [Camera/GirlSaviorPrototypeWarningFixture.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorPrototypeWarningFixture.cpp) | Provide a timed warning/impact fixture for camera visibility checks. |
| [Tests/](../../GirlSavior/Source/GirlSavior/Tests/) | Movement, camera, aiming, and combat console probes plus room-framing and fire-schedule Automation tests. |
| [Combat/GirlSaviorFireComponent.*](../../GirlSavior/Source/GirlSavior/Combat/GirlSaviorFireComponent.cpp), [GirlSaviorFireSchedule.h](../../GirlSavior/Source/GirlSavior/Combat/GirlSaviorFireSchedule.h) | Preserve ordered short clicks and a game-time deadline; launch at most once per post-aim tick. |
| [Combat/GirlSaviorTestProjectile.*](../../GirlSavior/Source/GirlSavior/Combat/GirlSaviorTestProjectile.cpp), [GirlSaviorDamageReceiverComponent.*](../../GirlSavior/Source/GirlSavior/Combat/GirlSaviorDamageReceiverComponent.cpp) | Sweep a sphere, stop on first blocker, deliver typed DirectProjectile damage, and publish impact/accepted-damage cues. |
| [Combat/GirlSaviorSafeArea.*](../../GirlSavior/Source/GirlSavior/Combat/GirlSaviorSafeArea.cpp) | Track overlapping controlled pawns and cancel firing while any safe area applies. |

## Integration contracts

- `CanUseGameplayInput()` is the live gameplay gate. Full suspension uses `SetGameplayInputAvailable(false)`. Movement-only locks leave aiming available. Mouse input uses GameAndUI with CaptureDuringMouseDown, a visible cursor, and DoNotLock.
- Aim ticks in `TG_PostUpdateWork` after camera evaluation and uses absolute viewport pixels with the constrained view rectangle. Consumers use `GetAimDirection()`. Fire ticks after aim, rechecks the gameplay gate, combat permission, and cursor inclusion, and cancels the series on failure.
- Fire uses one configurable mouse button (default LMB) routed through InputKey edges. A ready tap survives release until the firing tick; cooldown taps do not queue. The double game-time deadline survives cancellation, pause, and rate changes. Successful launches use actual shot time plus 1 / ShotsPerSecond (initially 4). New presses are required after cancellation.
- Combat permission is independent of movement: SetFireAllowed controls the explicit gate and SetSafeAreaActive tracks overlapping zones. Possession inside a zone synchronizes both registries. Stage 5 extends TryApplyDamage with protection before OnDamageAccepted; impact classification depends on receiver presence.
- PrototypeProjectile is ECC_GameTraceChannel2, an object channel with default Ignore. The swept projectile sphere blocks WorldStatic/WorldDynamic; lane walking blockers explicitly block the projectile channel. Pawn and other projectiles ignore it. Blueprint prototype cues use development debug spheres (yellow shot, green target, red obstacle) that outlive projectile destruction.
- Camera tuning belongs in `DA_CameraManager`. After same-pawn teleports, call `RequestFollowSnap()`.
- Room framing uses room footprint geometry and `WallInset`. Side gaps within the inset can join across overlapping sides. Room authoring supports yaw rotation and positive scale with world-up orientation.
- `CameraOcclusion` is `ECC_GameTraceChannel1`, default Ignore, configured in `Config/DefaultEngine.ini`. Fadeable components overlap it, carry `CameraFade`, and use custom primitive data slot 0 in the prototype fade material. Gameplay blocking remains on separate geometry.

## Working requirements

- `GirlSavior.Target.cs` builds Game; `GirlSaviorEditor.Target.cs` builds Editor. The descriptor enables GeometryProcessing for runtime room geometry and PinWright for Editor. Read the [PinWright guide](../../Codex/PinWright.md) for live editor work and confirm project identity first.
- Use the [documentation index](../README.md) when you need to locate a feature's detailed stage or design document. Match checks to the affected system and record what was actually verified.
