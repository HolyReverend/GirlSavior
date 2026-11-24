# Project documentation

Use this index when you need to find a document. Start with the file and section for the task. Follow a prerequisite or source link when you need a specific dependency, rule, or piece of evidence.

| Need | Read |
| --- | --- |
| Overall game concept | [Concept](GameDesign/ConceptDesign.md) |
| A specific gameplay mechanic | Choose it in the [game design index](GameDesign/GameDesign.md). |
| Current implementation status of gameplay mechanics | [Mechanics checklist](GameDesign/MechanicsChecklist.md) |
| Open decisions, risks, and checks by topic | [Open-item index](OpenItems/Index.md) |
| Current code, asset wiring, and shared integration contracts | [Architecture](Technical/Architecture.md) |
| Direct Unreal implementation work | [Implementation rules](Technical/ImplementationRules.md) |
| Prototype scope, stage order, or planned responsibilities | [Prototype 1 plan](Technical/Prototype%231/ImplementationPlan.md) |
| Playable combat route with ordinary enemies and waves | [Combat prototype plan](Technical/CombatPrototype/ImplementationPlan.md) |
| Walkable Camp, two-of-three skill preparation and the linked combat route | [Camp prototype plan](Technical/CampPrototype/ImplementationPlan.md) |
| Three playable weapons, slot selection at the Expedition Table and in-combat switching | [Weapon system plan](Technical/Weapons/ImplementationPlan.md) |

## Prototype 1 technical tasks

| Task | Detailed document |
| --- | --- |
| Reproduce initial project preparation | [Stage 0](Technical/Prototype%231/Stage00-ProjectPreparation.md) |
| Movement and walking response | [Stage 1](Technical/Prototype%231/Stage01-CharacterMovement.md) |
| Camera framing, room bounds, and foreground fading | [Stage 2](Technical/Prototype%231/Stage02-GameplayCamera.md) |
| Mouse aiming and cursor/input contract | [Stage 3](Technical/Prototype%231/Stage03-CharacterAiming.md) |
| Implement shooting | [Stage 4](Technical/Prototype%231/Stage04-CharacterShooting.md) |
| Implement defensive dash | [Stage 5](Technical/Prototype%231/Stage05-DefensiveDash.md) |

## Combat prototype technical tasks

| Task | Detailed document |
| --- | --- |
| Set up the first fight, connect the systems, and accept the playable route | [Combat prototype implementation plan](Technical/CombatPrototype/ImplementationPlan.md) |
| Resolve hits, protection, health, barrier and death | [Damage system](Technical/CombatPrototype/DamageSystem.md) |
| Implement the Pursuer, Shooter and Charger | [Ordinary enemies](Technical/CombatPrototype/BasicEnemies.md) |
| Author entrances and handle reinforcement pressure | [Enemy waves](Technical/CombatPrototype/EnemyWaves.md) |
| Wire warnings, impacts, HUD feedback and the result overlay | [Combat feedback](Technical/CombatPrototype/CombatFeedback.md) |

## Camp prototype technical tasks

| Task | Detailed document |
| --- | --- |
| Build the walkable blockout and the preparation flow at the table | [Camp scene and preparation](Technical/CampPrototype/CampScene.md) |
| Author heroines, skill cards and configurable mechanic definitions | [Heroine and support skill data](Technical/CampPrototype/SupportSkills.md) |
| Transfer the confirmed skills, apply them in combat, and return to Camp | [Camp to expedition handoff](Technical/CampPrototype/ExpeditionHandoff.md) |

## Weapon system technical tasks

| Task | Detailed document |
| --- | --- |
| Define firearm classes, lifetimes and integration boundaries | [Weapon architecture](Technical/Weapons/WeaponArchitecture.md) |
| Author shared weapon data, fire three distinct patterns, and switch slots | [Weapon data and combat](Technical/Weapons/WeaponDataAndCombat.md) |
| Edit and save slots at the table and carry a fixed set into the expedition | [Weapon selection and travel](Technical/Weapons/WeaponLoadout.md) |
| Show distinct shots, resolved impacts and the active weapon | [Weapon feedback and HUD](Technical/Weapons/WeaponFeedback.md) |

`GameDesign/` owns concepts, mechanics, and game rules. `Technical/` owns roadmaps, implementation stages, technical architecture, and implementation details. Open the other branch only for a specific design decision or technical constraint the task needs.

Completed-stage checklists record their original delivery. Consult current architecture for shared contracts. Read dated execution and acceptance sections only when you need prior evidence; execute only the checks required for the current task.
