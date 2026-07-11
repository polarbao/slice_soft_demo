# Workflow Map

## Default Flow

1. Restate the objective and constraints.
2. Run `git status --short` and report unrelated dirty state.
3. Read necessary files and docs.
4. Identify affected boundaries and risks.
5. Implement small changes directly when safe.
6. Pause for confirmation before large or high-risk changes.
7. Run or recommend relevant verification.
8. Summarize changes, verification, and residual risk.

## Task Card Flow

For Codex task-card based work:

1. Read `AGENTS.md`, `.agents/docs/README.md`, and the current task file.
2. Use current task entry: `docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md`.
3. Execute only the requested task card.
4. Use the task card's allowed files, forbidden actions, validation commands, and completion criteria.
5. Do not execute the next task unless the user explicitly asks.

## Routing

- Slice planning: `slice-dev-workflow`
- Slice review: `slice-code-review`
- Slice architecture boundaries / ADRs: `slice-architecture-guardrails`
- Slice build/dependencies/tests: `slice-build`
- Slice report/regression/golden/schema work: `slice-report-regression`
- Slice UI debug work: `slice-ui-debug`
- Slice strategy decisions: `slice-slicing-strategy`
- Slice handoff: `slice-context-handoff`
- Slice doc conflicts: `slice-doc-state-resolver`
- Slice chat save: `slice-chat-save`
- Generic planning fallback: `project-dev-workflow`
- Generic verification fallback: `project-verification`
