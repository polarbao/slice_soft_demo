---
name: slice-r0-r1-refactor
description: Use for R0/R1/R2 formal refactor planning and execution in slice_soft_demo, especially model.cpp/slicer.cpp decomposition and module-boundary work.
---

# Slice R0/R1 Refactor

Read:

- `docs/slicer/REPORT_R0_正式项目架构审查与重构设计当前状态.md`
- `docs/slicer/DOC_DECISION_R1_R0后进入核心模块边界重构阶段.md`
- `docs/slicer/DEV_R1_核心模块边界重构设计.md`
- `docs/slicer/TASKS_R1_核心模块边界重构任务清单.md`
- `.agents/docs/r0-r1-roadmap.md`

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
