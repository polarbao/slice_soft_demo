# Slice Soft Demo Codex Instructions

## Project Identity

- Project: `slice_soft_demo`
- Repository: `polarbao/slice_soft_demo`
- Current branch/ref: `feature/12e-08c-mesh-repair` as of 2026-07-21; verify with `git branch --show-current` before each task
- Main implementation paths: `src/slicer_core`, `apps/slicer_cli`, `apps/slicer_debug_ui`
- Formal docs: `docs/slice`
- Codex task docs: `docs/codex_task`
- Archived historical docs: `docs/archive/2026-06-30_slicer_legacy`
- Tech stack: C++20, Qt 5.15 Widgets, CMake, Windows x64/MSVC, optional OpenVDB via vcpkg
- Default test command: `ctest --test-dir build -C Debug --output-on-failure`

## Current Phase

- `12A` material fill, support, and varnish semantics have completed the current P0/P1 scope.
- `12B-R0/R1/R2` performance evaluation and OpenVDB SDF utility positioning are complete.
- `12C-R0` Qt workbench build compatibility and baseline admission is complete.
- `12C-R1` Profile and Settings closure is complete.
- `12C-R0/R1/R2` Qt workbench is complete; final fresh build, UI Smoke, and CTest passed.
- The latest completed task is `12E-08C-R4-08-R2 GO/NO-GO Refresh`.
- `12D-R0/R1/R2/R3` is complete.
- `12E-01/02/03/04/05/06/07`, `12E-08A/08B/08C`, `12E-08C-R1-01..04`, `12E-08C-R2-01..04`, and `12E-08C-R3-01..04` are complete. R3-04 records `NO-GO / FROZEN`. `12E-08C-R4-01..07`, R4-07-R1, R4-07-R2, Quick-CI-R1, and R4-08-R2 are complete; the two-family candidate matrix, versioned reference-machine candidate budget, and current Quick CI are PASS while production admission remains not evaluated. R4-08-R2 is `CONDITIONAL_TECHNICAL_PASS`. aishen/meigui/titian remain a 0/3 complex-relief coverage gap. `12E-09A-01` is complete. Explicit 08D authorization is the only current 08D start blocker.
- `12D-R0` documentation admission is complete and the 12C gate is satisfied.
- Current global_surface_shell remains diagnostic-only; raster mapping and full-material closure evidence do not admit production output. The approved Target State is explicit `slicePipeline.mode=legacy|global_surface_shell`, with legacy as default and both admitted production modes sharing the existing RGBWSV TIFF writer. This target is not implemented and does not authorize `12E-08D`; explicit user confirmation is still required.
- The repair prerequisite must remain explicit and disabled by default. `repair_then_strict` must re-run strict diagnostics; `manual_repair_required` must never count as a production PASS.
- The formal product direction is tracked in `docs/slice`; operational Codex tasks are tracked in `docs/codex_task/current`.

## Always-On Rules

1. Answer in Chinese unless the user explicitly asks for English.
2. Execute only the task explicitly requested by the user; do not start the next task without explicit instruction.
3. Before code, build, dependency, or architecture changes, read the relevant source and project docs first.
4. Do not invent command results, tests, builds, hardware validation, repository state, or implementation status.
5. Before each task, run `git status --short` and report unrelated dirty state instead of overwriting it.
6. Do not revert or delete user changes unless the user explicitly requests that operation.
7. For destructive operations, dependency upgrades, architecture migration, production-path changes, hardware/device control, or git history rewrite, give a plan and wait for confirmation.
8. After a minimal task, run task-specific validation. Before committing, run `git status --short` and `git diff --check`.
9. Commit only when the user asks or when the active task explicitly requires it; do not push unless explicitly instructed.

## Evidence Classification

- A: current code/config/tests/build scripts; safe implementation basis.
- B: formal `docs/slice` PRD/DEV/ADR/decision docs; target direction, not proof of implementation.
- C: archived demo docs, historical reports, chat logs, and completed Codex prompts/tasks; background only.
- D: deprecated or conflicting material; do not use as implementation basis.

When answering implementation-state questions, split the answer into `Current State`, `Target State`, `Historical State`, and `Pending Confirmation` when relevant.

## Skill Routing

- Slice feature planning and staged execution: `$slice-dev-workflow`
- Slice architecture boundaries and ADR/DOC_DECISION work: `$slice-architecture-guardrails`
- Slice build, dependency, CMake, packaging, and CI issues: `$slice-build`
- Slice code review and pre-merge checks: `$slice-code-review`
- Slice document-state conflict resolution: `$slice-doc-state-resolver`
- Slice context handoff: `$slice-context-handoff`
- Slice chat save/archive: `$slice-chat-save`
- Generic C++20/Qt/CMake guidance: `$cpp-coding-standards`
- Generic plan writing or project planning: `$writing-plans` / `$project-planner`

Project-level slice skills and `.agents/docs` facts override generic templates when they conflict.

## Mandatory Reference Docs

- AI collaboration rules: `.agents/AGENTS.md`
- Skill master: `.agents/docs/SLICE_AI_SKILL_MASTER.md`
- Project profile: `.agents/docs/project-profile.md`
- Architecture boundaries: `.agents/docs/architecture-boundary.md`
- Build and test: `.agents/docs/build-and-test.md`
- Code standards: `.agents/docs/code-standards.md`
- Commit style: `.agents/docs/commit-style.md`
- Document state: `.agents/docs/doc-state.md`
- Formal docs index: `docs/slice/README.md`
- Codex task index: `docs/codex_task/README.md`
- Completed 12D task list: `docs/codex_task/current/TASKS_12D_横截面材料无缝闭环任务清单.md`
- Next prepared task list: `docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md`
- Next prepared execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md`
- Prepared 12E task list: `docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md`
- Prepared 12E execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md`
- Prepared 12E repair task list: `docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md`
- Prepared 12E repair execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_08C_真实模型拓扑修复执行指令.md`
- Prepared 12E-R4 task list: `docs/codex_task/current/TASKS_12E_08C_R4_模型导入预检与修复资产准入任务清单.md`
- Prepared 12E-R4 execution prompt: `docs/codex_task/current/CODEX_PROMPT_12E_08C_R4_模型导入预检与修复资产准入执行指令.md`
- Approved 12E dual-mode decision: `docs/slice/DOC/DOC_DECISION_12E_Legacy与GlobalSurfaceShell双切片模式.md`
- Prepared 12E-08D dual-mode production task: `docs/slice/DOC/DOC_PREP_12E_08D_双模式生产写包准备.md`

## Production Safety Rules

1. Do not enable OpenVDB by default.
2. Do not make OpenVDB a mandatory dependency for all builds.
3. Do not replace the legacy `slicer_cli` production path.
4. Do not write production RGBWSV TIFF from the experimental OpenVDB path unless a later task explicitly allows it.
5. Do not modify the `p0.rgbwsv.2` production package protocol.
6. Do not modify RGBWSV channel order.
7. Do not modify uint8 bit depth.
8. Do not modify `black_is_print` polarity.
9. Do not treat `warn_and_attempt` output as production-safe.
10. Confirmed self-intersection must fail fast.
11. Non-manifold, duplicate/opposite duplicate, and local winding issues must block strict production admission.

## Expected Workflow Per Task

For every task:

```powershell
git status --short
```

For documentation/config-only tasks, validate with targeted text/schema checks and `git diff --check`.
For C++/Qt/CMake changes, use the task-specific commands from `.agents/docs/build-and-test.md`.
