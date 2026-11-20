# Unreal MCP

GirlSavior uses Epic's Unreal MCP in the Unreal Engine 5.9 Editor. The project explicitly enables `ModelContextProtocol`, `EditorToolset`, `BlueprintToolsets`, `AutomationTestToolset`, and `ConsoleToolset` for the Editor target. The built-in `PythonScriptingTools` comes from `EditorToolset`. Its dependency on `ToolsetRegistry` brings in `PythonScriptPlugin` for Editor. [Project descriptor](../GirlSavior/GirlSavior.uproject)

UE 5.9 adds Blueprint graph scripting, Python APIs for Chooser, StateTree, and Dataflow, and toolsets for areas such as MVVM, Slate inspection, scene capture, performance, Texture Graph, and widget animation. The project explicitly disables `AllToolsets`. Use the live catalog to see which tools the enabled plugins and their dependencies expose. If a task needs an unavailable toolset, identify its plugin and check its dependencies before enabling it for the Editor target. `MCPClientToolset` serves connections initiated by Unreal to other MCP servers. [UE 5.9 release notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-9-release-notes)

## Connect Codex

1. Open `GirlSavior.uproject` through Windows and let Unreal finish loading Demo. The project starts its MCP server on the local port `18059`. [Editor settings](../GirlSavior/Config/DefaultEditorPerProjectUserSettings.ini)
2. Open Codex at the trusted `GirlSaviorProject` root. Use this entry in the root `.codex/config.toml` and reload Codex after changing it.

   ```toml
   [mcp_servers.unreal-mcp]
   url = "http://127.0.0.1:18059/mcp"
   startup_timeout_sec = 15
   tool_timeout_sec = 120
   ```

3. Use the single workspace connection. If a Codex chat still shows the previous PinWright tools or was started with the editor closed, open a fresh connection after the editor starts. The endpoint is local to this desktop.

## Discover tools and verify the editor

- Start with `list_toolsets`. Use `describe_toolsets` with `toolset_names` to find tools in a group, then `describe_tools` with `tool_names` to get the inputs for the exact tools you need. Dispatch via `call_tool` with `toolset_name`, `tool_name`, and `arguments`. Use the live names and schemas for the installed build.
- Verify the editor instance before editing. MCP initialization metadata under `com.epicgames.unrealengine/instance` includes `projectFile`, `engineChangelist`, and `processId`. If the client hides it, verify the project's editor process and MCP startup log locally. For a read-only check, dispatch `EditorToolset.LevelToolset.GetCurrentLevel`. The Demo package is `/Game/Maps/Demo`.
- For unfamiliar Unreal work, check the available engine skills with `ToolsetRegistry.AgentSkillToolset.ListSkills` and read any skill applicable to the requested work.

The live GirlSavior catalog on CL 60738873 includes Blueprint graph editing and diff tools, `UMGEditor.WidgetBlueprintEditorSubsystem`, and several Sequencer scripting toolsets. Discover the exact operation before choosing a script or enabling another plugin.

## Choose the execution method

| Task | Route |
| --- | --- |
| Inspect a level, actor, or asset, edit a Blueprint, collect logs, or run a test | Use the focused MCP tool when it covers the operation. Check the returned status and read back the affected state. |
| Filter a large result, apply the same edit to a known set of assets, or use an editor API without a suitable focused tool | Use a short `PythonScriptingTools.execute_python` script. Compose existing tools inside it with `await call_tool(full_name, **arguments)` when useful. Return only the fields needed for verification. |
| Repeat a stable operation across tasks | First reuse the project's existing scripts. If multiple tasks need the same operation and schema, consider an Editor-only Python toolset in a plugin. Use an Unreal Agent Skill for reusable instructions. [Epic's Python and toolset guide](https://dev.epicgames.com/documentation/unreal-engine/using-python-with-ai-agents-in-unreal-engine) |

### Run editor Python through MCP

1. Discover `editor_toolset.toolsets.python_scripting.PythonScriptingTools` and call its `read_me` once per conversation before running Python. Use `search_symbols` to find APIs and registered tools in the running editor and `get_symbol` to verify signatures and return fields. These three discovery tools and `execute_python` are the complete Python toolset on CL 60738873. Recheck the schema after engine updates. Epic's Python guide also lists `grep_stubs`.
2. Pass Python source in `arguments.script.content` with `arguments.script.language` set to `Python`. Place imports at module level and avoid wildcard imports. Each call has a fresh namespace with `unreal` already available. End with a short string, list, or dictionary. `execute_python` returns the final expression as text in `returnValue`, encoding lists and dictionaries as JSON. `print` writes to the editor's `LogPython` category.
3. `execute_python` runs with Execute access in the editor process and has full interpreter access. Keep the script within the requested operation. For bulk edits, read the target paths and current values first, establish a recovery point, then apply changes in small batches. Serialize changes to the same state. After an error or timeout, read back the affected packages before retrying because a batch can stop after applying some edits.

For a read-only smoke check, pass this source to `execute_python` after discovering `EditorToolset.LevelToolset.GetCurrentLevel`. On the Demo map, the result contains `/Game/Maps/Demo`.

```python
await call_tool("EditorToolset.LevelToolset.GetCurrentLevel")
```

## Work in the editor

- Inspect the current actor or asset before making a change. Recheck after each dependent call. Compile changed Blueprints and save only the approved packages after verification.
- Use `EditorToolset.EditorAppToolset` for `IsPIERunning`, `StartPIE`, and `StopPIE`. Check PIE state before and after the session. Keep edits to an open map outside PIE. For the game's manual controls, use the acceptance steps in the relevant stage document.
- Use `AutomationTestToolset.AutomationTestToolset`. Call `DiscoverTests`, then `ListTests` with `nameFilter = GirlSavior.`. Pass the matching names to `RunTests` and check the pass, fail, and skip counts in the result or `GetTestResults`.
- For a project probe such as `GirlSavior.TestCamera`, start PIE if the probe requires it, then dispatch the documented command through `ConsoleToolset.ConsoleToolset.RunConsoleCommand`. Read the result through `ConsoleToolset.LogsToolset.GetLogEntries` or the probe's result file. A console response may contain log categories only. End the PIE session you started after the check.
- For an existing Editor Python probe, inspect the script and follow the invocation and timing in the stage document. Verify which world it uses, what it modifies, and how it reports completion. The camera stage documents the live probe and the fixture setup that runs in a project copy. [Camera helpers](../Docs/Technical/Prototype%231/Stage02-GameplayCamera.md#manual-acceptance-route)

## Build and recover

Editor C++ builds use the installed UnrealBuildTool. Close this project's editor before replacing its binaries, finish the build, then reopen the project and confirm a live MCP read.

```powershell
$project = (Resolve-Path 'GirlSavior/GirlSavior.uproject').Path
$build = 'C:/Program Files/Epic Games/UE_5.9/Engine/Build/BatchFiles/Build.bat'
& $build GirlSaviorEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
```

The server can answer initialization and `tools/list` while the editor is still loading. If a tool returns `Not run` because the editor is running a slow task, wait for the task to finish and retry the read-only check.

If the tools are missing, check the MCP startup lines in `GirlSavior/Saved/Logs/GirlSavior.log`, verify the endpoint and editor instance, and reconnect Codex. If only Python is missing, check for `PythonScriptPlugin` initialization and `PythonScriptingTools` registration in that log. After enabling a required plugin, restart the editor. After reloading a Python toolset in the running editor, use `ModelContextProtocol.RefreshTools` and refresh the Codex connection if the catalog is stale. Confirm recovery with a read-only call. [Epic's Unreal MCP guide](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)

For an automation-only check with the editor closed, use `UnrealEditor-Cmd.exe` from UE 5.9 with the project path, `-unattended -NullRHI`, `-ExecCmds="Automation RunTests GirlSavior."`, and `-TestExit="Automation Test Queue Empty"`. Verify the completed tests and exit result in the log.

For the archived integration and rollback steps, see the [PinWright record](PinWright.md).
