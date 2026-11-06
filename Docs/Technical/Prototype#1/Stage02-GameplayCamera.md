# Stage 2 — Gameplay Camera Implementation Plan

> During implementation, use the project's `stage-orchestrator` workflow. Use the camera scenario in Step 7 for acceptance.

**Goal.** Keep the hero and the immediate combat space readable in `Demo.umap` while the hero moves, turns, and crosses a doorway. The view keeps one perspective lens and heading across the level.

**Prerequisite.** [Stage 1](Stage01-CharacterMovement.md) delivers `AGirlSaviorPlayerCameraManager`, the screen-relative WASD basis, and a playable greybox in `Content/Maps/Demo.umap`. This stage extends that camera. Stage 3 consumes the resulting frame for aiming, and Stage 5 exercises follow at dash speed.

**Sources.** [Prototype plan](ImplementationPlan.md) · [Camera design](../../GameDesign/Mechanics/GameCamera.md) · [Aim design](../../GameDesign/Mechanics/CharacterAIM.md) · [Project layout](../../../Codex/ProjectLayout.md)

## Step 1 — Keep the camera in the existing gameplay path

- [ ] Extend `AGirlSaviorPlayerCameraManager::UpdateViewTargetInternal` to calculate the prototype pawn's `FMinimalViewInfo`. Use the capsule center for framing, with heading from the same `ViewRotation` that `ToWorldMoveIntent` uses. Keep the normal engine path for other view targets. Carry the same view target across room transitions and use the positional blend in Step 2. [Epic camera update API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerCameraManager/UpdateViewTargetInternal)
- [ ] Expose the camera tuning on the manager as editable class defaults. Create `Content/Blueprints/Core/BP_PrototypeCameraManager.uasset` and set it as the camera manager class in `BP_PrototypePlayerController`. Start with the values below. Tune FOV and distance together in `Demo` and apply the chosen pair to all rooms. Use perspective with `AspectRatio_MaintainXFOV` and an unconstrained viewport aspect ratio. Camera heading comes from the fixed view, and aim offset is zero throughout the prototype. [Epic view settings API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FMinimalViewInfo)

| Setting | Initial value |
| --- | --- |
| View rotation | Pitch **−35°**, yaw **45°**, roll **0°** |
| Horizontal FOV and follow distance | **45°**, **1600 cm**, carried forward as the first tuning pass |
| Follow settling time | **0.20 s** to remove 95% of the remaining horizontal error after stopping. Tune within **0.15–0.25 s**. |
| Hero screen margin | **20%** of width and height from each respective edge |
| Foreground fade | **0.15 s** for a full fade or restore, with **20%** coverage at full fade |
| Nearby warning range | **1000 cm** horizontal radius for occlusion checks |

Native camera code stays in `Source/GirlSavior/Camera/` in the existing runtime module. Keep camera math in C++ and use level actor properties for room and doorway authoring.

## Step 2 — Author rooms and doorway framing

- [ ] Add `AGirlSaviorCameraRoomBounds` in `Camera/GirlSaviorCameraRoomBounds.h/.cpp`. Give it a room membership box and an editable horizontal pivot rectangle in the room's local axes. The membership box identifies which room contains the hero's body center. The pivot rectangle limits the **preferred camera anchor**, with a small rectangle near the room center for a tight room. Use query-free box geometry for the point test. Add an editable list of warning fixture actors for this room.
- [ ] Add `AGirlSaviorCameraDoorway` in `Camera/GirlSaviorCameraDoorway.h/.cpp`. Give it a transition box and references to `RoomA` and `RoomB`. Point its local +X from A to B. The box covers the traversable width and its two ends meet the respective room memberships. Validate positive extents and both distinct room references. Cache the placed actors in the camera manager once per loaded level world and keep weak references. Use yaw-only authoring transforms and a single active room or doorway per sample. Confine room overlaps to the doorway coverage.
- [ ] Each camera update, compute each applicable room's preferred pivot by clamping the hero's position to that room's rectangle in local XY, then restoring the hero's world Z. Give doorway membership priority over room membership. Inside the doorway, calculate `t = Clamp((LocalX + HalfLength) / (2 * HalfLength), 0, 1)`. Blend the two room pivots with `t²(3 − 2t)`. Using position for the blend lets the player reverse in the doorway. Align the room and doorway memberships so their preferred pivots match at the doorway ends. Outside authored bounds, use the hero's position.
- [ ] Add two adjacent greybox rooms in `Demo.umap`, one smaller than the other, linked by one authored doorway. Keep the Stage 1 movement lane usable. Place the small room's exit and warning where the room's framing can keep them readable at a narrower aspect ratio, even when the frame includes space beyond the floor. The hero's screen margin takes priority over the preferred room pivot.

## Step 3 — Smooth follow and cap screen lag

- [ ] Store the last camera anchor. For normal follow, approach the preferred pivot in XY using `Alpha = 1 − exp(−ln(20) * DeltaTime / SettleSeconds)`. Use the current hero height for the anchor's Z. The blend approaches a fixed target without overshoot and removes 95% of the horizontal error in one settle interval. Snap any remaining XY error under **1 cm** to the preferred pivot before applying the screen margin.
- [ ] Build the candidate POV at `Anchor − ViewRotation.Vector() * FollowDistance`. Construct `FSceneViewProjectionData` with the owning local player's raw viewport rectangle, the candidate camera origin, and UE's inverse camera rotation and axis remap from the installed `LocalPlayer.cpp`. Set the raw rectangle with `SetViewRectangle` before deriving the constrained rect. Pass a copy of the candidate POV through `FMinimalViewInfo::CalculateProjectionMatrixGivenView` with the same axis constraint used for rendering. Inside this update, build projection from the candidate POV because the local player's camera cache still contains the prior view. [Epic projection matrix API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FMinimalViewInfo)
- [ ] Project the hero's capsule center into the candidate view. Inset the constrained view rectangle by the configured per-axis margin, giving an initial normalized range of **[0.20, 0.80]** on both axes. Require a finite projection with positive depth and explicitly test the inset rectangle. If the hero falls outside, find the smallest useful correction toward a hero-centered anchor using **eight bisection iterations**. Start with the proposed anchor as the failing endpoint and the validated centered anchor as the passing endpoint. Reuse the rotation, projection matrix, and rect during the probes and vary only camera origin. Use the passing endpoint. If projection data is unavailable or the centered endpoint fails validation, use the centered anchor and report invalid tuning if a usable viewport still cannot frame it. Keep this corrected anchor as the input to the next frame's follow. Apply it after the room blend and smoothing so the hero stays inside the margin even when the camera has to move beyond the preferred room bounds.
- [ ] Set the final anchor and lens on `OutVT.POV`. Keep the gameplay lens and transform free of later camera modifiers that would invalidate the screen-margin calculation.

## Step 4 — Handle spawn, reposition, and current-frame consumers

- [ ] Initialize follow on the first valid hero view, on view target replacement, and on return from a different view target. Add `RequestFollowSnap()` for an explicit same-pawn teleport or scripted return. At the next camera evaluation, resolve the destination room from the new location, build its preferred pivot directly, and apply the screen-margin correction. Update both the current view and stored follow anchor in that evaluation. Call `SetGameCameraCutThisFrame()` for this immediate change. Use the ordinary positional blend for walking through a doorway.
- [ ] For local occlusion, use `UpdateCamera` after `Super::UpdateCamera` has cached the frame. Read `ULocalPlayer::GetProjectionData` from the same game viewport and use its view origin, constrained view rect, and view-projection matrix. For Stage 3, document the consumer order as movement → final camera cache → aim in `TG_PostUpdateWork`. Screen coordinates are absolute viewport pixels, with validity checked against the constrained view rect. The standard follow view is shared by expedition and shelter. A later dialogue presentation can return to the gameplay view and request a snap if the hero was repositioned. [Epic Actor Ticking](https://dev.epicgames.com/documentation/unreal-engine/actor-ticking-in-unreal-engine) · [Epic LocalPlayer API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/ULocalPlayer)

## Step 5 — Fade foreground geometry that hides the hero or warning

- [ ] In **Project Settings → Collision**, add a `CameraOcclusion` trace channel with default response **Ignore**. Define one matching native channel constant. In `Content/Blueprints/Core/`, create `BP_PrototypeForegroundOccluder` with three parts. A gameplay blocker keeps the full obstacle collision. A low opaque mesh or outline shows the obstacle boundary. A tagged `CameraFade` upper mesh is Query Only and responds **Overlap** to `CameraOcclusion`, with other responses set to Ignore on that mesh.
- [ ] Under `Content/Art/Materials/`, create `M_PrototypeCameraFade` for the upper mesh. Read a scalar `OcclusionFade` from Custom Primitive Data index **0**, with zero meaning fully visible. Feed `1 − OcclusionFade` into `DitherTemporalAA` and use its output as the opacity mask. Keep the boundary mesh visible throughout the fade. [Epic Custom Primitive Data](https://dev.epicgames.com/documentation/unreal-engine/storing-custom-data-in-unreal-engine-materials-per-primitive)
- [ ] In the camera's post-cache update, gather the body center and active warning centers from the resolved room, or from both adjacent rooms in a doorway. Use only warnings within the configured nearby radius. For each on-screen point, run `LineTraceMultiByChannel` from the final view origin to the point and collect tagged hit components in one set. Ignore the hero and warning actors as query sources of obstruction. Process the hit array even when the return value is false, since overlap-only hits do not produce a blocking result. [Epic multi-trace behavior](https://dev.epicgames.com/documentation/unreal-engine/using-a-multi-line-trace-raycast-by-channel-in-unreal-engine)
- [ ] Track affected components by weak reference. Move their fade amount toward one while obstructing a sample and back to zero when clear. Advance the progress by `DeltaTime / FadeSeconds`, clamped to the target. Write `FadeProgress * (1 − MinimumCoverage)` through `SetCustomPrimitiveDataFloat(0, Value)`. Restore faded components on camera cut, view target exit, or `EndPlay`, and prune destroyed entries. After a cut, evaluate the new view and apply its current fade set immediately. Collision remains controlled by the separate blocker. [Epic primitive data setter](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UPrimitiveComponent/SetCustomPrimitiveDataFloat)

## Step 6 — Place a readable warning fixture

- [ ] Create `BP_PrototypeWarningFixture` in `Content/Blueprints/Core/`. Use a clearly visible floor marker and a timed cosmetic impact cue, with **0.8 s** of warning as the first tuning value. Use the fixture actor's hidden state to signal when its marker is active. Set its origin at the marker center and assign it to the room's warning list.
- [ ] In `Demo`, arrange a trigger near the doorway and a warning position inside the reachable play area. Keep the simulated source out of frame from the test approach. Place the tagged foreground obstacle so that one section of the approach exercises fade for the hero and another can exercise fade for the active floor warning. The fixture provides timing and visibility evidence for the camera stage.

## Step 7 — Build and run one camera acceptance scenario

- [ ] Build `GirlSaviorEditor Win64 Development` using the existing prototype build command. In PIE, use `Demo.umap` and compare the first framing pass at **16:9** and **4:3**. Run the following route once at **4:3**. Use `ShowDebug Camera` and temporary viewport coordinates or traces if measurements are needed.

| Route segment | Expected result |
| --- | --- |
| Open room | Walk, reverse, and stop. Heading, FOV, and distance stay fixed. The capsule center stays within the 20% inset and the view settles without overshoot within 0.15–0.25 s after the pawn stops. |
| Small room and doorway | Cross the doorway and reverse partway through it. Framing stays continuous in both directions. The hero, exit, and active warning stay readable on the planned approach. |
| Foreground blocker and warning | Trigger the warning with its source offscreen. The upper obstacle fades when it hides the hero or warning and restores as the line clears. The boundary stays visible and the obstacle still blocks walking. The warning appears before the cosmetic impact. |
| Explicit reposition | Reposition the same pawn to the other room and call `RequestFollowSnap()`. The first updated view frames the destination and reflects the new occlusion state. |

- [ ] Record the selected FOV and distance, build result, and observations from the route in the Stage 2 Project work report. Carry the final viewport and tick order to Stage 3. Verify the camera at dash speed in Stage 5.
