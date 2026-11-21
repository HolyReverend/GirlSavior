# Ordinary enemies and attack execution

**Stages CP1–CP2.** Implement Pursuer, Shooter and Charger as three definitions of one enemy character. Each has one attack and a recognizable movement pattern. Start with a placed Pursuer before connecting the roster to [waves](EnemyWaves.md). [Enemy roles](../../GameDesign/Mechanics/Encounters/EnemyTypes.md) · [Attack warnings](../../GameDesign/Mechanics/Encounters/EnemyAttackFeedback.md)

## 1. Create the C++ actors and data

| File or asset | Responsibility |
| --- | --- |
| `Enemies/GirlSaviorEnemyCharacter.h/.cpp` | `ACharacter` with capsule, standard Character Movement, damage receiver and attack component. Owns definition initialization, facing during an attack and one-time death/retirement. Use the `DashHardBlocker` actor tag on live blocking enemies for Stage 5. |
| `Enemies/GirlSaviorEnemyAIController.h/.cpp` | `AAIController` that acquires the encounter's live hero and requests path following. Owns the current move request and a bounded reconsideration interval. |
| `Enemies/GirlSaviorEnemyAttackComponent.h/.cpp` | Owns one attack instance at a time, transition deadlines, direction locking, authoritative threat geometry, hit resolution and attachment cleanup. Publishes read-only preparation, update and end cues keyed by AttackId. |
| `Enemies/GirlSaviorEnemyDefinition.h` | `UDataAsset` with role, health, walk speed, capsule size, attack tuning and optional projectile class. Use one reflected tuning struct per attack kind. Select the active struct by role and validate it with `IsDataValid` and on initialization. |
| `BP_EnemyPursuer`, `BP_EnemyShooter`, `BP_EnemyCharger` and matching `DA_Enemy…` | Bind definition, placeholder mesh, readable silhouette and cue assets. Assign `BP_EnemyProjectile` only to Shooter. Core state lives on the native classes. |

Add `AIModule` and `NavigationSystem` to the current module for the AI and nav APIs. Use forward declarations and keep engine AI/nav includes in implementation files where possible. Configure the enemy AIController class with Auto Possess AI = **Placed in World or Spawned**. For waves, initialize encounter, definition and room identity before activating AI. Use deferred spawn and an explicit activation call after successful registration. Placed fixtures follow the same registration and activation path. If configuration or possession fails, report the failure and remove the actor from the run without a kill.

Use the Pawn object type and the projectile response pairs from the [damage spec](DamageSystem.md#3-use-one-projectile-pipeline). World geometry and live pawn capsules block normal movement. Add a `CombatOccluder` trace channel set to default Ignore. Set blocking walls, cover, closed doors and protected boundaries to Block. Pawn and cosmetics Ignore. Use simple collision. Give all enemy roles the same initial capsule (radius **34 cm**, half-height **88 cm**). Author walkable passages for their nav agent. Disable nav relevance on enemy bodies so they do not cut moving holes in the navmesh.

### Starting values

These are prototype tuning proposals. All distances use cm and all times use game-time seconds.

| Role | HP / walk speed | Admission geometry | Preparation / locked tail / recovery | Delivery |
| --- | --- | --- | --- | --- |
| Pursuer | 30 / 380 cm/s | Visible target, attack sector reach **170**, half-angle **45°** | **0.60 / 0.30 / 0.75 s** | One sector hit for **10 physical** |
| Shooter | 40 / 320 cm/s | Line of fire, within **900** center-to-center, with preferred distance **700** | **0.85 / 0.35 / 0.8 s** | One **12 physical** projectile, radius **12**, speed **1100 cm/s**, max distance **1100** |
| Charger | 60 / 300 cm/s | Clear straight approach, range **350–750** center-to-center | **1.0 / 0.40 / 1.1 s** | First body contact for **18 physical**, speed **1300 cm/s**, max travel **750**, stripe width = capsule diameter |

Validate all values for finiteness, positive speed/HP/range, reachable capsule dimensions, valid phase durations and `0 < LockedTail < Preparation`. The speed and windup give a first-pass window to step sideways at the hero's **500 cm/s** walk speed. Verify that window next to a pillar and when dash is on cooldown. Change unsafe geometry or tune the attack if the authored lane has no practical walk-off route.

## 2. Navigate and acquire attacks

1. Use `NavMeshBoundsVolume` across the test route and entrances. In Approach, use `AAIController::MoveTo` with a nonpartial path, a role-specific acceptance radius, and an update interval of about **0.20 s**. Refresh when the target moves appreciably, the path fails, or the room route changes. Keep a still-valid request between updates. Stop the move request once when entering preparation, recovery, death, retirement or protected-target waiting.
2. Pursuer follows the hero and requests an attack when the hero's capsule can intersect the sector and cover permits contact. Shooter looks for an in-frame firing position in range with clear line of fire. If blocked, sample reachable left/right positions around the last firing direction using nav projection, full paths and a line-of-fire check from predicted body height. Try up to four authored/local samples per refresh and move to the shortest valid firing position. If none works, use one reachable approach path and reconsider, respecting safe boundaries. Charger approaches the throw window. If too close, seek one reachable lateral/back step within that window. Use the charge's capsule query to require a clear line toward the hero before starting.
3. Set the attack component to `TG_PostUpdateWork` and add the encounter actor as its tick prerequisite. Require live hero, active run, compatible room and safe-boundary state, intact movement/actor state, and a valid warning shape. Shooter and Charger require their current body bounds in the gameplay frame and a visible section of the grounded cue. Use the [combat view snapshot](EnemyWaves.md#3-check-spawn-visibility-and-access) and the visibility rule below. A role still outside the frame continues along the viable approach toward the hero or an in-frame attack position, even if already within its distance window. At a visible eligible position, it can wait for the shared slot, with bounded retries and a fresh eligibility check before windup.
4. Use this conservative visibility rule for the prototype. Require the warning presenter and shape asset to be valid. Sample the line/stripe at its ends and quarter points, with corresponding floor points set slightly above the ground. A point counts when projected inside the constrained rect and a `CombatOccluder` trace from the camera reaches it. Require at least two successive visible points at least **24 px** apart. For the stripe also require its lateral boundaries to be visible at one of those points. Use visible fan samples and the inner part of the sector for the Pursuer. Author depth-tested floor warnings and confirm readability in PIE near column edges and faded walls. This query approximates the rendered cue, so the PIE route still supplies visual acceptance.
5. Recovery holds position and releases back to Approach at its deadline. Reacquire the current hero through the encounter after re-possession. Losing the target or entering protection cancels pending attached attacks. Shielding or damaging an enemy leaves its phases intact while it lives.

## 3. Implement one phase machine

| Phase | Action |
| --- | --- |
| Approach | AI moves or waits. Validate admission before reserving any dangerous slot. |
| Preparation | Assign a unique AttackId from the encounter, stop movement and create one warning from the gameplay shape. Track hero direction until the lock deadline. At lock, store origin, horizontal direction and maximum open shape, and publish a stronger warning update. |
| Active | Commit once after the locked warning has been displayed for at least its configured locked tail. A hitch that crosses both deadlines first displays the locked cue and starts its actual locked deadline. Recheck live state and blockers, keeping locked direction. A body attack ends here on contact or completion. A projectile transfers its slot at launch and continues independently. |
| Recovery | Start the recovery deadline from the actual action/end time. Remove the windup warning or transition to the active stripe/projectile cue. Return to Approach on the first eligible update. |

Use game-time deadlines and state guards for transitions. Warnings and attacks use the same immutable origin/direction once locked. Recheck new obstructions at commit and shorten the gameplay shape and cue when needed. Keep a locked, clipped attack within the warned extent if a door opens. A source leaving the frame after a valid start can finish the locked attack. Preserve the warning's visible part or cancel before committing if there is no way to present a threat to the player. The feedback spec defines cue attachment and lifecycle.

Expose `FGirlSaviorAttackCue` with AttackId, role, phase, phase start/end time, lock state, world-space origin and direction, plus floor polygon points for Pursuer or start/end and half-width for straight attacks. Send it on phase changes and geometry updates. Send one cue end with AttackId and end reason. These values let Blueprint draw the native snapshot directly.

### Pursuer · sector

- Create a flat fan from the capsule's facing toward the hero, with vertices sampled across the half-angle (start with 16 angular segments). Trace the radial ends against `CombatOccluder` at combat body height and project the clipped vertices down to the floor for presentation. Use the same planar fan for the impact test. Test the hero capsule's planar disc against the closed polygon (center inside or edge distance within radius) and require overlapping body height and a clear trace to the hero. Intersect with the locked fan at impact when new cover appears. This yields one sector test against the live hero, followed by one `DirectMelee` hit request.
- End the active phase in the same update, with a swing cue even on miss. Overlap during movement and sustained body contact apply no damage. A sidestep and a wall can both prevent the hit.

### Shooter · straight projectile

- Use a capsule-center start, current yaw and a radius-12 sphere sweep against `CombatOccluder` to derive the warning line endpoint at cover or max range. During preparation, determine line-of-fire by the same radius/height and obstruction rules. At lock, capture that direction and its open distance. At commit, spawn one projectile on the locked line through the [shared launch pipeline](DamageSystem.md#3-use-one-projectile-pipeline). The movement sphere applies the live collision matrix and can hit a cover object that enters the line after lock. If opening the line would extend it, keep the locked budget. Once the line is locked, use it even if the hero moved away.
- If the projectile stops, resolves a hit, expires silently, or is destroyed, conclude the detached attack once. Enter body recovery when the projectile launches. Death during preparation cancels the line. Death after launch leaves the projectile with its snapshot and dangerous slot until completion.

### Charger · swept body

- Project the centerline onto the flat authored floor. Build the stripe from the capsule center corridor (half-width = capsule radius). Clip travel by the capsule sweep against static combat blockers and first invalid supported position. Reuse this sweep and floor-support check for preview and active move. Snapshots include the centerline endpoints and capsule radius so presentation derives the same stripe. How far the hero's own capsule must move to clear it depends on both capsule radii.
- Stop path following and temporarily suspend ordinary Character Movement while advancing the charge. Start movement on the next update after committing to Active. From the actual capsule position, consume `min(speed * DeltaTime, remaining distance)` in short swept legs (prototype max **20 cm**), checking current floor support and blocking geometry. Only the charge advances the body in Active. Use swept movement of the root capsule to the supported endpoint and validate the returned hit. Stop on the first pawn or world contact or the last supported pose before an edge. Keep the fixed heading through completion, including during a long frame.
- Submit `DirectCharge` once on hero contact. A protected hero still blocks the body. Hitting cover or an ally ends travel without damage to the hero. Shorten the active stripe behind the body, play a local stop cue on wall contact and restore walking during Recovery. Death or retirement interrupts it immediately and leaves no active body damage.

## 4. Share one dangerous slot per encounter

- `AGirlSaviorCombatEncounter` owns a single optional record with AttackId and a weak holder. `TryAcquireDangerousAttack(AttackId, Owner)` succeeds only in an active run with an empty slot. Shooter and Charger acquire at preparation start. Pursuer needs no slot. A waiting heavy role can retry on the next AI refresh and emits a warning only after acquisition.
- An attached windup or charge releases on cancel, active completion, owner death, or EndPlay. A Shooter transfers the same record from the attack component to the spawned projectile before movement starts. An unsuccessful launch releases immediately. Release/transfer require the matching AttackId and holder, making duplicate or late release harmless. Completion frees the slot during recovery. On terminal cleanup, invalidate the slot and drain registered projectiles and attacks.

## 5. Verify in isolation and together

| Case | Evidence |
| --- | --- |
| Pursuer contact, then swing | Prolonged contact alone changes zero HP. Staying in the sector at commit gives one hit. Stepping out or moving behind cover gives a swing and zero damage. |
| Shooter and cover | On losing line during approach, Shooter finds a reachable firing angle or waits. After direction lock, the shot keeps its direction when the hero steps sideways. A door closed after lock blocks the projectile. |
| Charger and cover | Stepping beyond the stripe plus hero radius avoids damage. The first wall, ally, or hero stops the capsule. A low-FPS run still stops at the first blocker. |
| Death and slot | Kill each role during preparation. Warning ends and no new attack launches. Kill Shooter just after launch and verify the projectile persists, attributes its damage, and holds the slot until terminal. Kill Charger during motion and verify the move and body hit stop. |
| Pause and ordering | Pause during preparation, projectile flight and recovery. Resume from remaining time. With one Shooter and one Charger, record that their dangerous windows never overlap. Check warning and attack geometry at the camera edge. |

[Prototype plan](ImplementationPlan.md) · [Cue setup](CombatFeedback.md) · [Playtest of attacks](../../OpenItems/GS-028-EnemyAttackFeedbackPlaytest.md)
