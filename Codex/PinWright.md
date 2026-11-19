# PinWright archive

PinWright is disabled for the Unreal MCP trial. Use the [Unreal MCP guide](UnrealMCP.md) for live editor work. The vendored files and prior local settings are available for a future switch back.

- Vendored from tag `v1.0.0` at commit `20de4b5d6f18e9839aca17cc732fd6ef0a54a260`, with local fixes for Unreal Engine 5.9 Preview covering Cascade, Control Rig, RigVM, and integer formatting. Review those fixes when updating the plugin. [Plugin descriptor](../GirlSavior/Plugins/PinWright/PinWright.uplugin) · [Upstream release](https://github.com/PinWright/pinwright-ue/releases/tag/v1.0.0)
- Local snapshots are under `Archive/PinWright/`. They contain the former workspace MCP config, project descriptor, and Editor-only `GameFeatureData` Asset Manager rule. The editor generates gateway credentials when it starts.
- To switch back, close the editor and restore the former MCP and PinWright plugin entries from `GirlSavior.uproject.json`. Restore the rule from `DefaultGame-PinWright.ini`. For a PinWright-only restart, remove the trial port and set `bAutoStartServer=False` in the native MCP section of `DefaultEditorPerProjectUserSettings.ini`. Check for a conflicting override in `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`. The engine enables native MCP by default.
- Replace the active Unreal MCP table in the root `.codex/config.toml` with the archived PinWright table. Keep unrelated config entries. Use the installed engine's Python path if the archived one is unavailable. Rebuild the Editor target and reload Codex after opening GirlSavior.
- For verification, read `GirlSavior/Saved/PinWright/wiki/index.md` and run `system.identity` against the expected project before editing.

For installation or updates, follow the [PinWright agent install guide](https://github.com/PinWright/pinwright-ue/blob/master/AGENT_INSTALL.md).
