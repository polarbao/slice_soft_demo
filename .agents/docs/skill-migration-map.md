# Skill Migration Map

This package adapts the previous PrintSolution-style project rules to `slice_soft_demo`.

## Previous capability -> new slice skill

```text
PRINTSOLUTION_AI_SKILL_MASTER.md -> .agents/docs/SLICE_AI_SKILL_MASTER.md
rules.md -> .agents/AGENTS.md + .agents/docs/code-standards.md
Architecture guardrails -> slice-architecture-guardrails
Build/test workflow -> slice-build
Code review -> slice-code-review
Doc state resolution -> slice-doc-state-resolver
Context handoff -> slice-context-handoff
Chat save -> slice-chat-save
Feature implementation workflow -> slice-dev-workflow
Slicing-specific strategy decisions -> slice-slicing-strategy
Qt debug UI work -> slice-ui-debug
R0/R1/R2 refactor work -> slice-r0-r1-refactor
Report/regression/test work -> slice-report-regression
```

## Important adaptations

- PrintSolution SDK/board-specific rules were replaced by slice pipeline/RGBWSV/importer/material/support/UI rules.
- Qt boundary remains: UI only.
- Evidence levels A/B/C/D are preserved.
- Current/Target/Historical/Pending state split is preserved.
- ai_workspace context handoff and chat save flow is preserved.
