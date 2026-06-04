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
