# Expedition Table cards

## Screen flow

Keep `E` on `BP_ExpeditionTable`, the existing Camp modal, and `WBP_CampPreparation`. Replace its one-mission view with a list of cards. Keep the current separate weapon editor, preparation page, and Camp focus policy.

```text
Стол экспедиций

┌──────────────────────────────┐  ┌──────────────────────────────┐
│ [иконка] Склад снабжения     │  │ [иконка] Заброшенная караульня │
│ Найдите оружейный ящик       │  │ Доберитесь до стойки с оружием│
│ за дверью склада.            │  │ за караульным залом.          │
│ Преследователь, Стрелок      │  │ Преследователь, Стрелок,       │
│ Награда · Дробовик           │  │ Таранщик                      │
│                              │  │ Награда · Автоматическая     │
│                              │  │ винтовка                     │
└──────────────────────────────┘  └──────────────────────────────┘

Назад                 Набор оружия                 Настроить вылазку
```

| Page or action | Expected behavior |
| --- | --- |
| First open | Display the catalog in asset order. Select and focus the first available card. If no card is available, focus the first visible card and disable preparation. If the list is empty, show «Нет доступных вылазок» and focus «Назад». |
| Click or navigate to a card | Click or keyboard activation selects the card. A selected outline and focus styling are independently visible. Selection alone keeps the page open. Use click and keyboard activation instead of requiring a hover. |
| «Настроить вылазку» | Open preparation for the selected available card. Show the selected title and the read-only three-slot summary. Keep the existing exactly-two support selection and separate «Отправиться» button. |
| «К выбору вылазки» | Restore the card list, selected ID and focus. Retain support draft while the table stays open, including after selecting another mission. |
| «Набор оружия» | Open the current weapon modal from either page. Confirm or cancel and return focus to the button that opened it. Preserve mission and support draft. Refresh the confirmed weapon summary from the profile. |
| «Назад» or Escape | Follow the current Camp navigation: close the weapon modal first, preparation returns to the list, and the list closes to Camp. Closing the table drops the temporary draft. While focused, map input remains suspended. |
| «Отправиться» | Enable only with a valid mission, confirmed loadout and valid support pair. Recheck on invocation. Suppress repeat actions during pending travel. Display a localized error and restore focus if the request fails. |

## Card view and authored widgets

Extend `Camp/GirlSaviorCampWidgets.h/.cpp` with `FGirlSaviorExpeditionCardView` containing `MissionId`, `DisplayName`, `Description`, `LocationIcon`, `EnemyLabels`, `RewardName`, `RewardIcon`, `StatusText` and a card status enum (`Available`, `Completed`, `Unavailable`). Build snapshots in the Camp controller from the catalog resolver and the active weapon profile. Pass a selected ID separately. Use `DT_Weapons` for the reward name and optional icon. Use localized `FText` for visible status, role and button text. For legacy cards show the existing title and description with optional sections hidden.

Create `UGirlSaviorExpeditionCardWidget` in the Camp widget files and `/Game/Blueprints/Camp/UI/WBP_ExpeditionCard` as its presentation child. A card accepts `SetCardView(const FGirlSaviorExpeditionCardView&, bool bSelected)` and emits its MissionId on activation. Feed the list through `UGirlSaviorCampPreparationWidget` and handle activation in `AGirlSaviorCampPlayerController`. Draw both a location icon and a text name even when reward art is present. Keep the existing native fallback layout for probes and repair invalid Blueprint bindings before accepting the cooked asset.

- Put cards in a scrollable area and preserve the three actions in a footer. At 1280×720 show the two cards side by side. Use a single-column layout when the card area is narrower than 720 logical units. Let the localized description and enemy list wrap. Keep the active card visible during keyboard navigation.
- Completed cards stay readable with «Завершена · оружие вынесено». Invalid cards with a valid unique ID can show «Вылазка недоступна» wherever a display title is available. Duplicate or missing IDs never produce selectable card keys. Completed and unavailable cards can be inspected, but cannot start preparation.
- Read reward ownership when opening the table and recheck it when launching. A profile load error must be visible on the table and disables weapon-search launches until the profile is available. Display a reason in the panel and keep navigation responsive. The selected ID is transient. Route title and reward ownership are derived from their respective owners.
- Check mouse activation, keyboard focus, long Russian strings, narrow layout, cancellation, return from the weapon editor and error messaging. [Expedition Table design](../../GameDesign/Mechanics/Camp/ExpeditionBoard.md) · [Camp interaction acceptance](../../OpenItems/GS-019-CampInteractionPlaytest.md)
