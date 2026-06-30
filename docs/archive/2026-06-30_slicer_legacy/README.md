# docs/slicer 历史归档

> 文档状态：Archive Index
> 归档日期：2026-06-30
> 来源目录：`docs/slicer` 和根目录聊天 PDF
> 当前用途：历史证据、阶段回溯、旧决策查询

本目录保存 P0 到 09P-R1 期间形成的大量阶段文档。它们记录了项目真实演进过程，但不再作为当前正式文档入口。

## 分类

| 目录 | 文件数 | 内容 |
|---|---:|---|
| `architecture` | 6 | R0/R1/R2 架构审查、边界说明 |
| `chat_exports` | 1 | 网页 ChatGPT 导出的 PDF 背景资料 |
| `checklists` | 3 | RIP、surface shell 等检查清单 |
| `decisions` | 32 | DOC_DECISION 与阶段性技术决策 |
| `demos` | 30 | 阶段验证方案 |
| `dev` | 31 | 历史 DEV 和阶段技术设计 |
| `environment` | 4 | OpenVDB、vcpkg、构建环境说明 |
| `other` | 1 | 无法归入主类但需保留的历史文档 |
| `prd` | 30 | 历史 PRD 和阶段需求 |
| `reports` | 31 | 阶段当前状态报告 |
| `reviews` | 5 | 文档审查、代码审查、状态判断 |
| `roadmaps` | 9 | 历史路线图 |

## 归档规则

```text
已完成阶段文档：保留在 archive，作为历史证据。
当前正式规划：迁移或重写到 docs/slice。
当前 Codex 任务：迁移到 docs/codex_task/current。
旧 Codex 任务和提示词：迁移到 docs/codex_task/archive。
```

## 使用规则

archive 下的文档可以回答“为什么当时这样做”，但不能直接回答“现在代码是否已经实现”。判断当前实现时必须回到代码、测试、脚本、最新 REPORT 和 docs/slice 正式文档。
