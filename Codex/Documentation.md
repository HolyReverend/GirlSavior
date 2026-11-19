# Documentation rules

Write short Markdown files with one topic each. Give each mechanic whose rules can be used independently its own file. Use the [documentation index](../Docs/README.md) to locate an unfamiliar document. For a known task, open its owning document or section directly.

| Location or content | Language |
| --- | --- |
| Game design and open-item cards | Plain Russian |
| Technical documentation and Codex instructions | Clear English |
| Work reports and Git messages | Follow the task-specific guide |

Keep one owner for each kind of information.

- The game concept owns the premise and core loop. Mechanic files own player-facing rules and design tuning.
- Technical stage documents own implementation steps, technical tuning choices, and checks for that stage.
- `Architecture.md` owns the current code/asset map and shared integration contracts.
- The prototype plan owns stage order, scope, and planned responsibilities.
- Codex guides own workflow and task-specific conventions.

Use links for dependencies between topics and short summaries when needed to understand the rule. Update the owning document when a decision changes. Label proposals and tuning targets, and date historical observations. Use the [open-item workflow](OpenItems.md) to record unresolved decisions and verification gaps that matter to future work.

Keep general task routes in `AGENTS.md` and subject-specific document selection in the documentation index.
