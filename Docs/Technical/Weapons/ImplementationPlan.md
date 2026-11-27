# Weapon system implementation plan

> For implementation agents. Follow the project's [implementation rules](../ImplementationRules.md) and the stage orchestrator. Mark each stage after recording its evidence.

**Goal** — Equip the pistol, shotgun and assault rifle at the Expedition Table, take all three onto the Camp-linked route and use them with distinct feedback.

**Status on 2026-11-27** — W1–W4 are implemented. The Win64 Development game passed the three-weapon Camp round trip and restart. The user confirmed W4 manual feedback acceptance.

**Read in order** — [Architecture and interfaces](WeaponArchitecture.md) → [Data and combat](WeaponDataAndCombat.md) → [Camp selection and travel](WeaponLoadout.md) → [Presentation and HUD](WeaponFeedback.md).

**Environment** — Unreal Engine 5.9 Preview, existing `GirlSavior` Runtime module, C++ gameplay, Enhanced Input, DataTable, and UMG/Blueprint content. Target the existing `CampPrototype` → `CombatPrototypeRoute` round trip. New gameplay classes and Blueprints use descriptive names without `Prototype`. Existing map names and fixture identifiers remain valid entry points.

## W1 · Catalog and pistol migration

- [x] Read the saved `BP_CombatCharacter` FireComponent defaults and `DA_CombatPistol` in the editor. Record the baseline values, native fixture callers and Blueprint dependencies before editing. Compare Demo, the standalone combat pistol and the current Camp launch in the execution record.
- [x] Add `Weapons/GirlSaviorWeaponDefinition.*` and `Weapons/GirlSaviorFirearm.*`, including the three-position value, resolved slot contract and a centered discharge. Add `Weapons/GirlSaviorPistol.*`, row validation and `Tests/WeaponDefinitionTests.cpp`.
- [x] Add the table asset and wire the pistol row into the existing native fire, projectile and encounter owners. Assign a pistol icon, the current/default held mesh and the existing shot sound so the row passes validation. Only this stage may use a partial catalog with the pistol row. Keep the existing Camp-origin starting pistol until W3 adds the launch snapshot. Remove the old editable `CombatPistol` property from the gameplay path after migrating its Blueprint reference and checking all callers. Remove `DA_CombatPistol` and the obsolete tuning class only after a reference audit, or retain any genuinely independent fixture use. Use the table as the migrated pistol's single authority for combat tuning.
- [x] Migrate the pistol property-dependent fixtures in `Tests/CampCombatEffectTests.cpp`, `Tests/PrototypeFeedbackCommands.cpp`, `Tests/PrototypeFirstFightAcceptance.cpp` and `Camp/GirlSaviorCampExpeditionProbe.cpp`. Configure the native mechanics tests with the saved pistol row and make saved map probes read the configured weapon or its launch parameters. Verify the unarmed recovery path in `Tests/CampTravelRecoveryPIE.cpp`. Keep the existing single-pistol support behavior until W2 migrates the weapon-source check.
- [x] Build `GirlSaviorEditor` and run `GirlSavior.Weapons.Definitions` and the existing fire, damage and Camp regression suite. Inspect saved asset references after restarting the editor. Verify 4 shots/s at clean tick intervals, 10 base damage, correct hit and 14 m expiry, and working Demo, enemy projectile and standalone Retry paths.

### W1 execution record · 2026-11-24

The saved baseline measured 10 damage, 4 shots/s, 6 cm radius, 2000 cm/s speed and a 1400 cm cutoff. The pre-migration Demo lifetime probe passed. Standalone combat and Camp were exercised after migration. The saved-content check initially reported the missing catalog and later reopened the table and both character Blueprints successfully in a fresh editor process. The old Data Asset and native tuning class were retired after a saved reference audit.

| Check | Result |
| --- | --- |
| Win64 Development Editor build | Succeeded with UE 5.9 and MSVC 14.50. `Saved/W1/BuildVerified.log`. |
| Saved table and character wiring | Confirmed the starter row, combat table reference and independent Demo defaults. `Saved/W1/VerifyFinal.log`. |
| `Automation RunTests GirlSavior.` | 17/17 passed, including both weapon definition tests and the fire, damage and Camp regressions. `Saved/W1/AutomationVerified.log`. |
| Demo PIE | Combat probe 37/37 and projectile lifetime passed. `Saved/W1/PIEFocused.log`. |
| Standalone combat PIE | 22/22. Real LMB discharges logged at 4 shots/s. Clear shots expired at 1400 cm at both 60 and 15 Hz. The cover, safe entrance and damage checks passed. `Saved/W1/PIEFocused.log`. |
| Camp travel, support and recovery PIE | 88/88 across three Camp round trips, 24/24 for travel recovery and 4/4 for direct-start Retry. `Saved/W1/PIEFocused.log`. |
| Shooter and projectile contacts PIE | Both cases passed. A Shooter projectile remained live after its source died. Health, geometry, ward and silent expiry remained distinct. `Saved/W1/EnemyProjectileVerified.log`. |

An early Camp probe used the legacy Demo projectile field for a synthetic hit. The migrated probe now uses the resolved weapon's projectile class and retains its controlled nonlethal damage. The focused rerun allowed the play window to acquire foreground status before injecting input.

## W2 · Shotgun, rifle and shared firing integration

- [x] Add `Weapons/GirlSaviorShotgun.*` and `Weapons/GirlSaviorAssaultRifle.*`. Add their rows to `DT_Weapons`. Use valid temporary mesh, sound and icon references with positive presentation values until W4 supplies the final variants. Use a developer test fixture with resolved rows to exercise all behavior before the table page lands.
- [x] Add one active-slot path to `Combat/GirlSaviorFireComponent.*`. Bind the three Boolean actions in `Core/GirlSaviorPlayerController.*`, with mapping registration and cleanup alongside `Encounters/GirlSaviorCombatEncounter.*`. Assign `IMC_ExpeditionWeapons` and actions on the combat controller content path. Stage output is a fixture that can fire and switch all three through the same component and numeric keys that Camp uses later.
- [x] Extend `Combat/GirlSaviorTestProjectile.*` and `Combat/GirlSaviorDamageTypes.h` with the launch metadata, falloff and trail snapshot. Update `Support/GirlSaviorSupportRun.cpp`, projectile admission and the existing Camp support probes for the typed hero weapon marker. Keep existing damage receiver callbacks and the enemy dangerous attack slot. Add regression cases in `Tests/WeaponCombatTests.cpp`, `Tests/WeaponDamageTests.cpp` and the existing `CampCombatEffectTests.cpp`.
- [x] Run `GirlSavior.Weapons.Combat` and `Automation RunTests GirlSavior.`. In a focused combat PIE instance verify fixed fan, rifle spread reset, no cooldown bypass, safe areas and ending a run while a projectile is in flight. Use the [acceptance matrix](#acceptance-matrix) for expected results.

### W2 fixture

Launch a non-Shipping editor with `-GirlSaviorWeaponCombatTest`, load `/Game/Maps/TestMaps/CombatPrototypeRoute` and start PIE. Slot 1 is the pistol, slot 2 is the shotgun and slot 3 is the rifle. Press the matching top-row key and fire with LMB after leaving the safe pocket. The fixture uses the same slot and fire path as combat. Camp-origin and ordinary direct runs still start with only the pistol until the table flow is implemented in W3. The two new rows share the temporary icon, mesh and shot sound. Their projectile trail lengths are already driven by the table.

### W2 execution record · 2026-11-25

| Check | Result |
| --- | --- |
| Win64 Development Editor and saved assets | Build succeeded. A fresh process reloaded all three table rows and the controller's mapping context and actions. `Saved/W2/BuildEndpoint.log`, `VerifyVerified.log`. |
| `Automation RunTests GirlSavior.` | 21/21 passed, including fan, consistently seeded rifle spread, switch deadline, falloff and support. `Saved/W2/AutomationEndpoint.log`. |
| Focused combat PIE with three slots | 17 checks passed. Actual key events selected slots 1–3. One shotgun cue accompanied five pellets at −12°, −6°, 0°, 6° and 12°. The rifle repeated and returned to a tight shot after a pause. Tests covered safe zones, a blocked cooldown tap, a canceled hold and projectile cleanup at defeat. `Saved/W2/CombatPIEEndpoint.log`. |
| Normal entry points | Demo 37/37 plus lifetime, empty slot preserved the pistol, first fight 25/25, Camp round trips 88/88, travel recovery 24/24 and direct Retry 4/4. `Saved/W2/RegressionEndpoint.log`. |

The first input runs arrived with a background editor window. The verified rerun required foreground status and viewport focus. The real sweep fixture found an exact-endpoint contact missed by Chaos. The final range query accepts contacts at the end within numerical roundoff, and the fixture also checks that a blocker 1 cm beyond remains untouched. The build consumed the existing uncommitted W1 changes.

## W3 · Weapon page, profile and authenticated launch

- [x] Add `Weapons/GirlSaviorWeaponProfileSubsystem.*` and `Weapons/GirlSaviorWeaponSaveGame.h`, with record validation, retryable confirmation and an isolated playtest option. Cover fresh, old, invalid and failed-write cases in `Tests/WeaponProfileTests.cpp`.
- [x] Add `Weapons/GirlSaviorWeaponLoadoutWidget.*` and `WBP_WeaponLoadout`. Extend `Camp/GirlSaviorCampDefinition.*`, `GirlSaviorCampPlayerController.*` and `GirlSaviorCampWidgets.*` for the table reference, draft and navigation. Update `DA_CampPrototype` and `BP_CampPlayerController`, and extend the native layout used by `WBP_CampPreparation`. Verify key activation, Escape and focus in PIE.
- [x] Extend `Expeditions/GirlSaviorExpeditionLaunch.h`, `GirlSaviorExpeditionSessionSubsystem.*` and `Encounters/GirlSaviorCombatEncounter.*` with the validated snapshot and rebind behavior. Add coverage to `CampExpeditionSessionTests.cpp` and `CampTravelRecoveryPIE.cpp`. Exercise launch failure, recovery and report return through existing entry points.
- [x] Run `GirlSavior.Weapons.Profile` and the full suite. Prove, through table confirmation and a process restart, that slot order is durable. Launch with a deliberate gap in slot 1. Verify initial equip and switch behavior, return after victory and defeat, and that reopening the weapon editor starts from the confirmed set after discarding an unconfirmed draft.

### W3 execution record · 2026-11-25

Start the non-Shipping editor with `-GirlSaviorWeaponPlaytest`, open `/Game/Maps/TestMaps/CampPrototype` and start PIE. The separate playtest profile has all three weapon cards available for placement at the table. It starts with the pistol equipped. An ordinary new profile still owns only the pistol.

| Check | Result |
| --- | --- |
| Win64 Development Editor and saved content | Final build succeeded. A fresh editor loaded the shared table reference and saved widget binding. `Saved/W3/BuildFinal.log`, `Verify.log`. |
| `Automation RunTests GirlSavior.` | 24/24 passed. Covers profile confirmation, storage failure, record recovery and playtest isolation. Session regression verified that a subsequent profile write leaves the pending snapshot intact. `Saved/W3/AutomationFinal.log`. |
| Isolated save and victory PIE | 27 checks passed. Enter opened the mission weapon page, equipped two weapons with slot 1 intentionally empty, and confirmed to disk. The preparation entry swapped the draft slots, then Escape discarded the swap and restored focus. The route equipped slot 2. Victory returned the saved order. `Saved/W3/PIE_Write.log`. |
| Fresh-process restart and defeat PIE | 21 checks passed. The fresh process read the same disk record and restored slot order and the empty first position. Defeat returned to Camp and preserved the confirmed order. Both runs exercised same-frame possession, a possession gap and a replacement pawn with a still-pending shot deadline. `Saved/W3/PIE_Read.log`. |
| Ordinary-profile regression PIE | Demo 37/37 plus lifetime, standalone battle 25/25, Camp interaction 52/52, Camp round trips 88/88, travel recovery 32/32 and direct Retry 4/4. `Saved/W3/RegressionPIE.log`. |

The W3 PIE probe used real Slate key events for Enter and Esc and native button events for card assignment and confirmation. It wrote to a GUID-scoped test profile and removed that test file after the readback. The build used the existing uncommitted W1–W2 work.

## W4 · Presentation, HUD and playable acceptance

- [x] Author the weapon meshes, icons and shot sounds and fill the `Presentation` fields. The presenter and the native layout of `WBP_CombatHUD` bind to the current hero. A discharge emits one shot cue and groups repeated contacts. The route has authored physical materials.
- [x] Cover event counts and target grouping in `Tests/WeaponFeedbackTests.cpp` and update the feedback and Camp probes. Run the project automation suite and the Camp interaction, round trip, and travel recovery probes. [Camp acceptance](../CampPrototype/Acceptance.md)
- [x] Verify the ordinary profile in PIE and package Win64 Development with the current four maps. On the playtest profile, equip all three weapons, fire each on the route, return to Camp and restart. Verify the cooked asset references.
- [x] Compare the three shot sounds and inspect readability during mixed combat in a focused game viewport. The user confirmed the W4 manual check on 2026-11-27. [GS-027](../../OpenItems/GS-027-ShotFeedbackPlaytest.md)
- [x] Record the build command, test names, pass counts, packaged round trips and pending manual checks. Update [current architecture](../Architecture.md), mechanics status and open items. Write the [project report](../../../Codex/WorkReports.md).

### W4 execution record · 2026-11-27

`DT_Weapons` points to three held meshes under `/Game/Art/Meshes/Weapons`, three icons, and three distinct shot sounds. The pistol reuses its icon and `SW_PistolShot`. `SC_WeaponShots` gives the weapon sounds eight voices. `CombatPrototypeRoute` assigns the Stone, Metal, and Wood contact material to the safe wall, Quiet room door and first room cover, respectively. The active HUD slot uses a diamond and frame. Empty positions remain visible.

| Check | Result and evidence under `GirlSavior/Saved/W4/` |
| --- | --- |
| Win64 Development Editor | Build passed. `BuildVerified.log`. A fresh commandlet verified the saved table, presenter sound bindings and all three route collision materials. `VerifyAssets.log`. |
| `Automation RunTests GirlSavior.` | 25/25, including `GirlSavior.Weapons.Feedback.GroupedCues`. `Automation.log`. |
| Weapon combat PIE | 18/18. Verified numeric input, safe pocket, shotgun pattern, continuity of the shared cadence, rifle spread and teardown. Five pellets stopped at spawn against one blocker produced one geometry cue. The shotgun and rifle HUD renders show the corresponding active slot. `CombatPIE.log`, `ShotgunHUD00000.png`, `RifleHUD00000.png`. |
| Ordinary-profile regression PIE | Demo 37/37 plus lifetime, first fight 25/25 with the pistol alone, Camp interaction 52/52, three Camp round trips 88/88 on an isolated repeat, default recovery 32/32, Direct 4/4, Fallback 12/12 and StackedBarrier 14/14. The first Camp pass had one late interval while the packaged build was active. The isolated repeat passed all nine interval samples. `RegressionPIE.log`, `RegressionRepeat.log`. |
| Enemy and impact regression PIE | Shooter 1/1, projectile contacts 1/1, barrier depletion 1/1. Health, geometry and ward cues appeared at contact. Free flight expired silently. `EnemyProjectilePIE.log`. |
| Win64 Development package | `BUILD SUCCESSFUL`. Two game processes passed the Camp → expedition → Camp flow with `Shotgun / Pistol / AssaultRifle`. `FullWrite` recorded 53 checks and returned after victory. `FullRead` restored the same order from disk, recorded 46 checks and returned after defeat. The probe sent keys `1`–`3` and LMB through the game viewport and verified the committed discharge and HUD icons in each slot. `BuildCookRun.log`, `PackageFullWrite.log`, `PackageFullRead.log`. |

Build and test from the workspace root. Replace `<guid>` with the same new 32-digit hexadecimal ID in both launches. The test profile has its own save namespace. For a manual playtest, omit `-GirlSaviorWeaponTestProfile` and `-ExecCmds`.

```powershell
RunUAT.bat BuildCookRun -project=GirlSavior/GirlSavior.uproject -target=GirlSavior -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -package -archive -archivedirectory=GirlSavior/Saved/W4/Win64Development '-map=/Game/Maps/Demo+/Game/Maps/TestMaps/CombatPrototype+/Game/Maps/TestMaps/CombatPrototypeRoute+/Game/Maps/TestMaps/CampPrototype' -skipdeploy -unattended -nop4 -utf8output
GirlSavior.exe /Game/Maps/TestMaps/CampPrototype -GirlSaviorWeaponPlaytest -GirlSaviorWeaponTestProfile=<guid> '-ExecCmds=GirlSavior.TestWeaponLoadout FullWrite' -windowed -ResX=1280 -ResY=720
GirlSavior.exe /Game/Maps/TestMaps/CampPrototype -GirlSaviorWeaponPlaytest -GirlSaviorWeaponTestProfile=<guid> '-ExecCmds=GirlSavior.TestWeaponLoadout FullRead' -windowed -ResX=1280 -ResY=720
```

## Acceptance matrix

| ID | Setup and action | Expected result | Evidence |
| --- | --- | --- | --- |
| W01 | Resolve the cooked table, use a fresh normal profile, try a locked card. Confirm a loadout with only slot 2 populated. | Three valid catalog rows. Fresh defaults to pistol in slot 1. Locked cards cannot be equipped. A deliberate gap persists and the first occupied slot equips at launch. | Definition/profile automation and PIE |
| W02 | Open the weapon page from missions and preparation. Reorder via replace, move and swap. Close via Back, Esc and Confirm. Try a failed write and recover. Restart the process, then return from both route outcomes. | Each exit restores focus to its opening view. Unique ordered slots, cancel discards changes, failed writes retain the old confirmed record, successful confirmation survives restart and both outcomes. The support draft follows its existing lifecycle. | Profile automation, PIE and packaged game |
| W03 | Double launch, force pending travel failure, reject a token, inject an invalid loaded row, and start combat directly. | One trusted launch. Recovery returns to Camp and leaves confirmed weapons intact. Bad configurations cannot partially arm the hero. Direct start uses pistol and Retry. | Session automation and Camp recovery PIE |
| W04 | Fire a ready tap, hold LMB, tap before ready, pause, dash, move across doors and enter safe areas. Switch while holding or on the same tick as a ready click. Attempt an empty, same or invalid slot. | Input follows the current fire rules. A valid switch cancels the press, retains the deadline and requires a new edge. Ignored switches keep the original series. No catch-up bursts or extra discharges. | Schedule automation and focused PIE |
| W05 | Fire into a wide target, a group and cover at close/medium/far distances. Measure one projectile against a target and also trace when empty. | Pistol 10 per hit through 1400 cm. Shotgun one discharge per 0.70 s, 5 fixed offsets, 20 per pellet through 300 cm of travel, 10 at 550 cm, zero at 800 cm. Three full-damage pellets deal 60. Rifle 5.5 through 700 cm, 2.75 at 950 cm, zero at 1200 cm, 0.125 s intervals. Intervals are minimums, since the post-aim tick may quantize timing. Each first valid blocker stops one projectile. | Pure math/fixture automation, PIE and cooked smoke |
| W06 | Inject rifle stream seed. Fire four shots, wait for exactly the game-time threshold in automation, then repeat. Repeat with a world pause, and switch away and back briefly and after a long gap. Fire into a closed door from touching range. | Samples stay inside 1° on shots 1–3, inside 4° starting at shot 4. The threshold resets burst logic and waiting in pause does not count. Cover creates a consumed shot with local feedback and no damage past the door. | Combat automation and PIE |
| W07 | Fire from each weapon with Liora then Ida, and exercise Saira's current barrier path. Switch between two discharges, destroy/rebind the hero and finish a run. | Liora uses eligible committed damage and refreshes without stacking. Ida changes future cadence for each weapon. Saira works as before. Already launched damage/trails retain their weapon identity. No effects or mappings leak to Camp. | Support automation and Camp PIE regression |
| W08 | Compare identical targets with all three. Shoot a barrier, a mixed barrier/body target, two enemies, a wall, a ward and an empty corridor. Repeat muted and during a warning, with an active Torch. | Three recognizable shot rhythms, silhouettes, kicks and traces. One muzzle cue on a shotgun discharge, one cue per target/result category per group, lethal wins over repeat suppression. Miss fades silently, impact stays in the right position, and warnings and aim remain readable. | Cue automation plus manual PIE listening and visual review |
| W09 | On the playtest profile, equip `Shotgun / Pistol / AssaultRifle`. Launch from Camp, press `1`, `2`, `3`, shoot after each switch and return. | Each press updates the active slot indicator and changes the next shot. Return and process restart restore exactly the confirmed slots. The normal profile has no playtest grants. | Packaged acceptance through actual player input |
| W10 | With the shotgun, fire a volley whose five pellets stop synchronously on spawn against the same blocker. | One accepted discharge with one muzzle cue and one geometry cue for that blocker. Five completion callbacks cannot clear the deduplication key during spawn. | Feedback automation and point-blank PIE |

The full design checks for discovery in the rescue routes, the extraction result and actual campaign checkpoint recovery remain on [GS-017](../../OpenItems/GS-017-WeaponPlaytest.md) and [GS-022](../../OpenItems/GS-022-SaveProgressPlaytest.md). Weapon presentation acceptance is tracked in [GS-027](../../OpenItems/GS-027-ShotFeedbackPlaytest.md). The implementation must distinguish these future checks from the all-three-weapons test on the current route.
