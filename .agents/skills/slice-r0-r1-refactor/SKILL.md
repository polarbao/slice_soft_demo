---
name: slice-r0-r1-refactor
description: Use for R0/R1/R2 formal refactor planning and execution in slice_soft_demo, especially model.cpp/slicer.cpp decomposition and module-boundary work.
---

# Slice R0/R1 Refactor

Read:

- `docs/archive/2026-06-30_slicer_legacy/reports/REPORT_R0_正式项目架构审查与重构设计当前状态.md`
- `docs/archive/2026-06-30_slicer_legacy/decisions/DOC_DECISION_R1_R0后进入核心模块边界重构阶段.md`
- `docs/archive/2026-06-30_slicer_legacy/dev/DEV_R1_核心模块边界重构设计.md`
- `docs/codex_task/archive/completed_tasks/TASKS_R1_核心模块边界重构任务清单.md`
- `.agents/docs/r0-r1-roadmap.md`

These documents are historical C-level evidence unless current code or `docs/slice` promotes a decision.

R1 rules:

```text
wrap first
move later
rewrite last
```

Do:

- Establish module directories.
- Create wrapper APIs.
- Move responsibilities in small steps.
- Preserve current behavior.
- Run quick regression after each meaningful step.

Do not:

- Add large features.
- Implement SurfaceShell texture.
- Implement CompensatedShrink varnish.
- Change RGBWSV protocol.
- Rewrite parsers in one pass.
