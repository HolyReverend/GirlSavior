# Camp prototype implementation plan

**Status.** CAMP 1 through CAMP 4 are technically verified. The [acceptance record](Acceptance.md) includes the Win64 build, walkthrough captures, and pending manual playtest cases.

**Goal.** Walk around a small 3D Camp, meet three heroine placeholders, select two of three skills at the expedition table, play the existing combat route with those effects, and return to Camp after reaching the exit or dying.

**Architecture.** Keep the `GirlSavior` Runtime module. Camp owns interaction and the preparation draft. A game-instance subsystem carries the confirmed loadout across map changes. The existing combat encounter owns fight progression and attaches the selected effects to the new expedition's hero and enemies. C++ owns rules and state. Blueprints and Data Assets own level composition, presentation, and tuning.

**Read in order.** This plan, [Camp scene and preparation](CampScene.md), [heroine and skill data](SupportSkills.md), then [map travel and combat integration](ExpeditionHandoff.md). Read the [current architecture](../Architecture.md), [Unreal rules](../ImplementationRules.md), and project `stage-orchestrator` workflow before implementing. Use UE 5.9 and the toolchain specified in the current architecture. Follow the project C++ and Data Asset comment rules. [Build and automation commands](../../../Codex/UnrealMCP.md)

## Player contract

| Step | Observable result |
| --- | --- |
| Enter Camp | The hero starts near the table. WASD and the existing isometric camera work. The campfire, workshop marker, shelter marker, Liora, Ida, and Saira are recognizable. |
| Approach the table | One prompt reads `E — Стол экспедиций`. `E` opens the mission list. The single available test mission opens a separate preparation screen through `Настроить вылазку`. |
| Prepare | Three skill cards show a heroine name, icon, effect, and description. Pick exactly two distinct cards in any order. A selected card can be removed. The pistol is shown in slot 1 as fixed equipment. |
| Confirm | `Отправиться` validates the draft and opens `/Game/Maps/TestMaps/CombatPrototypeRoute`. The new hero starts in the existing safe pocket with full health, the pistol, and both selected effects. |
| Finish | The existing route decides success or defeat. Its outcome panel offers `В лагерь`. The hero arrives near the Camp table. Opening it again shows the last confirmed pair from this play session. |

**Prototype decisions.** The GDD selects up to two rescued heroines, each contributing an active and a passive ability. This fixture exposes three predefined skills, one per heroine, and requires exactly two skills. All three placeholders and cards are available at the start. A single fixed mission tests the link, and the pistol uses the existing combat loadout. Camp placement at the table and health on the next launch come from fresh actors in each world. The last pair is retained during the current game instance. [Camp](../../GameDesign/Mechanics/Camp/CampHub.md) · [Expedition table](../../GameDesign/Mechanics/Camp/ExpeditionBoard.md) · [Heroine support](../../GameDesign/Mechanics/Heroines/HeroineSupport.md) · [Expeditions](../../GameDesign/Mechanics/Expeditions/Expeditions.md)

**Scope for later stages.** Profile persistence, unlocks and rescue, full active and passive kits, rank upgrades, relationships and VN scenes, workshop and construction systems, weapon editing, mission and region catalogs, procedural routes, loot, and production extraction rules. Keep the map entry point additive. `Demo` remains the editor and game default. Direct starts of `CombatPrototypeRoute` and `CombatPrototype` keep their existing standalone outcomes and Retry path.

## Build order

### CAMP 1 · Definitions and validation

- [x] Create the native classes and value types in [Support Skills](SupportSkills.md). Compile the Editor target before creating assets that use their schemas.
- [x] Author `DA_CampPrototype`, three heroine assets, three skill assets with inline effect definitions, and placeholder icons. Validate the roster, IDs, ownership, repeated assignments, effect presence and numeric values. Share these validators between editor and launch.
- [x] Save and reload assets. Verify that the heroine assets hold skill references and that modifying the typed value in a skill asset changes the data read by a card without a Blueprint code edit.

Win64 Development Editor built successfully on UE 5.9. A fresh editor process passed 12 `GirlSavior.` Automation tests, including `GirlSavior.Camp.Schema`, `GirlSavior.Camp.SavedFixture`, and `GirlSavior.Camp.Validation`. A separate editor process reopened the saved assets and confirmed reflected edits update all three native card details. The edits were restored without saving. The native factory test confirmed new runtimes use the current tuning and existing runtimes retain their original values.

### CAMP 2 · Walkable Camp and table

- [x] Implement the scene and input contracts in [Camp Scene](CampScene.md). Build and save `CampPrototype`, wire its World Settings, and confirm the correct pawn, camera, and controller in PIE. Verify the walk from spawn to table and to each heroine.
- [x] Assign both map references on `DA_CampPrototype` and rerun full validation on that asset.
- [x] Wire the mission page, preparation page, prompt, and read-only heroine info panel. Verify `E`, `Esc`, the two-card cap, removing and replacing a card, and the draft rules. Confirm that LMB and Left Shift cannot fire or dash in Camp.
- [x] Verify the draft and launch validation through native tests. CAMP 3 connects the launch action to the session.

The saved Camp reopened with both world references and passed Map Check. All 13 `GirlSavior.` Automation tests passed. `GirlSavior.Camp.Preparation` validates all three pairs, empty and invalid drafts, and a changed mission. In floating single-player PIE, `GirlSavior.TestCamp` passed 53 checks. It walked to the table and all three heroines, verified targeting with competing and blocked objects, clicked the mission action through Slate, and exercised modal focus, card selection, draft retention and discard, and return to movement.

### CAMP 3 · Handoff and real combat effects

- [x] Implement the subsystem and guarded consume path in [Expedition Handoff](ExpeditionHandoff.md). Run the direct-route baseline with an empty subsystem and compare its outcome and Retry behavior.
- [x] Install effect runtimes using the existing fire, projectile, enemy movement, vitals, and HUD seams. Verify each effect with the selected skill and with a pair that excludes it. Verify each of the three possible pairs and return after both outcomes.
- [x] Exercise stale, duplicate, or mismatched travel requests and a controlled failure that preserves the origin. Verify launch and return can be attempted again. Verify that a failure during encounter initialization exposes a way back to Camp. Exercise engine fallback separately and verify the stale request is cleared.

The final Win64 Development Editor build passed all 15 `GirlSavior.` Automation tests. `GirlSavior.TestCampExpeditions` passed 88 checks in three Camp → route → Camp round trips in one PIE session. It reopens the table after each outcome and verifies the last committed order. It uses known gameplay positions and real projectile, damage, exit and UI paths to isolate each condition. It also measures real pistol cues and confirms that slow and recharge timers pause with the game. `GirlSavior.TestCampRecovery` passed 37 checks across missing, malformed and stale tokens, failed route initialization, engine fallback to Demo, and standalone Retry. The `GirlSavior.Camp.ExpeditionSession` test covers duplicate claims and game-instance isolation.

### CAMP 4 · Acceptance and handoff

- [x] Build Win64 Development Editor and run the existing `GirlSavior.` Automation suite plus the Camp tests. Run the existing Demo and combat route probes that cover the touched fire/damage/outcome behavior. Record the command, result, and log for each check.
- [x] Run the technical acceptance cases below in single-player PIE starting from Camp. Reopen the saved map and assets from disk. Cook the Camp and route maps and complete one round trip in the packaged game.
- [x] Record actual results in the [acceptance record](Acceptance.md), update [Architecture](../Architecture.md), and record prototype evidence in [GS-018](../../OpenItems/GS-018-HeroineSupportPlaytest.md) and [GS-019](../../OpenItems/GS-019-CampInteractionPlaytest.md). The two cards have closure conditions for the full design.

## Acceptance cases

| Check | Expected result |
| --- | --- |
| Fresh Camp | Correct player spawn, frame, movement and collision. `E` away from interactables does nothing. Near competing targets, one prompt matches the object that opens. A blocked target is unavailable. |
| Focus and draft | WASD stops during modal panels. `Esc` returns from preparation to missions with the draft intact, then closes the table and restores movement. Reopening starts from the last confirmed pair, or empty on first launch. |
| Selection | Test zero, one and two cards. Try a duplicate and a third card. Launch requires two unique available cards. Remove one and use another. Repeat with all three pairs. |
| Actual launch | A double click issues one travel. Existing route, safe pocket, waves, pistol, dash, and encounter admission remain in use. The hero starts with full health. `RunId`, `MissionId`, and the pair of `AbilityId`s agree in launch and attach logs. |
| Combat effects | Compare Liora's positive weapon hit and expiry, Ida's shot deadline, and Saira's initial barrier and recharge. Selected effects apply together and excluded effects are absent. Pause longer than a recharge or slow duration, then resume and confirm that only elapsed game time counts. In a fixture with a nonzero baseline barrier, trigger a lethal hit with Saira selected and verify the committed HUD values, damage cue, terminal notice and cleanup. |
| Next expedition | Return once after success and once after defeat. Change the remembered pair, launch again, and verify a new `RunId`, clean full health, and only the newly confirmed effects. |
| Failure and direct starts | A preflight error leaves a usable origin UI. Engine fallback clears the stale request. An absent token on a pending destination, a malformed token and a stale token each fail to apply a loadout. A tokenless direct start without a pending destination and its Retry retain baseline combat and the existing action. |

Capture one Camp walkthrough and one Camp → route → Camp walkthrough for manual acceptance. Include controls, expected outcomes, and the specific cases still awaiting the user. Record checks actually run separately from pending manual cases.
