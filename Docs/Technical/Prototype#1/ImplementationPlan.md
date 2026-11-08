# Prototype 1 implementation plan

**Goal.** In one playable level, move, aim, shoot, and use a defensive dash under the fixed-angle gameplay camera.

**Scope.** Demo uses greybox rooms, a physical doorway, a movement lane, and warning/occlusion fixtures. Shooting and dash stages extend the same test space with a safe area, targets, and damage fixtures. Expedition generation, enemy AI, shelter progression, dialogue content, and an attack catalog belong to later plans.

## Stage order

| Stage | Deliverable | Depends on |
| --- | --- | --- |
| [0. Project preparation](Stage00-ProjectPreparation.md) | Buildable Editor target and reproducible Demo entry point | Existing C++ project |
| [1. Character movement](Stage01-CharacterMovement.md) | Screen-relative WASD and walking collision | Stage 0 |
| [2. Gameplay camera](Stage02-GameplayCamera.md) | Shared camera settings, connected room framing, follow, and foreground visibility | Stage 1 |
| [3. Character aiming](Stage03-CharacterAiming.md) | Body-plane mouse aiming and the shared cursor/input contract | Stage 2 |
| [4. Character shooting](Stage04-CharacterShooting.md) | Held-fire scheduling, test projectiles, and typed damage reception | Stage 3 |
| [5. Defensive dash](Stage05-DefensiveDash.md) | Grounded dash, brief protection, and readiness presentation | Stages 1–4 |

## Target responsibilities

- GameMode selects the character and controller. The controller owns input and control availability.
- Character Movement owns walking and the planned dash mode. Aim owns horizontal facing.
- The camera manager owns view/follow behavior; room bounds provide floor geometry and the shared camera Data Asset supplies tuning.
- The planned fire component owns shot scheduling, projectiles own travel/impact, and the damage receiver applies typed damage with the dash protection check.
- Blueprint presentation consumes native state and events.

[Architecture.md](../Architecture.md) records the current implementation. Each stage owns its detailed steps, tuning choices, and checks. The [game design index](../../GameDesign/GameDesign.md) selects the gameplay rules. Open only the chosen stage and the specific prerequisite sections it needs; preserve completed work when extending the prototype.
