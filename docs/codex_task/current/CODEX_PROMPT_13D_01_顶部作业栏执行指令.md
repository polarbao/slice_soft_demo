# CODEX PROMPT 13D-01 顶部作业栏执行指令

> 状态：READY FOR DEVELOPMENT（13C-05 已于 2026-07-28 完成）

请先阅读 13B-08、13C 最终状态和以下文件：

```text
docs/slice/PRD/PRD_13D_Qt工作台信息架构与布局收口.md
docs/slice/DEV/DEV_13D_Qt工作台Shell与ContextInspector设计.md
docs/slice/DEMO/DEMO_13D_Qt工作台交互验证方案.md
docs/slice/DOC/DOC_PREP_13D_工作台布局收口准备.md
docs/codex_task/current/TASKS_13D_Qt工作台布局收口任务清单.md
```

只执行 13D-01：复用 13B-08 的主动作建立顶部作业栏。不得同时迁移 ContextInspector、诊断 Dock 或
重写 PreviewWorkspace。完成后运行默认布局、主动作状态、13B-08 作业流和现有 UI self-test；未通过不得
进入 13D-02。
