# DOC_CLASSIFICATION_2026-06-30_docs治理归档清单

> 文档版本：v1.0
> 文档状态：Docs Governance / Archive Classification
> 生成日期：2026-06-30
> 适用范围：`docs` 文档整理、正式 PRD/DEV 入口建立、Codex 任务目录拆分

## 1. 本轮整理结论

当前 `docs/slicer` 中的历史文档已经完成阶段记录使命，应整体从当前入口中移出，归档到：

```text
docs/archive/2026-06-30_slicer_legacy
```

正式 PRD、DEV、路线图和当前 demo 基线后续统一放入：

```text
docs/slice
```

Codex 执行任务、任务清单、提示词和交接文档统一放入：

```text
docs/codex_task
```

## 2. 已归档内容

| 分类 | 目录 | 文件数 | 状态判断 |
|---|---|---:|---|
| 架构阶段文档 | `docs/archive/2026-06-30_slicer_legacy/architecture` | 6 | 已完成，归档 |
| 网页聊天导出 | `docs/archive/2026-06-30_slicer_legacy/chat_exports` | 1 | 背景资料，归档 |
| 检查清单 | `docs/archive/2026-06-30_slicer_legacy/checklists` | 3 | 已完成或阶段性参考，归档 |
| 决策记录 | `docs/archive/2026-06-30_slicer_legacy/decisions` | 32 | 保留为历史决策证据 |
| 验证方案 | `docs/archive/2026-06-30_slicer_legacy/demos` | 30 | 已完成阶段验证方案，归档 |
| 历史 DEV | `docs/archive/2026-06-30_slicer_legacy/dev` | 31 | 阶段技术设计，归档 |
| 环境说明 | `docs/archive/2026-06-30_slicer_legacy/environment` | 4 | 环境背景，归档 |
| 其他 | `docs/archive/2026-06-30_slicer_legacy/other` | 1 | 保留背景，归档 |
| 历史 PRD | `docs/archive/2026-06-30_slicer_legacy/prd` | 30 | 阶段需求，归档 |
| 状态报告 | `docs/archive/2026-06-30_slicer_legacy/reports` | 31 | 历史状态证据，归档 |
| 审查文档 | `docs/archive/2026-06-30_slicer_legacy/reviews` | 5 | 历史审查依据，归档 |
| 历史路线图 | `docs/archive/2026-06-30_slicer_legacy/roadmaps` | 9 | 被 formal roadmap 替代，归档 |

## 3. Codex 相关文档归档

| 分类 | 目录 | 文件数 | 状态判断 |
|---|---|---:|---|
| 旧执行提示词 | `docs/codex_task/archive/completed_prompts` | 33 | 已完成或已被新阶段替代 |
| 旧任务清单 | `docs/codex_task/archive/completed_tasks` | 35 | 已完成或已被新阶段替代 |
| 旧交接文档 | `docs/codex_task/archive/handoff` | 1 | 只作历史上下文 |
| 当前任务 | `docs/codex_task/current` | 1 | 09P-R2 当前入口 |

## 4. 当前正式入口

| 文件 | 状态 | 用途 |
|---|---|---|
| `docs/slice/README.md` | 当前入口 | docs/slice 使用规则 |
| `docs/slice/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md` | 当前入口 | 文档体系和证据等级 |
| `docs/slice/PRD_FORMAL_SliceSoft_正式切片软件产品需求总览.md` | 当前真源 | 正式产品级需求 |
| `docs/slice/DEV_FORMAL_SliceSoft_正式切片软件总体技术方案.md` | 当前真源 | 正式技术总体方案 |
| `docs/slice/PRD_DEMO_IMPLEMENTED_SliceSoft_当前Demo功能基线.md` | 当前基线 | 已实现 demo 功能 |
| `docs/slice/DEV_DEMO_IMPLEMENTED_SliceSoft_当前Demo技术基线.md` | 当前基线 | 已实现 demo 技术结构 |
| `docs/slice/ROADMAP_FORMAL_SliceSoft_Demo到正式项目演进路线.md` | 当前路线 | 后续阶段规划 |
| `docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md` | 当前执行入口 | 09P-R2 任务拆分 |

## 5. 判断原则

```text
1. archive 中的文档不是删除，而是降级为历史证据。
2. 后续新增 PRD / DEV / ROADMAP / DEMO / REPORT / DOC_DECISION 默认进入 docs/slice。
3. 后续新增 TASKS / CODEX_PROMPT / Codex handoff 默认进入 docs/codex_task。
4. docs/user_guides 继续保留用户指南，不纳入本次归档。
5. 判断当前实现必须结合代码、CMake、脚本、测试和最新 REPORT，不直接照搬历史 PRD/DEV。
```
