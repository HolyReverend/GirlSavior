# Stage 0 — Project Preparation

Completed-stage checklist. Use [current architecture](../Architecture.md) for shared contracts. Read dated audit/execution sections only for historical evidence; a recorded pass is not a current test result.

> During implementation, use the project's `stage-orchestrator` workflow. This stage ends with a buildable Editor target and a repeatable PIE entry point for [Stage 1](Stage01-CharacterMovement.md).

**Goal.** Prepare the existing C++ blank project for the five mechanics in the [prototype plan](ImplementationPlan.md). Use the existing runtime module and designate `Content/Maps/Demo.umap` as the starting level in which all five mechanics will be built and tested.

## Audit on 2026-11-06

| Area | Observed state | Stage 0 decision |
| --- | --- | --- |
| Engine and targets | Launcher UE **5.8.4** is installed. `EngineAssociation` is `5.8`. The `GirlSavior` runtime module has its primary module entry point, plus `GirlSavior` (Game) and `GirlSaviorEditor` (Editor) targets. Project source contains the template module and target files. | Build the existing Editor target. Stage 1 will add the first gameplay classes. [Current architecture](../Architecture.md) |
| Toolchain | UBT found Win64 SDK **10.0.26100.0** and MSVC **14.51.36260**. The Editor target check returned **Succeeded**, **Target is up to date**, with **0 actions**. UBT warned that this compiler is newer than its preferred **14.50.35717**. | Use the installed toolchain for the baseline. A fresh compile of Stage 1 classes will exercise the compiler. Record the actual version and build result if the environment changes. |
| Input | `EnhancedInput` is already in `GirlSavior.Build.cs`. Its engine plugin is enabled by default and appears in the previous Editor log. `DefaultInput.ini` selects `EnhancedPlayerInput` and `EnhancedInputComponent` and enables key flushing on viewport focus loss. There are no project input assets yet. | Use the existing Enhanced Input setup. Stage 1 owns `IA_Move`, `IMC_Gameplay`, possession, and the controller's gameplay input mode. |
| Content and maps | The only project content package is `Content/Maps/Demo.umap`. The 2026-11-05 Editor log shows it entered PIE using `GameModeBase`. `GameDefaultMap` currently points to `/Engine/Maps/Templates/OpenWorld`. `EditorStartupMap` has no project override. | Use `Demo.umap` as the working prototype level. Set both startup map fields to `Demo`. Use the stock game framework for the Stage 0 smoke check. |
| Editor tooling | `ModelingToolsEditorMode` and `PinWright` are enabled for the Editor target. | Keep the existing tooling. For live asset edits through PinWright, verify the Editor project identity before editing. [PinWright guide](../../../Codex/PinWright.md) |

The audit used project files, installed engine metadata, the previous Editor log, and a current UBT target check. Preparing `Demo` as the startup level and running its PIE smoke check belong to Stage 0 implementation.

## Execution on 2026-11-07

- **Scope and authorization:** implement Stage 0 at the user's request. Reuse `Demo`, keep the stock framework, and leave Stages 1–5 for later. No commit or push requested.
- **Baseline:** HEAD reference `main` at `1d06897619cb8f7b68c45ae66c93ec64035298d7`; Git dirty status could not be obtained in the managed shell. Explicit pre-edit SHA-256 snapshots: `Demo.umap` (46,346 bytes) `7BD25F6C757F4480E11A934551927BE102C2BA54025259D5515CE52FE84F574F`; `DefaultEngine.ini` (2,891 bytes) `049F7F50E202F3175D0427F3E0153E1F862E41966FF81C941B50F30C3063F2E9`. Do not infer a clean working tree.
- **Ownership:** primary owns the editor, map, configuration, this checklist, evidence and reports. A bounded-worker (Sol low role) performed a read-only source/configuration audit; no child edits or editor actions. Primary model/effort and token usage are unavailable in execution metadata.
- **Build — Pass:** PinWright `editor_build`, Editor closed; `GirlSaviorEditor Win64 Development`, exit 0, `Result: Succeeded`, target up to date, 0 actions. Build log: `GirlSavior/Saved/PinWright/builds/94c7e64938764ab2b62150dd6a40bd2d/build.log`. Installed compiler 14.51.36260 is newer than UBT's preferred 14.50.35717.
- **Work units — Pass:** build baseline; reuse and prepare the floor; persist both startup maps; restart and run the stock-pawn PIE smoke check. All implementation and editor operations were owned by the primary.
- **Map — Pass:** reused the template floor at location (0, 0, 0), scale (2.4, 2.4, 1), Static mobility and BlockAll collision. Bounds are approximately 2,400 × 2,400 × 50 cm, top Z = 0. A simple downward collision trace at (500, 500) hit Z = 0 with an upward normal and one simple collision shape. Existing lighting and the single PlayerStart at (0, 0, 92) were retained; GameMode Override remains unset. `level.save` returned `saved: true`; the same bounds survived restart.
- **Startup and PIE — Pass:** restarted the visible Editor without a map argument; it loaded `/Game/Maps/Demo.Demo`. Both startup-map settings read back as Demo. One standalone local PIE world used `GameModeBase`, `GameStateBase`, `PlayerController` and `DefaultPawn`. Controller-to-pawn and pawn-to-controller references matched; pawn location was (0, 0, 92), matching Default Player Start. PIE stopped successfully; zero PIE worlds and zero dirty packages remained.
- **Evidence:** `GirlSavior/Saved/Stage0/Acceptance.json`, `GirlSavior/Saved/Stage0/EditorSmokeLog.txt`, the build log above, and `GirlSavior/Saved/Screenshots/OpenLevel/Prototype1Stage0Demo.png` (visually reviewed Editor overview, not the gameplay camera).
- **Limitations:** no project module, map-load or spawn failures were observed. The log is not error-free: CommonUI reports that the stock viewport is not CommonGameViewportClient-derived; PinWright enables optional CommonUI for Editor. Keep the stock framework required by this stage and revisit UI routing when UI is introduced. Two `LogAutomationTest: Error: Condition failed` lines occur before engine initialization; their cause is not established. No automation suite was run. The successful build did not compile new C++.
- **State:** technically verified; ready for Stage 1. Editor remains open on Demo with PIE stopped. No active child writers or pending decisions. No commit or push performed.
- **Cost observations:** build 4.67 s; initial Editor readiness 51.3 s; planned restart readiness 20.4 s; no build retries. Managed-shell filesystem access required a workspace PSDrive workaround. Token usage and total accepted-result cost are unknown; no savings claim.

## Step 1 — Verify the Editor build baseline

- [x] Confirm the `.uproject` opens with UE 5.8.4 and that the `GirlSaviorEditor` target is available. Use the existing `GirlSavior.Build.cs` and both targets. Regenerate IDE project files if the IDE's solution is stale.
- [x] With the project Editor closed, run the following from PowerShell. Acceptance is exit code **0** and UBT `Result: Succeeded`. Record whether UBT compiled anything or found the target up to date.

```powershell
$project = 'E:/Projects/VibeCoding/GirlSaviorProject/GirlSavior/GirlSavior.uproject'
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' GirlSaviorEditor Win64 Development "-Project=$project" -WaitMutex
```

- [x] Not needed in this execution: PinWright built successfully without fallback flags. If the managed shell reports access denied for the UBA session directory, direct `-UBARootDir` to a writable folder under `GirlSavior/Saved/`. UE 5.8.4 still initializes UBA with `-NoUBA`, so the cache path matters. If the managed shell also blocks loading the generated `GirlSaviorModuleRules.dll`, use the authorized desktop build environment. The audit passed there with `-NoUBA` and a workspace-local UBA directory. Treat those flags as per-invocation options.

## Step 2 — Prepare Demo as the shared greybox map

- [x] In Unreal Editor, open `Content/Maps/Demo.umap` (`/Game/Maps/Demo`). Inspect its existing floor, lighting, and spawn point. Reuse suitable actors and save the prototype work in this same level.
- [x] Ensure the level has a flat floor with simple blocking collision and a top surface at **Z = 0**. Use a footprint of about **2400 × 2400 cm** for the later movement lane. If the floor is missing, use an Engine Basic Shapes cube. Keep basic lighting and one `PlayerStart` above the floor with clearance for the stock pawn and the Stage 1 capsule. Add any missing setup actors.
- [x] Save the level. Leave its `GameMode Override` unset for the baseline smoke check. Stage 1 will assign `BP_PrototypeGameMode` on this map and add the collision wall and lane markers. [Epic GameMode setup](https://dev.epicgames.com/documentation/unreal-engine/setting-up-a-game-mode-in-unreal-engine)

## Step 3 — Set the startup maps

- [x] In **Project Settings → Maps & Modes**, set both **Editor Startup Map** and **Game Default Map** to `Demo`. Save the settings. These two fields make the same level the entry point for Editor sessions and game launches. [Epic Maps & Modes settings](https://dev.epicgames.com/documentation/unreal-engine/project-section-of-the-unreal-engine-project-settings)
- [x] Verify that `Config/DefaultEngine.ini` contains both references in the correct section. The expected saved values are:

```ini
[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=/Game/Maps/Demo.Demo
GameDefaultMap=/Game/Maps/Demo.Demo
```

## Step 4 — Accept the baseline and hand it off

- [x] Reopen the project and confirm the Editor loads `Demo`. Start one PIE session with one local player, **Play Standalone** net mode, and **Default Player Start** as the spawn location. Check that the PIE world is `Demo`, the default pawn spawns and is possessed, and the session exits normally. Use the Editor log to check for project module, map load, or spawn errors. This checks the entry point for development. Screen-relative WASD and collision behavior are tested in Stage 1. [Epic Unreal Engine play modes](https://dev.epicgames.com/documentation/unreal-engine/running-unreal-engine)
- [x] Record the UBT result, map package path, both startup-map settings, and the PIE result in the Stage 0 Project work report. The expected project deliverables are `Demo.umap` ready for the prototype and the two map settings in `DefaultEngine.ini`.
- [x] Hand the map to [Stage 1](Stage01-CharacterMovement.md). It will add native player classes, the input assets, the fixed follow camera, and the movement lane in the existing level. Later stages extend the same map with the fixtures they need.
