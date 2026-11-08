# Current project architecture

Verified on 2026-11-08 against source, configuration, and saved editor assets. Unreal Engine 5.8.4. Current runtime systems cover movement, camera framing, foreground fading, and mouse aiming.

## Entry points and assets

`Config/DefaultEngine.ini` opens `/Game/Maps/Demo` for both editor and game. The map overrides the GameMode.

```text
Demo → BP_PrototypeGameMode
       ├─ BP_PrototypeCharacter → custom movement + AimComponent
       └─ BP_PrototypePlayerController
          ├─ IMC_Gameplay → IA_Move
          └─ BP_PrototypeCameraManager → DA_CameraManager
```

Blueprints under `Content/Blueprints/Core/` and `Characters/` configure the matching native classes. Input lives in `Content/Input/`. `Content/Data/DataAssets/DA_CameraManager` is a `UGirlSaviorCameraSettings` asset holding shared camera tuning. `Demo` also places native Room Bounds, a foreground occluder, and a warning fixture.

## Native code map

Paths below are relative to `GirlSavior/Source/GirlSavior/`. Entries ending in `.*` cover the matching header and implementation.

| Files | Responsibility |
| --- | --- |
| [GirlSavior.cpp](../../GirlSavior/Source/GirlSavior/GirlSavior.cpp), [GirlSavior.Build.cs](../../GirlSavior/Source/GirlSavior/GirlSavior.Build.cs) | Register and build the single Runtime module. Public dependencies are Core, CoreUObject, Engine, InputCore, EnhancedInput. Private GeometryCore and GeometryAlgorithms support room geometry. |
| [Core/GirlSaviorPrototypeGameMode.*](../../GirlSavior/Source/GirlSavior/Core/GirlSaviorPrototypeGameMode.cpp) | Select the character and controller defaults. |
| [Core/GirlSaviorPlayerController.*](../../GirlSavior/Source/GirlSavior/Core/GirlSaviorPlayerController.cpp) | Own Enhanced Input registration, camera-relative movement intent, cursor/input mode, and the shared gameplay-input gate. |
| [Characters/GirlSaviorCharacter.*](../../GirlSavior/Source/GirlSavior/Characters/GirlSaviorCharacter.cpp) | Install the custom movement component and own the aim component. |
| [Characters/GirlSaviorCharacterMovementComponent.*](../../GirlSavior/Source/GirlSavior/Characters/GirlSaviorCharacterMovementComponent.cpp) | Apply planar walking acceleration/stopping response and preserve collision handling through CharacterMovementComponent. |
| [Characters/GirlSaviorAimComponent.*](../../GirlSavior/Source/GirlSavior/Characters/GirlSaviorAimComponent.cpp) | Project the cursor onto the body-height plane, hold facing near the body, and own character yaw. |
| [Camera/GirlSaviorPlayerCameraManager.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorPlayerCameraManager.cpp) | Resolve the follow pivot, smooth tracking, enforce screen margins, and fade foreground occluders. |
| [Camera/GirlSaviorCameraSettings.h](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorCameraSettings.h) | Define and validate the camera Data Asset parameters. |
| [Camera/GirlSaviorCameraRoomBounds.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorCameraRoomBounds.cpp), [GirlSaviorCameraRoomFraming.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorCameraRoomFraming.cpp) | Author noncolliding room boxes and compute connected floor regions, gap bridges, and inset-safe pivots. |
| [Camera/GirlSaviorCameraChannels.h](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorCameraChannels.h), [GirlSaviorPrototypeForegroundOccluder.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorPrototypeForegroundOccluder.cpp) | Define the occlusion channel and separate persistent blocking geometry from the fadeable upper mesh. |
| [Camera/GirlSaviorPrototypeWarningFixture.*](../../GirlSavior/Source/GirlSavior/Camera/GirlSaviorPrototypeWarningFixture.cpp) | Provide a timed warning/impact fixture for camera visibility checks. |
| [Tests/](../../GirlSavior/Source/GirlSavior/Tests/) | Movement, camera, and aiming console probes plus room-framing Automation tests. |

## Integration contracts

- `CanUseGameplayInput()` is the live gameplay gate. Full suspension uses `SetGameplayInputAvailable(false)`. Movement-only locks leave aiming available. Mouse input uses GameAndUI with CaptureDuringMouseDown, a visible cursor, and DoNotLock.
- Aim ticks in `TG_PostUpdateWork` after camera evaluation and uses absolute viewport pixels with the constrained view rectangle. Consumers use `GetAimDirection()`. Firing must recheck the gameplay gate, check cursor inclusion, and order its tick after aim.
- Camera tuning belongs in `DA_CameraManager`. After same-pawn teleports, call `RequestFollowSnap()`.
- Room framing uses room footprint geometry and `WallInset`. Side gaps within the inset can join across overlapping sides. Room authoring supports yaw rotation and positive scale with world-up orientation.
- `CameraOcclusion` is `ECC_GameTraceChannel1`, default Ignore, configured in `Config/DefaultEngine.ini`. Fadeable components overlap it, carry `CameraFade`, and use custom primitive data slot 0 in the prototype fade material. Gameplay blocking remains on separate geometry.

## Working requirements

- `GirlSavior.Target.cs` builds Game; `GirlSaviorEditor.Target.cs` builds Editor. The descriptor enables GeometryProcessing for runtime room geometry and PinWright for Editor. Read the [PinWright guide](../../Codex/PinWright.md) for live editor work and confirm project identity first.
- Use the [documentation index](../README.md) when you need to locate a feature's detailed stage or design document. Match checks to the affected system and record what was actually verified.
