# Git workflow

Read this guide only when the task requires Git operations. The Git repository is rooted at `GirlSaviorProject/`. The Unreal project is in `GirlSavior/` within the repository.

## Commit messages

Use English for the type and optional scope. Write the summary and body in Russian.

```text
type(scope): описание
type: описание
```

| Type | When to use it |
| --- | --- |
| `feat` | A new gameplay feature |
| `fix` | A bug fix |
| `refactor` | Code restructuring that preserves behavior |
| `docs` | Documentation |
| `build` | Builds, dependencies, or toolchain configuration |
| `ci` | Automated checks and builds |
| `test` | Adding or changing tests |
| `chore` | Repository maintenance or the initial project baseline |

Use the scope to name the affected system, such as `player`, `camera`, `interaction`, `light`, `ui`, or `project`. Omit it if it does not help explain the change.

- Keep each commit focused on one complete change. Commit related C++ code and assets together.
- Aim for a subject line of 72 characters or fewer. Leave off the trailing period.
- Start the summary with an action verb such as «добавить», «исправить», or «настроить».
- Separate the body from the subject with a blank line. Use it to explain the reasons, relevant constraints, and significant checks performed.
- Leave dates, author names, and the commit hash out of the message. Git records them as metadata.

### Examples

```text
feat(interaction): добавить поворот отражателя по нажатию E
fix(ui): скрывать подсказку после потери цели
refactor(interaction): вынести проверку доступности цели
build: добавить скрипт сборки редактора
docs: уточнить правила работы фонаря
```

## Work reports for commits

Before each commit, check the [work reports](WorkReports.md) for the areas covered by the staged changes. Read relevant reports to write a more precise commit message, and verify their details against the staged diff. Exclude reports whose filename ends in `_commited.md` from searches and reads.

After the commit succeeds, rename every report that supplied information for its message by adding `_commited` before `.md`. For example, `Report_Codex_2026_11_06_9_30_UpdateGitWorkflow.md` becomes `Report_Codex_2026_11_06_9_30_UpdateGitWorkflow_commited.md`. Leave unused reports under their original names.
