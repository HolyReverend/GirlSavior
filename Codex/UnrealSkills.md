# Unreal Engine skill routing

Open this guide only for work that requires inspecting, changing, building, testing, or running the Unreal project. Skip it for game design, documentation, and Codex workflow tasks that do not require such work. For mixed tasks, use it only for the Unreal portion.

- Select the most specific skill for each subsystem involved and read its installed `SKILL.md`. Add another skill only if its scope also matches the work. Follow related-skill links only when the task calls for them.
- Use the [current architecture](../Docs/Technical/Architecture.md) when a skill asks for project context. Reuse it if already read. When `ue-project-context` refreshes project facts, update that architecture file.

## Quod Soler skills

| Unreal task | Skill |
| --- | --- |
| Define reflected C++ types, manage UObject references, or use UE containers and delegates | `ue-cpp-foundations` |
| Set up actor or component lifecycle, attachment, spawning, or ticking | `ue-actor-component-architecture` |
| Change modules, targets, dependencies, or plugin descriptors, or diagnose build and linker errors | `ue-module-build-system` |
| Scan or refresh the recorded engine version, modules, plugins, and other project facts | `ue-project-context` |
| Work on GameMode, GameState, controllers, possession, spawning, or match flow | `ue-gameplay-framework` |
| Bind Enhanced Input actions, mapping contexts, triggers, or modifiers | `ue-input-system` |
| Change jumping, crouching, floor checks, movement modes, or prediction in CharacterMovementComponent | `ue-character-movement` |
| Work on AnimInstance, montages, blend spaces, animation state machines, or notifies | `ue-animation-system` |
| Implement GAS abilities, effects, attributes, or cues | `ue-gameplay-abilities` |
| Build or activate Game Feature plugins and their gameplay extensions | `ue-game-features` |
| Set up RPCs, property replication, relevancy, dormancy, or Iris | `ue-networking-replication` |
| Define or load Data Assets, Data Tables, curves, or soft asset references | `ue-data-assets-tables` |
| Save or load player progress, manage slots, or version serialized data | `ue-serialization-savegames` |
| Configure collision channels, traces, sweeps, overlaps, or Chaos rigid bodies | `ue-physics-collision` |
| Load or unload maps, level instances, or World Partition Data Layers | `ue-world-level-streaming` |
| Generate content with PCG, runtime meshes, or seeded placement | `ue-procedural-generation` |
| Implement AI controllers, Behavior Trees, perception, NavMesh, EQS, or Smart Objects | `ue-ai-navigation` |
| Implement StateTree tasks, conditions, transitions, or events | `ue-state-trees` |
| Build Mass Entity processors, queries, fragments, or crowd spawning | `ue-mass-entity` |
| Implement UMG, Slate, Common UI, or MVVM widgets and screens | `ue-ui-umg-slate` |
| Control material instances, post-processing, scene captures, or render targets from C++ | `ue-materials-rendering` |
| Spawn Niagara systems, set effect parameters, or pass data to Niagara from gameplay | `ue-niagara-effects` |
| Play or mix sound effects, music, dialogue, ambience, or MetaSounds | `ue-audio-system` |
| Play or bind Level Sequences, set up cinematic shots, or render them | `ue-sequencer-cinematics` |
| Extend the editor with detail panels, menus, modes, or utility widgets | `ue-editor-tools` |
| Write UE tests, add logs or debug drawing, or capture profiling data | `ue-testing-debugging` |
| Run work off the game thread, dispatch results back, or schedule timers and tickers | `ue-async-threading` |

Source for this section. [Quod Soler skill catalog](https://github.com/quodsoler/unreal-engine-skills/tree/main#available-skills)

## Other installed Unreal skills

| Unreal task | Skill |
| --- | --- |
| Expose C++ APIs to Blueprint or call Blueprint events from C++ | `ue-blueprint-cpp-interop` |
| Build gameplay cameras, spring arms, view-target blends, or camera shakes | `ue-gameplay-cameras` |
| Define or query Gameplay Tags or wire gameplay messages | `ue-gameplay-tags-messaging` |
| Work on movement modes, layered moves, or rollback with the Mover plugin | `ue-mover` |
| Design module boundaries, dependency direction, or Public/Private APIs | `ue5-architecture` |
| Reproduce a project issue, check assets or logs, or narrow a regression | `ue5-debug-validation` |
| Check PIE performance, packaging configuration, or release readiness | `ue5-performance-packaging` |
| Inspect or change the open GirlSavior editor through MCP, or run editor actions | Use the project-specific [PinWright guide](PinWright.md). |
| Create or edit an Agent Skill registered with Unreal's MCP server | `unreal-engine-skills-for-codex:unreal-skill` |
