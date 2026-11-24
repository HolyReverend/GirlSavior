# Camp scene and preparation

The Camp map contains the walkable hub. The local controller owns the interaction target, active panel, and preparation draft. These objects live in the Camp world. [Plan](ImplementationPlan.md) · [Camp interaction design](../../GameDesign/Mechanics/Camp/CampInteraction.md) · [Expedition table design](../../GameDesign/Mechanics/Camp/ExpeditionBoard.md)

## Create the map in Unreal Editor

1. Create and save `/Game/Maps/TestMaps/CampPrototype`. Block out a flat floor of about 22 × 16 m with simple perimeter collision. Use a Directional Light and Sky Light or an equivalent readable lighting setup. Place one `PlayerStart` roughly 3 m from the table with a direct path and clear spawn capsule.
2. Put the campfire proxy near the center, the expedition table on the arrival side, the workshop marker to one side and the shelter marker to the other. Arrange the view so the table and campfire are legible from spawn. Use distinct silhouettes, colors and Russian labels. Keep the workshop and shelter as scenery.
3. Place three instances of `BP_CampHeroine`. Assign `DA_Heroine_Liora` near the table, `DA_Heroine_Ida` near the workshop, and `DA_Heroine_Saira` near the fire. Give the placeholders visibly different colors or shapes. Set their displayed names from the referenced definitions. Keep the interaction anchor outside any blocking mesh and leave space between nearby interaction ranges where possible.
4. Add a native camera room bounds actor with a broad footprint covering the playable floor. Keep the starting point inside the usable camera inset. Verify framing at spawn and on all approaches with `DA_CameraManager`. Place any tall blockers where the view stays clear, or follow the existing foreground fade contract.
5. Create `BP_CampGameMode` from `AGirlSaviorGameMode`. Set it as the map's World Settings → GameMode Override. Assign the existing `BP_Character` and `BP_CampPlayerController`. Make `BP_CampPlayerController` a child of the new native Camp controller. Copy `MoveAction` and the camera manager class from `BP_PlayerController` in Class Defaults. The shared `BP_CameraManager` already uses `DA_CameraManager`. Assign the Camp input and widget assets below. Assign `DA_CampPrototype` on the single `BP_ExpeditionTable` instance.

| Create under `GirlSavior/` | Ownership and wiring |
| --- | --- |
| `Source/GirlSavior/Camp/GirlSaviorCampPlayerController.*` | Derive from `AGirlSaviorPlayerController`. Resolve target, own the draft and modal state, dispatch player actions, and call the expedition subsystem. |
| `Source/GirlSavior/Camp/GirlSaviorCampInteractable.*` | Placeable family base with a query-only sphere, scene anchor, stable `InteractionId`, and virtual `GetPrompt` and `Interact(AGirlSaviorCampPlayerController&)` hooks. Native subclasses `AGirlSaviorExpeditionTable` and `AGirlSaviorCampHeroine` hold `DA_CampPrototype` and a heroine definition respectively. Use a matching header and implementation for each native subclass. |
| `Source/GirlSavior/Camp/GirlSaviorCampPreparation.*` | Value type for the draft and native selection operations. Store mission ID, ordered distinct ability IDs, and validate against the Camp catalog. |
| `Source/GirlSavior/Camp/GirlSaviorCampWidgets.*` | Thin native widget bases with Blueprint presentation events, callable requests, focus handling and a read-only view of the controller state. |
| `Content/Blueprints/Camp/BP_CampGameMode`, `BP_CampPlayerController`, `BP_ExpeditionTable`, `BP_CampHeroine` | Class defaults, simple meshes, labels, interaction anchors and collision spheres. |
| `Content/Blueprints/Camp/UI/WBP_CampPrompt`, `WBP_CampPreparation`, `WBP_CampHeroineInfo` | Prompt, preparation with a `WidgetSwitcher`, and a read-only heroine profile. Create for the owning local player and add through its controller. |
| `Content/Input/Actions/IA_CampInteract`, `Content/Input/IMC_Camp` | Boolean action on E. Duplicate the mappings of `IMC_Gameplay` into the Camp context and add `IA_CampInteract`. Assign `IMC_Camp` to the inherited `GameplayMappingContext` property and bind `Started` with `UEnhancedInputComponent` in `SetupInputComponent`. The base controller owns registration and removal on possession and teardown. |

## Input and interaction contract

- In Camp possession, call `SetFireAllowed(false)`. Add a protected virtual dash admission hook to the base controller with default `true`. Check it at the existing Left Shift press gate and just before `TryStartDash`, and override it to `false` in the Camp controller. Keep the parent's movement, camera, focus and cancellation paths. Share the parent's `GameAndUI` viewport setup between possession and modal close.
- Give each interaction sphere object type `WorldDynamic`, query-only collision, Generate Overlap Events enabled, `Pawn` set to Overlap and other responses set to Ignore. Enable overlap events on the Camp hero capsule after possession and update overlaps if needed. Poll the possessed pawn's overlapping actors at 10 Hz while walking. A candidate must be valid, enabled, within 220 cm of its anchor, in front of the camera and inside the constrained camera view, and unobstructed by a simple Visibility trace from hero chest to the anchor. Ignore the hero and candidate in that trace. Set camp walls and blockers to block Visibility. Use current projection even when the viewport has black bars. Place the anchor and sphere at the same interaction origin and give the sphere at least the authored range so the broad phase cannot hide an eligible target.
- Pick the closest eligible anchor. Break exact ties by `InteractionId` in stable lexical order. Validate unique nonempty map IDs (`ExpeditionTable`, `Liora`, `Ida`, `Saira`). On E, recheck the gameplay input gate and panel state and run the same selection function. Interact when the result matches the current prompt. If it has changed, refresh the prompt and wait for a fresh press. Keep the displayed target as a weak pointer, clear the prompt on modal open or unpossess, and stop the polling timer in teardown.
- Use one unobtrusive prompt, e.g. `E — Стол экспедиций` or `E — Лиора`. The hero profile presents the heroine name, description, and available skill. Close it with `Esc` or `Назад`. The table is the loadout entry point.
- Opening either panel calls `SetGameplayInputAvailable(false)`, clears movement intent through that gate, flushes pressed keys, and calls `StopMovementImmediately()` on the pawn's movement component so inertia cannot slide the hero away. Set visible cursor and `FInputModeUIOnly`, and focus the widget. Configure a focusable root that handles `Esc` via `NativeOnPreviewKeyDown` so focused children share the same back behavior. Keep the world unpaused. Consume `Esc` while a travel request is in progress.
- Closing the final page removes the widget and bindings, flushes input, restores the parent's `GameAndUI` defaults, and restores game viewport focus before reopening gameplay input. Recompute the prompt. Clear modal states, delegates, and timers when the old world or controller ends.

## Preparation flow

| State | Action and transition |
| --- | --- |
| Walking | E on the table opens Missions. Create one draft, preselect the test mission, and seed the selection from the last successful pair only if the entire pair is still valid. Otherwise start with an empty selection. |
| Missions | Show `Боевой маршрут` and `Пройдите тестовый маршрут до выхода`. `Настроить вылазку` opens Preparation. `Esc` or `Назад` closes the table and discards the draft. |
| Preparation | Show the fixed pistol slot, three skill cards, two selected slots, and `Выбрано N из 2`. Clicking a selected card or its remove action clears it. Clicking an unselected card fills the next available slot. With both slots filled, display `Уберите одно умение, чтобы выбрать другое` when a third is attempted. Show the description and formatted effect values on the card details. |
| Back from Preparation | `Esc` or `К выбору вылазки` returns to Missions with the draft intact. Closing the table from Missions discards all draft changes. |
| Launch | `Отправиться` is enabled for a valid mission and two different valid ability IDs. Revalidate in C++, snapshot the current selection, then lock all panel commands while the subsystem attempts travel. If travel fails and the Camp world remains usable, keep the draft and restore focus and the launch action with an error message. [Travel contract](ExpeditionHandoff.md) |

Use localized `FText` for visible strings. Drive card state, names, icons, selection slots, and enabled states from native data. Bind selection and button events once and refresh the view after each state change. Let the widgets render the controller's results, including the localized reason if data or travel validation fails. Pass stable IDs back to the controller for card actions.

## CAMP 2 implementation

The three `WBP_Camp*` assets use native UMG prototype layouts, following the combat fixture's widget pattern. A Blueprint can provide its own root widget, read the current mission, offers and selection, and implement the presentation events. The native controller owns the selection rules in either layout.

CAMP 3 wires `Отправиться` to the expedition session. The action revalidates the draft and reports loading while travel is pending. The table starts a fresh visit from the most recently committed pair when both offers are still valid. [Expedition handoff](ExpeditionHandoff.md)
