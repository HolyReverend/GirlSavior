# Weapon expeditions implementation plan

**Goal** — From the Camp table, launch either of two three-room searches, pick up its weapon, and extract to unlock that weapon for the next loadout.

**Architecture** — An expedition catalog lists individual definitions. Each definition orders authored rooms and references a map and a reward. The existing Camp, combat, travel and weapon systems retain their gameplay responsibilities. The route actor supplies room gating and run-local pickup state.

**Stack** — Unreal Engine 5.9, the existing GirlSavior runtime module, C++, UMG, Enhanced Input, Data Assets and the current weapon SaveGame profile.

**Specification** — [Architecture and data](ExpeditionArchitecture.md), [Expedition Table](ExpeditionTable.md), [routes and extraction](RouteAndReward.md).

**Status** — Technical specification dated 2026-11-27. All implementation and acceptance steps below are pending. Follow the repository's whole-stage workflow when implementation starts.

## Global constraints

- Use the project naming rules. New gameplay classes and Blueprints use names without `Prototype`. Existing fixture package names in this plan identify assets already in the project.
- Build on the shared `DT_Weapons`, existing Camp support pair, combat pawn, camera, HUD, fire controller and travel handshake.
- A clean profile starts with the pistol. Gear found in the route becomes owned after successful extraction. Preserve three ordered slots and their exact contents at every save and map boundary.
- Save and test changes to `DA_CampPrototype`, table widget defaults, encounters and configured maps through normal project entry points. Refresh [current architecture](../Architecture.md) after the new wiring is verified.

## E1 — Definitions, resolver and profile write

**Files** — Create `Expeditions/GirlSaviorExpeditionDefinition.*` and `Expeditions/GirlSaviorExpeditionCatalog.*`. Update `Camp/GirlSaviorCampDefinition.*`, `Camp/GirlSaviorCampPreparation.*`, and `Weapons/GirlSaviorWeaponProfileSubsystem.*`. These paths are relative to `GirlSavior/Source/GirlSavior/`.

- [ ] Implement the spec types and the Camp resolver. Ensure shared support validation works with either catalog mode and checks the full pair on launch. Exercise the legacy fallback with existing saved assets before wiring the new catalog.
- [ ] Implement mission selection without mutating support. Call the same mission resolver during launch and when making card snapshots. Write coverage in `Tests/ExpeditionCatalogTests.cpp` under `GirlSavior.Expeditions.Catalog`. Test asset order, duplicate and absent ID rejection, valid siblings beside an invalid asset, wrong row/table/world and compatibility with a null new list. Explicitly check that an assigned empty list does not revive the old mission and switching IDs retains the support draft.
- [ ] Implement the shared profile transaction and `TryUnlockWeapon`. Extend `Tests/WeaponProfileTests.cpp` under `GirlSavior.Weapons.Profile`. Assert default pistol only, failed write leaves ownership and records unchanged, successful unlock survives reload with slots 2 and 3 exact and slot 1 empty, repeat unlock writes no new generation, unknown/invalid row is rejected, and future schema and test storage isolation still work.
- [ ] Build Development Editor and run the two prefixes. Expect zero failures.

## E2 — Travel, scene binding and extraction

**Files** — Update `Expeditions/GirlSaviorExpeditionLaunch.h`, `GirlSaviorExpeditionSessionSubsystem.*`, the combat encounter and wave implementation under `Encounters/`, enemy death under `Enemies/`, `Core/GirlSaviorPlayerController.*`, and the existing combat HUD and outcome widgets. Create the route and point files named in [Route and reward](RouteAndReward.md). Add the expedition interaction input context and update the player controller defaults.

- [ ] Resolve the selected definition before starting travel, retain the validated immutable snapshot and keep the existing world claim and Camp recovery flow. Cover trusted world/run checks, wrong map or token, double request, stale or owned card, two different destination packages and a legacy mission in `Tests/CampExpeditionSessionTests.cpp`. In the PIE recovery probe exercise immediate dispatch failure, asynchronous failure with a surviving origin and an engine fallback world. Assert that surviving-world actions become available again and any committed reward survives all return failure paths.
- [ ] Bind a route to the claimed snapshot, resolve its points/rooms/door, track confirmed kills, and admit interaction through the shared controller. Keep the legacy overlap success branch on legacy maps. Verify E only acts on the active route point with a living hero and an unobstructed path.
- [ ] Process the staged weapon and extraction in the encounter. Present the three meaningful outcomes (empty early exit, weapon extracted, weapon lost) and recoverable write errors with the existing HUD. Add `Tests/WeaponExpeditionTests.cpp` under `GirlSavior.Expeditions.WeaponSearch` for gate progression only on confirmed deaths, duplicate kill or interaction, unopened final exit, same-frame lethal preemption, invalid actor recovery, write failure and retry, and return failure after a successful unlock. Cover direct starts on both new routes through the map check below.
- [ ] Build Development Editor and run `GirlSavior.Expeditions.` plus the existing Camp session and combat regression tests. Use isolated test profile storage wherever a scenario writes. Keep the old recovery probe on a legacy-only test catalog if it depends on exact fixtures. Migrate probes that intentionally use the playable Camp to select by mission ID and verify the new route interactions.
- [ ] Preserve the legacy end-to-end probes in Development. Add an explicit non-Shipping `-GirlSaviorLegacyCombatRoute` option that makes the Camp controller use a transient duplicate of the table's catalog with its expedition-list reference cleared. Hold the duplicate with a transient strong `UPROPERTY` for the table visit and point the existing weak `ActiveCatalog` to it. Keep this independent of profile flags and serialized assets. Run the old Camp round-trip probe with it. Combine it with `-GirlSaviorWeaponPlaytest` and the existing test namespace when running `GirlSavior.TestWeaponLoadout FullWrite/FullRead`. Those probes need the legacy route because the all-weapons profile correctly shows both new searches as completed. Use the standard catalog and an empty isolated profile for new-route acceptance.

## E3 — Table cards and game assets

**Files** — Update `Camp/GirlSaviorCampPlayerController.*` and `Camp/GirlSaviorCampWidgets.*`, `WBP_CampPreparation`, `DA_CampPrototype` and `Config/DefaultGame.ini`. Create the widget, DA, texture and map assets listed in the specification.

- [ ] Build the card view, wire actions to controller and preserve focus through the existing modal. Verify available, completed, unavailable and empty lists, the two-of-three support gate, and a failed launch returning control to the table.
- [ ] Author both maps and their Data Assets from the [blockout recipe](RouteAndReward.md#blockout-recipe). Assemble the ordered list, assign it to Camp, save all packages, then reopen in a fresh Editor session. Verify exact saved asset references and three room IDs per route. Validate the DAs in Content Browser and run Map Check on both maps. Confirm a deliberate broken binding reports an actionable field and the real maps validate with zero errors.
- [ ] Run `Automation RunTests GirlSavior.` and play both routes using normal UI, E, shooting and movement. Run the [acceptance matrix](#acceptance-matrix) first in PIE and then the packaged smoke test. Record actual runs and any deferred checks in the work report and linked open items.

## Verification commands

Use the locally configured UE 5.9 binaries and absolute project path from the workspace. Save logs and the package below `GirlSavior/Saved/WeaponExpeditions/`. Run Data Validation on the feature assets and compare any whole-project issues against the pre-change baseline.

```text
Build.bat GirlSaviorEditor Win64 Development -Project="<absolute-path-to-GirlSavior.uproject>" -WaitMutex
UnrealEditor-Cmd.exe "<absolute-path-to-GirlSavior.uproject>" -run=DataValidation -unattended -NullRHI
Automation RunTests GirlSavior.
RunUAT.bat BuildCookRun -project="<absolute-path-to-GirlSavior.uproject>" -target=GirlSavior -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -package -archive -archivedirectory="<absolute-path-to-Saved/WeaponExpeditions/Win64Development>" -map=/Game/Maps/Demo+/Game/Maps/TestMaps/CombatPrototype+/Game/Maps/TestMaps/CombatPrototypeRoute+/Game/Maps/TestMaps/CampPrototype+/Game/Maps/TestMaps/SupplyVault+/Game/Maps/TestMaps/AbandonedGuardroom -skipdeploy -unattended -nop4 -utf8output
```

The automation line runs in the Editor console. Inspect individual test outcomes and await suite completion. For the packaged smoke test launch the Development executable with `/Game/Maps/TestMaps/CampPrototype`. Use a clean isolated profile, without the all-weapons flag or cheat. A development probe may use an explicit empty test namespace. If the existing namespace requires all-weapons provisioning, extend the development-only storage selection independently of provisioning and assert it cannot write the normal profile. Use the same test namespace after restart to confirm persistence. See the current [Camp](../CampPrototype/Acceptance.md) and [weapon](../Weapons/ImplementationPlan.md#acceptance-matrix) suites for regression entry points.

## Acceptance matrix

| Check | Expected result |
| --- | --- |
| Open the table on a clean profile | Two distinct icon cards with correct names, summaries, enemy roles and expected weapons. Weapon editor shows pistol in slot 1 and slots 2–3 empty. E, Escape, focus and preparation behave as specified. |
| Select the supply mission, then back out and select the guardroom | Selection changes independently of support and weapon drafts. Successful launch reaches the selected world. Verify each mission has exactly three authored rooms and a matching route ID. |
| Walk into either early point and continue | A prompt appears. Extraction happens on E. An empty early exit leaves the card and reward available. The early point remains reachable after picking up the weapon. |
| Fight in either room 2 and try the final exit early | The door blocks movement and attacks until the required three enemies are confirmed dead. The final point cannot extract before pickup. |
| Pick up the shotgun, extract through the final point and return | The weapon is pending before extraction. Completion opens only `Shotgun`, marks only the supply card as completed and leaves slots in their saved positions. |
| Find the rifle, die before extraction, then repeat through the early point | Bypass the room 1 Shooter on the first pass, then backtrack to it and die with the rifle. The result reports the rifle lost and it still spawns next run. The second run opens only the rifle reward and marks that card completed. Verify the rifle final exit as well in a fresh profile. |
| Start another run with the earned weapon | Manually place it in a slot and confirm in Camp. In a new isolated run or before finishing the other search, switch with the matching key and fire. The old loadout stays the same until explicit slot confirmation. |
| Save failure, return failure and restart | Before a successful write, the bag is retryable and the profile is unchanged. After a successful write, a failed trip to Camp can be retried and a full restart retains the weapon and prior slot order without duplicate ownership. |
| Legacy and direct-start behavior | Demo and old fixture maps play as before. The legacy route still supports its existing direct Retry and recovery tests. New maps opened directly show the table-start diagnostic and cannot grant an item. |
| Win64 Development package | Both card textures, worlds, doors, enemies and point references load. Launch and extract both missions through the table with a clean profile. Restart the process and confirm both weapons are owned exactly once. |

The short maps provide stage evidence for [weapon acquisition](../../OpenItems/GS-017-WeaponPlaytest.md), [table interaction](../../OpenItems/GS-019-CampInteractionPlaytest.md), [extraction](../../OpenItems/GS-015-ExpeditionReturnPlaytest.md) and [profile persistence](../../OpenItems/GS-022-SaveProgressPlaytest.md). Final rescue placement, production pacing and full checkpoint restoration have separate closure conditions.
