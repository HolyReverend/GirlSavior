# Camp 4 acceptance · 2026-11-24

Win64 Development passed one rendered Camp → combat route → Camp round trip with Liora and Ida. The probe used the expedition table to choose the pair, verified the effects on the route, reached the real exit trigger, used `В лагерь`, and reopened the table with both cards in the original order. It moved the hero to controlled positions for combat checks and the exit. Single-player PIE covered all three pairs and both outcomes.

## Build and checks

UE 5.9, CL 60738873. Paths below are relative to the Unreal project folder `GirlSavior/`.

| Check | Command or entry point | Result and evidence |
| --- | --- | --- |
| Win64 Development Editor and game | `RunUAT.bat BuildCookRun -project=<GirlSavior.uproject> -target=GirlSavior -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -package -archive -archivedirectory=<Saved/Camp4/Win64Development> -map=/Game/Maps/Demo+/Game/Maps/TestMaps/CombatPrototype+/Game/Maps/TestMaps/CombatPrototypeRoute+/Game/Maps/TestMaps/CampPrototype -skipdeploy -unattended -nop4 -utf8output` | Both targets built and all four maps cooked. Exit 0 in `Saved/Camp4/BuildCookRun.log`. Rebuilt and staged the Development probe with `-skipcook` against the same cooked content. Exit 0 in `Saved/Camp4/BuildAndRestage.log`. |
| Saved content | `UnrealEditor-Cmd.exe <GirlSavior.uproject> -run=PythonScript -script=<Saved/Camp2/VerifyCamp2Assets.py> -unattended -NullRHI` | Fresh process reopened Camp, its fixtures, input and catalog. Both world references resolved. Map Check reported 0 errors and 0 warnings in `Saved/Camp4/SavedAssets.log`. |
| Native automation | `Automation RunTests GirlSavior.` | 15/15 passed in `Saved/Camp4/EditorAcceptance.log`, including saved Camp data, loadout validation, effect behavior, and session isolation. |
| Camp walk and table | From Camp PIE, `GirlSavior.TestCamp` | 52/52 passed. Spawn, movement, interaction range, line of sight, profiles, prompts, focus and draft. |
| Expedition and effects | From Camp PIE, `GirlSavior.TestCampExpeditions` | 88/88 passed. All three pairs across three round trips. Verified double activation, safe spawn, matching run data, positive hit and slow expiry, shot cadence, barrier recharge, pause timing, absence of the third skill, and remembered order after success and defeat. |
| Travel recovery | From Camp PIE, `GirlSavior.TestCampRecovery`, `GirlSavior.TestCampRecovery Fallback` and `GirlSavior.TestCampRecovery Direct` | 24/24, 9/9 and 4/4 passed. Retried launch and return after dispatch failure, recovered from initialization failure, cleared stale state after engine fallback, and exercised rejected tokens and direct-start Retry. Native validation covers invalid launch inputs. |
| Lethal hit with two barriers | From Camp PIE, `GirlSavior.TestCampRecovery StackedBarrier` | 12/12 passed. A temporary vitals fixture gave the live pawn 4 base barrier and 8 Saira barrier. The lethal hit committed HUD values, emitted the break and defeat cues, published the terminal notice, cleared support and threats, and still allowed return to Camp. |
| Demo regression | From Demo PIE, `GirlSavior.TestCombat` and `GirlSavior.TestDemoLifetime` | 37/37 passed. The lifetime probe also passed, measuring 2502.05 cm at 1.25 s and expiry by 1.65 s. |
| Combat route regression | With the combat route loaded, `Saved/Camp4/RunWavePIE.py` with `Route DefeatRetry` and `Saved/Camp4/RunFeedbackPIE.py` with `Barrier3 Barrier7 ProjectileContacts InputShots Pursuer SuccessRetry` | 8/8 passed. All five route rooms and exit, wave cleanup, real LMB fire and cooldown, projectile contacts, barrier feedback, lethal outcome and clean Retry. The route probe logged 10463.8 cm of swept walking. |
| Packaged round trip | From the package root, run `GirlSavior/Binaries/Win64/GirlSavior.exe /Game/Maps/TestMaps/CampPrototype -windowed -ResX=1280 -ResY=720 -ExecCmds="GirlSavior.TestCampExpeditions"` | 28/28 passed in `Saved/Camp4/PackagedRoundTripVerified.log`. The launch and attachment matched the same run, mission and ordered abilities. The first attempt completed travel with one failed shot-cadence check after the window lost focus. An uninterrupted rerun passed. The first log is `Saved/Camp4/PackagedRoundTrip.log`. |

All PIE rows use `Saved/Camp4/EditorAcceptance.log`. The wave and feedback runners also wrote JSON results in that folder.

## Walkthrough and manual acceptance

The local package is in `Saved/Camp4/Win64Development/`. `Demo` is the default when opening the executable. From the package root, start Camp in PowerShell with `./GirlSavior.exe /Game/Maps/TestMaps/CampPrototype`. This entry point loaded Camp in a separate launch recorded in `Saved/Camp4/PackagedLauncher.log`. The console command in the table above runs the scripted check.

1. Walk with `WASD`, use `E` to talk to Liora, Ida and Saira, and close each profile with `Esc`. Approach the expedition table and check that the prompt matches the object that opens. Compare with `Saved/Camp4/CampWalk.png`.
2. At the table, open the test mission and choose `Настроить вылазку`. Select two cards. Remove one and choose a different skill. `Esc` returns to the missions with the draft preserved. Open preparation again and press `Отправиться`. Compare with `Saved/Camp4/CampPreparation.png`.
3. Start in the safe pocket with full health. Move with `WASD`, aim with the mouse, fire with LMB, and use Left Shift for dash. Reach the exit and use `В лагерь`. Open preparation again. Both confirmed cards should appear in their original slots. The packaged checkpoints are `Saved/Camp4/PackagedSuccess.png` and `Saved/Camp4/PackagedRememberedPair.png`.

Manual review remains open for the clarity of the prompts, labels and preparation screens on a first visit, for visibility across the Camp camera frame, and for how the three pairs feel when playing the route with normal controls. For the defeat walkthrough, use `В лагерь`, confirm the pair is still present, change one card and launch the next run. The technical results above use scripted inputs and controlled encounters. [GS-018](../../OpenItems/GS-018-HeroineSupportPlaytest.md) · [GS-019](../../OpenItems/GS-019-CampInteractionPlaytest.md)
