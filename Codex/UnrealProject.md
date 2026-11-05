# Unreal project context

- Engine version confirmed in the editor log on 2026-11-05. The project ran on installed Unreal Engine 5.8.4. [GirlSavior.uproject](../GirlSavior/GirlSavior.uproject) records `EngineAssociation` as `5.8`.
- The primary module is `GirlSavior` (`Runtime`). Its [Build.cs](../GirlSavior/Source/GirlSavior/GirlSavior.Build.cs) lists `Core`, `CoreUObject`, `Engine`, `InputCore`, and `EnhancedInput` as public dependencies. The targets are `GirlSavior` (Game) and `GirlSaviorEditor` (Editor).
- [DefaultInput.ini](../GirlSavior/Config/DefaultInput.ini) selects Enhanced Input. [DefaultEngine.ini](../GirlSavior/Config/DefaultEngine.ini) sets the default game map to `/Engine/Maps/Templates/OpenWorld`.
- The project enables `ModelingToolsEditorMode` and `PinWright` for the Editor target. For live editor work, read the [PinWright guide](PinWright.md).
