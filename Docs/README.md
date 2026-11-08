# Project documentation

Use this index to locate a document. Open only the file or section needed for the task. Prerequisite and source links identify dependencies; read them only when their contract is needed.

| Need | Read |
| --- | --- |
| Overall game concept | [Concept](GameDesign/ConceptDesign.md) |
| A specific gameplay mechanic | Choose it in the [game design index](GameDesign/GameDesign.md). |
| Current code, asset wiring, and shared integration contracts | [Architecture](Technical/Architecture.md) |
| Direct Unreal implementation work | [Implementation rules](Technical/ImplementationRules.md) |
| Prototype scope, stage order, or planned responsibilities | [Prototype 1 plan](Technical/Prototype%231/ImplementationPlan.md) |

## Prototype 1 technical tasks

| Task | Detailed document |
| --- | --- |
| Reproduce initial project preparation | [Stage 0](Technical/Prototype%231/Stage00-ProjectPreparation.md) |
| Movement and walking response | [Stage 1](Technical/Prototype%231/Stage01-CharacterMovement.md) |
| Camera framing, room bounds, and foreground fading | [Stage 2](Technical/Prototype%231/Stage02-GameplayCamera.md) |
| Mouse aiming and cursor/input contract | [Stage 3](Technical/Prototype%231/Stage03-CharacterAiming.md) |
| Implement shooting | [Stage 4](Technical/Prototype%231/Stage04-CharacterShooting.md) |
| Implement defensive dash | [Stage 5](Technical/Prototype%231/Stage05-DefensiveDash.md) |

`GameDesign/` owns concepts, mechanics, and game rules. `Technical/` owns roadmaps, implementation stages, technical architecture, and implementation details. Open the other branch only for a specific design decision or technical constraint the task needs.

Completed-stage checklists record their original delivery. Consult current architecture for shared contracts. Read dated execution and acceptance sections only when you need prior evidence; execute only the checks required for the current task.
