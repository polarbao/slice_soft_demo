# <PROJECT_NAME> Codex Instructions

## Project identity

- Project: `<PROJECT_NAME>`
- Repository: `<REPOSITORY_URL_OR_NAME>`
- Main branch/ref: `<CURRENT_BRANCH_OR_REF>`
- Main application path: `<MAIN_APP_PATH>`
- Tech stack: `<TECH_STACK>`
- Build system: `<BUILD_SYSTEM>`
- Test command: `<TEST_COMMAND>`

## Always-on rules

- 当前实现主线是 `<MAIN_APP_PATH>`，不是 `<DEPRECATED_PATHS>`。
- 回答使用中文，除非用户明确要求英文。
- 修改前必须读取相关源码和文档，不得仅根据文件名猜测。
- 不虚构构建、测试、运行、部署或硬件验证结果。
- 破坏性操作、依赖变更、架构迁移、生产路径变更前必须先给方案并等待确认。

## Evidence classification

- A：当前代码/配置/测试中已存在，可作为编码依据。
- B：正式设计/PRD/ADR，可作为目标方向，但不能证明已实现。
- C：历史文档/旧 Demo/旧方案，仅作背景。
- D：废弃或冲突资料，不作为实现依据。

## Skill routing

- 开发需求、方案设计、任务拆解：`$project-dev-workflow`
- 代码审查、diff/PR 检查：`$project-code-review`
- 架构边界、模块职责、ADR：`$project-architecture-guardrails`
- 构建、依赖、部署：`$project-build`
- 文档新旧冲突、实现状态判断：`$project-doc-state-resolver`
- 跨模型/跨 IDE/新会话交接：`$project-context-handoff`
- 保存/归档当前 AI 会话：`$project-chat-save`

## Reference docs

- 项目画像：`.agents/docs/project-profile.md`
- 架构边界：`.agents/docs/architecture-boundary.md`
- 构建与测试：`.agents/docs/build-and-test.md`
- 代码规范：`.agents/docs/code-standards.md`
- 文档状态：`.agents/docs/doc-state.md`
- 会话归档：`.agents/docs/chat-save.md`


# AGENTS.md

## Project context

This repository is polarbao/slice_soft_demo.

Current baseline branch:
- spike/09B-R3-shell-production-readiness

Current phase:
- 09B-R3 has completed production-readiness pre-admission diagnostics.
- Next phase is 09P: OpenVDB surface shell texture experimental production pipeline integration.

## Hard rules

1. Execute only the task explicitly requested by the user.
2. Do not execute the next task unless the user explicitly asks.
3. Before starting each task, verify the working tree is clean:
   - git status --short
4. After finishing each minimal task, run the task-specific validation commands.
5. Before committing, always run:
   - git status --short
   - git diff --check
6. Commit after every completed minimal task.
7. Each commit must contain only changes for the current task.
8. Do not mix unrelated documentation, code, test, script, or formatting changes.
9. If validation fails, fix it before committing. If it cannot be fixed, stop and report the failure.
10. Do not push unless explicitly instructed.

## Production safety rules

1. Do not enable OpenVDB by default.
2. Do not make OpenVDB a mandatory dependency for all builds.
3. Do not replace the legacy slicer_cli production path.
4. Do not write production RGBWSV TIFF from the experimental OpenVDB path unless a later task explicitly allows it.
5. Do not modify the p0.rgbwsv.2 production package protocol.
6. Do not modify RGBWSV channel order.
7. Do not modify uint8 bit depth.
8. Do not modify black_is_print polarity.
9. Do not treat warn_and_attempt output as production-safe.
10. confirmed self-intersection must fail fast.
11. non-manifold, duplicate/opposite duplicate, and local winding issues must block strict production admission.

## Expected workflow per task

For every task:

```powershell
git status --short