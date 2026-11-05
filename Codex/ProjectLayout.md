# Unreal project layout and naming

Read this guide only when creating a folder, file, or asset in `GirlSavior/`. Use the current project layout first. Create folders when the task needs them. Treat these examples as defaults and propose a simpler structure when it fits the feature better.

## Placement

Paths in this table are relative to `GirlSavior/`.

| Resource | Suggested location |
| --- | --- |
| C++ gameplay files | `Source/GirlSavior/`. Group by feature as the module grows. |
| Levels and test maps | `Content/Maps/`, with `TestMaps/` when needed. |
| Blueprint classes and widgets | `Content/Blueprints/`. Use groups such as `Core/`, `Characters/`, `Components/`, `Interfaces/`, or `UI/` as needed. |
| Input actions and mapping contexts | `Content/Input/`, with actions under `Actions/` when useful. |
| Character animation and rigs | `Content/Animations/<Character>/`. Keep clips, rigs, and related data near the character. |
| Shared materials and textures | `Content/Art/Materials/` and `Content/Art/Textures/`. |
| Data assets, tables, and curves | `Content/Data/DataAssets/`, `Content/Data/DataTables/`, or `Content/Curves/`. |

## Asset names

Use English names that describe the subject and purpose. Prefer `<Type>_<Name>[_<Variant>]` for assets with a useful type prefix. Match an established convention in the destination folder.

| Type | Example names |
| --- | --- |
| Blueprint class, component, interface, widget | `BP_Door`, `BPC_Interaction`, `BPI_Interactable`, `WBP_Inventory` |
| Input action and mapping context | `IA_Move`, `IMC_Default` |
| Data asset, table, and curve | `DA_MovementSettings`, `DT_Items`, `C_CameraBlend` |
| Animation sequence, Animation Blueprint, Control Rig | `A_Player_Idle`, `ABP_Player`, `CR_Player_FootIK` |
| Skeletal mesh, skeleton, and physics asset | `SK_Player`, `SKEL_Player`, `PA_Player` |

Reference for the layout and naming examples. [Sands of Silence project architecture](https://docs.google.com/document/d/1EOcxtSSRkHpthlPB0unHX3Rf9FeJ2oEqP5KrYHfinwo)
