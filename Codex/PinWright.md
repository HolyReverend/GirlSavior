# PinWright

Read this guide for live editor work or PinWright setup and troubleshooting. Verify the editor identity before changing assets or levels.

- Installed from tag `v1.0.0` at commit `20de4b5d6f18e9839aca17cc732fd6ef0a54a260`. [Plugin descriptor](../GirlSavior/Plugins/PinWright/PinWright.uplugin) · [Release](https://github.com/PinWright/pinwright-ue/releases/tag/v1.0.0)
- The project enables PinWright for the Editor target. The previous `MCPClientToolset`, `AllToolsets`, and `AIModuleToolset` entries are disabled in [GirlSavior.uproject](../GirlSavior/GirlSavior.uproject).
- The Editor-only GameFeatures dependency uses a `GameFeatureData` Asset Manager rule with `NeverCook`. [DefaultGame.ini](../GirlSavior/Config/DefaultGame.ini)
- Open Codex at the trusted `GirlSaviorProject` workspace root. Set up a local `.codex/config.toml` to start the plugin's stdio proxy with Unreal's Python. The editor writes the local port and authentication files under `GirlSavior/Saved/PinWright/`. Keep their contents out of logs and documentation.
- After adding or changing the config, restart the Codex session and open GirlSavior in Unreal Editor. Read `GirlSavior/Saved/PinWright/wiki/index.md`, then only the relevant method pages. Run `system.identity` and check the project path before the first edit. If the gateway starts before the editor is ready, wait for PinWright to report readiness in the editor log and retry.

For installation or updates, follow the [PinWright agent install guide](https://github.com/PinWright/pinwright-ue/blob/master/AGENT_INSTALL.md).
