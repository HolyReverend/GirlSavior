# Enemy waves and room integration

**Stage CP3.** Run one pressure system across the test route. Introduce each role, then mix roles as the player advances. Reinforcements use reachable entrances outside the current frame. The player can leave an ordinary room while enemies are alive. [Wave design](../../GameDesign/Mechanics/Encounters/EnemyWaves.md) · [Pressure profiles](../../GameDesign/Mechanics/Expeditions/ExpeditionDifficulty.md) · [Ordinary rooms](../../GameDesign/Mechanics/Encounters/CombatEncounters.md)

## Encounter foundation for CP1

Before adding waves, place `AGirlSaviorCombatEncounter` in the first-fight map with waves disabled. Give it the live hero reference, Waiting/Active/terminal state transitions, a first-room overlap, the safe pocket, and the combatant/projectile registries. Implement activation, death, terminal cleanup and the [view query](#3-check-spawn-visibility-and-access) here. Use one explicit encounter reference for placed combatants, safe volume and presenter. The encounter resolves possession and attaches receivers, then registers fixtures after map BeginPlay and presenter binding. Enable attacks only when the hero enters the first fight and initialization is valid. CP2 adds the dangerous slot. CP3 adds authored profile/topology data, retirement and the scheduling logic below.

## 1. Author the route in the Editor

| Owner | Data and wiring |
| --- | --- |
| `AGirlSaviorCombatEncounter` in `Encounters/` | One actor in the map. Holds the active hero, ordered run state (`Waiting`, `Active`, `Succeeded`, `Defeated`), current room, wave deadlines/reservation, live registries and the dangerous slot. Expose read-only run state and combatant registration/removal events to the presenter. Add a waves-enabled flag for the CP1/CP2 placed-enemy fixtures. |
| `AGirlSaviorCombatRoom` in `Encounters/` | One authored room ID, a hero overlap volume, explicit neighbor links, associated door references, a profile and a list of entrance groups. Give each entrance group an approach direction and at least four exact capsule-center slots on one continuous floor path. Use floor markers and editor-only arrows for authoring. |
| `UGirlSaviorWaveProfile` in `Encounters/` | Desired population, refill threshold, normal interval, optional one-time introduction role, ordered eligible group recipes of at most four roles, and empty-frame acceleration flag. Reference the approved enemy classes through the encounter's role-to-class list. Validate unique room IDs, positive intervals, valid classes and the threshold/target relation at startup. |
| Doors and route triggers | Use the actual door blocking state for each adjacency edge. Author Nav Modifiers with `NavArea_Null` while closed and cleared when open. With Runtime Generation = Dynamic Modifiers Only, keep the navmesh baked on the open floor. Reject closed edges immediately while nav tiles catch up. Make crossings explicit overlaps. Prefer nonoverlapping room volumes with doors/corridors assigned to a single logical room. |

The first combat-room overlap starts the run and makes its group immediately eligible. Resolve room changes from the possessed hero and keep the last room through a threshold gap. On same-frame overlaps, choose the room containing the hero capsule center in the authored route order. Clear the choice on EndPlay. Keep explicit checks for a hero already inside at possession/start. Returning from a later room uses the current room's profile and preserves its introduction flag and the last actual group time.

### Author starting profiles

| Section | Target / refill at or below | Normal check interval | First group and repeat examples |
| --- | --- | --- | --- |
| Pursuer introduction | 3 / 1 | 7 s | First one Pursuer. Then 2–3 Pursuers when budget permits. |
| Shooter introduction | 3 / 1 | 7 s | First one Shooter. Then Pursuer + Shooter, with Pursuers as fallback. |
| Charger introduction | 3 / 1 | 7 s | First one Charger. Then Pursuer + Charger, with Pursuers as fallback. |
| Quiet connector | 2 / 1 | 10 s | Up to two Pursuers, ordinary interval even during an empty frame. |
| Mixed room | 6 / 4 | 5 s | Start with 2 Pursuers + Shooter + Charger when four slots and both roles are available. Keep at least one Pursuer in repeat groups. Shorten the recipe or use a Pursuer fallback when the role or entrance budget is smaller. |

Treat these values as playtest starting points. Set the global caps to **8** occupied places, **2** Shooters, **1** Charger, **4** per group and **3 s** minimum gap between actual groups. Charge caps across all rooms. Role recipes use ordered priorities. Clip to valid slots and use the stable asset/room order for ties. Spawn a one-enemy group for a first introduction, a quiet section, or when only one valid place/role remains. Mark introduction served only when that role actually spawns in its owning room. Repeat groups on the mixed route include a Pursuer.

## 2. Keep one live registry and at most one reservation

- Register each eligible live enemy once by actor identity, storing role, room and whether it belongs to an encounter fixture. Track any special preplaced test enemies in the same caps. Registration occurs before activation and requires valid configuration, possession and a free place. Check the actor's live receiver. A death, explicit retirement or unexpected EndPlay frees the live place once. Corpses, projectile actors and dissolved reservations have no live place. Track all live combat projectiles separately for terminal cleanup and slot ownership.
- A reservation stores group ID, intended roles/count, chosen entrance, room/profile ID, created time and due time. Start with a **0.20 s** reservation delay. Any reserved slot participates in total and per-role counts. End with commit or cancellation. While pending, skip additional scheduling attempts. Keep a bounded **0.25 s** retry between failed candidates. Recalculate admission from current game time on every attempt.
- On each eligible late-frame update, refresh the view and hero/room state. Process terminal/safe states and retirement before evaluating the reservation or trying to create one. Use a **0.25 s** query cadence for population/entrance decisions. Read live counts from engine-authoritative registration and cap at each mutation.

## 3. Check spawn visibility and access

1. In `TG_PostUpdateWork`, get the local hero controller's `ULocalPlayer`, viewport, and `FSceneViewProjectionData` using the existing aim path. Use its actual constrained rect and view-projection matrix. Cache this snapshot for the current frame and expose it to the attack check. Require a valid gameplay camera and current projection. Defer new attacks and groups that need projection while it is unavailable. [Epic projection API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FSceneViewProjectionData) · [Existing aim projection](../../../GirlSavior/Source/GirlSavior/Characters/GirlSaviorAimComponent.cpp)
2. For each slot form a world-space AABB that encloses the intended enemy capsule and the entire spawn silhouette. Project all eight corners in absolute viewport pixels. If a corner cannot be projected safely in front of the near plane, defer this candidate. For a successful projection, use the projected bounds as a conservative visible test. Require no intersection with the constrained rect expanded by **32 px** on each side. Dark or occluded points inside the rect count as on-screen. Use the unexpanded rect for active enemy bounds and render/log coverage checks.
3. Verify the slot's capsule with a current-world overlap test using movement blocking responses, and confirm walkable support. Project the corresponding floor point to NavMesh within an authored tolerance. Use the authored capsule-center transform for spawning and reject significant navigation offsets. Run a full `FindPathSync` query from the slot to a valid hero approach point with partial paths disabled. Reject a path whose segments cross the authored doorway bounds of a currently closed door or cross into the safe pocket. This check covers the time between a door state change and navmesh refresh. [Epic navigation mesh modifiers](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-how-to-modify-the-navigation-mesh-in-unreal-engine)
4. Require approach path length of at least **300 cm** as an initial clearance. Score entrance groups by shortest available path among fully valid candidate slots, keep all members in one direction, and use the stable entrance ID for ties. Author entrances so at least one valid approach leaves a separate walk-off lane. If the whole combat room is visible, use deeper linked corridors. When a group cannot enter fairly, let the attempt wait.
5. For empty-frame acceleration, first check live enemy bounds using the same unexpanded rect. If none intersect, estimate approach time for registered reachable offscreen enemies from their remaining path points and walk speed. Sample the path toward the hero at a bounded spacing (start at **100 cm**) to find the first point whose body enters the rect. Defer the fast check when a live approach is estimated to become visible within **1 s**. Fall back to the normal interval if ETA or projection is unknown. Record actual elapsed time in the playtest.

## 4. Schedule, revalidate, and commit

```text
if run is inactive or hero is protected:
    cancel any reservation
else if reservation exists:
    if now >= dueTime: revalidate and finish reservation
else if view is valid and retry is due and TimingAllows(profile, now):
    if occupied <= profile.threshold:
        budget = min(profile.target - occupied, globalCap - occupied, 4)
        choose roles and an entrance within budget and role caps
        reserve the chosen group or wait until the next check

TimingAllows(profile, now):
    if LastCommittedGroupTime is unset: return true
    if now < LastCommittedGroupTime + globalMinGap: return false
    return now >= LastCommittedGroupTime + profile.interval
           or (profile accelerates and frame is empty and no imminent approach)
```

1. For revalidation, take the **current** profile, counts, hero, doors and final camera snapshot. Require a valid view and rerun `TimingAllows` with the current profile for every reservation. Compute `baseline = live + reserved other than this group`. Require baseline at or below the current threshold. Require `baseline + this group` within target, global cap and role caps. Check the current recipe and whether the current room/profile permits this entrance. If a check fails, cancel the entire reserve and retry later. Retain `LastCommittedGroupTime`, initially unset through failed first attempts.
2. Recheck the exact transforms using Step 3 immediately before spawning. Use deferred spawn with `DontSpawnIfColliding` and initialize each actor with its definition, room and encounter. For each valid success, exchange the reserve for a live record before enabling the AI. Activate the group only after processing the commit. On engine spawn/initialization failure, reclaim the unused entry without a kill. A partially successful group is the one actual group for this check. If at least one enemy registered successfully, update `LastCommittedGroupTime` from its commit. Cancel and back off when all fail. Commit at most one group per check.
3. Example with target **6**, threshold **4** and **4 live**: reserve two. On commit the threshold sees **4**, and the cap check sees **6**. If the hero moves to the quiet profile (target **2**) before commit, cancel the reserve. The four live enemies stay subject to tracking and retirement, and their existence blocks further refills until pressure permits.

## 5. Retire and protect consistently

- Keep an enemy live while it is in the current frame or can reach the hero from the current or a neighbor room. Update each enemy's room via the authored volume or the same spatial lookup and same gap rule as the hero. For a regular enemy outside the frame and too far in room topology, or outside the frame with no reachable path from a nearby room, retire it without death/reward. Recheck invalid paths on a bounded cadence and require two consecutive failures for a nearby enemy so nav rebuilds do not retire it immediately. Skip visibility-dependent retirement if projection is unavailable. A fixture explicitly attached to an unfinished objective can be marked retained. It still counts toward the cap.
- Author the start pocket using the existing `AGirlSaviorSafeArea` overlap for the hero's outgoing input gate. Reuse that volume for the encounter's protected-target query. Cover the pocket with `NavArea_Null` for enemies, and author projectile-only blocking panels at open boundary segments that also block `CombatOccluder`. Hero walking still crosses. Clear all enemy spawn slots from the protected volume. On entering, cancel pending reservations and attached attacks targeting the hero. Enemies wait outside, counted as live, and may resume after exit. For retirement while the hero is protected, use the last outside approach point to evaluate nearby enemies. An in-flight projectile that reaches the boundary is intercepted by collision. Verify protected-target checks and both projectile responses separately.
- At the exit or hero death, latch terminal state once, stop scheduling, clear reserves, and cancel attached attacks. Clean up registered live enemies and in-flight combat projectiles safely after any active damage dispatch. Unbind actors on teardown and clear the slot. See [death ordering](DamageSystem.md#4-connect-death-to-the-run) and the [outcome overlay](CombatFeedback.md).

## 6. Verify scheduling and level setup

| Scenario | Expected evidence |
| --- | --- |
| Quick kills and feasible entrance | First group appears in frame **1–3 s** after start. After the fast clear, measure time until the next group becomes visible. Target about **3 s** after both an entrance and the minimum-gap window become available. An imminent approach prevents a redundant fast refill. |
| Slow clear, pause or hitch | With live, reserved and fixture enemies, occupancy never exceeds **8**, role caps hold, and actual groups stay at least **3 game seconds** apart. No catch-up burst. The quiet connector uses its normal interval. |
| Profile change with group pending | Count reserve once. Move into the quiet profile after reserve and before commit. If invalid, all reserved places free. Returning uses that room's existing intro status and global last-group time. |
| Projection or route invalidation | Sprint or dash toward an entrance during reservation, close its door, stand in a room whose entire footprint is visible, and resize PIE. Reprojection and path/overlap checks cause the group to wait or retry. No member appears in the current visible bounds. |
| Terminal and retirement | An unreachable offscreen regular enemy retires and never respawns solely because the player backtracks. A nearby enemy with a route remains counted. Exit, death, safe entry and Retry follow their respective cleanup rules. |

Automate count math, elapsed-time admission, reserve cancellation, duplicate cleanup and bounds tests with synthetic projections. In PIE verify the actual cooked-style map wiring, visual appearance frame, paths, door transitions and the shared dangerous slot. Log game time, room/profile, live and reserved counts, role counts, group ID, entrance ID, visibility timestamp and reason for each commit or cancellation. Treat pacing as a playtest target. [Wave playtest card](../../OpenItems/GS-013-EnemyWavePressure.md)
