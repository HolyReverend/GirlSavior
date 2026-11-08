# Technical implementation rules

Read this file only for direct work on Unreal code, assets, configuration, builds, tests, or the editor.

## Project approach

- Use the engine version recorded in the current architecture. The task-specific guides below define C++/Blueprint responsibilities and data authoring.
- Before Unreal work, read the [current architecture](Architecture.md) and [Unreal skill routing](../../Codex/UnrealSkills.md) once. Use already loaded context when a skill asks for it. Open only the relevant stage sections or guides. A prerequisite link does not require reading or replaying completed stages.

| Work | Read when needed |
| --- | --- |
| Gameplay C++ or Blueprint integration | [C++ and Blueprints](../../Codex/Cpp.md) |
| Gameplay data, tuning, or content variants | [Data-driven design](../../Codex/DataDriven.md) |
| New folders, files, or assets inside `GirlSavior/` | [Project layout and naming](../../Codex/ProjectLayout.md) |

## Architecture maintenance

After each Unreal task, review [Architecture.md](Architecture.md) against the completed work. Update affected file responsibilities, relationships, assets, configuration, and integration contracts. Describe the verified current implementation and keep it concise. Edit the file when its contents need to change.

## Comments

- Comment C++ functions and meaningful code blocks in clear, simple English. Explain their purpose, non-obvious behavior, and constraints. Keep comments consistent with the implementation.
- For every parameter of a new Data Asset, write a Russian comment explaining what the parameter controls and how it affects behavior. Cover nested parameters as well. Put the comment on the corresponding C++ property or in the Blueprint variable description, depending on where the parameter is defined. These parameter comments use Russian even when the Data Asset class is written in C++.

## Manual PIE checks

When a manual PIE check requires the user, give the steps and expected result. Ask whether it passed, failed with details, or is deferred. Keep dependent acceptance pending until the user answers.
