# Work reports

At the end of each task, create a short Markdown report for each area it affected. For work that only inspected or verified something, report under the area inspected.

| Area | Folder from the workspace root | WorkType |
| --- | --- | --- |
| Unreal project code, assets, configuration, and builds | `GirlSavior/Reports/` | `Project` |
| Game design and technical documentation | `Docs/Reports/` | `Doc` |
| Codex instructions and workflow, including root `AGENTS.md` | `Codex/Reports/` | `Codex` |

Use `Report_<WorkType>_<yyyy_MM_dd_H_mm>_<JobName>.md` with the local task completion time in Europe/Moscow. Use a short English PascalCase job name and an hour without a leading zero. Example `Report_Doc_2026_06_11_8_22_DesignAddMovement.md`. If the name already exists, use a more specific job name and create a new file.

Record what was done, which files or assets changed, and the checks actually run with their results. Note any outstanding work. Write game design reports in plain Russian and other reports in clear English. Report writing belongs to the task it describes.

Exclude report folders from routine searches and context gathering. Read or search reports only when the user explicitly asks or when preparing a commit under the [Git workflow](Git.md). Apply that guide's exclusion for reports already marked as used.
