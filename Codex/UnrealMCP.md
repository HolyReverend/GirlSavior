# Unreal MCP

GirlSavior uses Epic's Unreal MCP in the Unreal Engine 5.9 Editor. The project enables `ModelContextProtocol`, `EditorToolset`, `BlueprintToolsets`, `AutomationTestToolset`, and `ConsoleToolset` for Editor. [Project descriptor](../GirlSavior/GirlSavior.uproject)

`AllToolsets` enables the full bundle. Add a specific toolset when a project workflow needs it. `MCPClientToolset` is for connections initiated by Unreal to another MCP server.

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

## Work in the editor

- Start with `list_toolsets`. Inspect the relevant schema with `describe_toolsets` or `describe_tools` and use `call_tool` for editor operations. Use the names reported by the live catalog. For a level check, pass `toolset_name = EditorToolset.LevelToolset` and `tool_name = GetCurrentLevel`.
- Verify the editor instance before editing. The MCP initialization metadata includes the project file, engine build, and editor PID. If the client omits that metadata, verify the project's editor process and MCP startup log locally. Call `GetCurrentLevel` to confirm the working level. For Demo the package is `/Game/Maps/Demo`.
- For unfamiliar Unreal work, check the available engine skills with `ToolsetRegistry.AgentSkillToolset.ListSkills` and read any skill applicable to the requested work.
- Inspect the current actor or asset before making a change. Recheck after each dependent call. Compile changed Blueprints and save only the approved packages after verification.
- Use `EditorToolset.EditorAppToolset` for `IsPIERunning`, `StartPIE`, and `StopPIE`. Check PIE state before and after the session. Keep edits to an open map outside PIE. For the game's manual controls, use the acceptance steps in the relevant stage document.
- Use `AutomationTestToolset.AutomationTestToolset`. Call `DiscoverTests`, then `ListTests` with `nameFilter = GirlSavior.`. Pass the matching names to `RunTests` and check the pass, fail, and skip counts in the result or `GetTestResults`.
- For a project probe such as `GirlSavior.TestCamera`, start PIE if the probe requires it, then dispatch the documented command through `ConsoleToolset.ConsoleToolset.RunConsoleCommand`. Read the result through `ConsoleToolset.LogsToolset.GetLogEntries` or the probe's result file. A console response may contain log categories only. End the PIE session you started after the check.
- Use Unreal Editor Python for the existing Python probes. Before running a stage script, check its prerequisites, which world it reads, and whether it modifies or saves assets. The camera stage documents which scripts use a project copy. [Camera helpers](../Docs/Technical/Prototype%231/Stage02-GameplayCamera.md#manual-acceptance-route)

## Build and recover

Editor C++ builds use the installed UnrealBuildTool. Close this project's editor before replacing its binaries, finish the build, then reopen the project and confirm a live MCP read.

```powershell
$project = (Resolve-Path 'GirlSavior/GirlSavior.uproject').Path
$build = 'C:/Program Files/Epic Games/UE_5.9/Engine/Build/BatchFiles/Build.bat'
& $build GirlSaviorEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
```

If the tools are missing, check the MCP startup lines in `GirlSavior/Saved/Logs/GirlSavior.log`, verify the endpoint and editor instance, and reconnect Codex. For an automation-only check with the editor closed, use `UnrealEditor-Cmd.exe` from UE 5.9 with the project path, `-unattended -NullRHI`, `-ExecCmds="Automation RunTests GirlSavior."`, and `-TestExit="Automation Test Queue Empty"`. Verify the completed tests and exit result in the log.

For the archived integration and rollback steps, see the [PinWright record](PinWright.md).
