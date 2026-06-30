---
name: slice-dev-workflow
description: Use for slice_soft_demo feature planning, implementation design, staged execution, 09P/OpenVDB experimental work, and R0/R1/R2 historical refactor planning. Read project docs before proposing code changes.
---

# Slice Dev Workflow

Before work, read:

1. `.agents/AGENTS.md`
2. `.agents/docs/SLICE_AI_SKILL_MASTER.md`
3. `.agents/docs/project-profile.md`
4. `.agents/docs/architecture-boundary.md`
5. Relevant formal docs in `docs/slice`
6. Relevant active task docs in `docs/codex_task/current`
7. Relevant historical docs in `docs/archive/2026-06-30_slicer_legacy` only as C-level evidence
8. Current source files

## Required output before code changes

```markdown
## Implementation Plan

### Problem Type
### Layer(s) Involved
### Official Documents
### Historical Documents
### AI Workspace Evidence
### Current Code Reality
### Current State
### Target State
### Historical State
### Pending Confirmation
### Risk Points
### Files To Change
### Verification Plan
```

## Workflow

1. Identify target module and evidence level A/B/C/D.
2. State assumptions and missing information.
3. Provide design options and risks.
4. For large changes, stop for confirmation.
5. Execute small tasks one at a time.
6. Run required verification gates.
7. Update stage report if the task changes project state.
