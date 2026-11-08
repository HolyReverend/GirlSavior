# Documentation rules

Write short Markdown files with one topic each. Use the [documentation index](../Docs/README.md) when you need to locate a project document.

| Location or content | Language |
| --- | --- |
| Game design in `Docs/GameDesign/` | Plain Russian |
| Technical documentation and Codex instructions | Clear English |
| Work reports and Git messages | Follow the task-specific guide |

Keep one owner for each kind of information.

- Game design mechanics own player-facing behavior and design tuning.
- Technical stage documents own implementation steps, technical tuning choices, and checks for that stage.
- `Architecture.md` owns the current code/asset map and shared integration contracts.
- The prototype plan owns stage order, scope, and planned responsibilities.
- Codex guides own workflow and task-specific conventions.

Use short linked summaries where another topic needs context. Update the owning document when a decision changes. Clearly date historical observations and distinguish them from current instructions. Historical results do not prove a current check passed.

Keep general task routes in `AGENTS.md` and subject-specific document selection in the documentation index.
