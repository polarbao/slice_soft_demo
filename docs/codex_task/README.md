# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 当前阶段：11 UI 切片层预览、交互配置与多模型能力评估

本目录用于存放 Codex 相关的操作任务、执行提示词、任务清单与历史任务归档。

## 目录结构

```text
docs/codex_task/current
  当前可执行任务。Codex 每次只应读取并执行用户明确指定的一个任务。

docs/codex_task/archive/completed_tasks
  已完成或已被新阶段替代的 TASKS / CODEX_TASKS。

docs/codex_task/archive/completed_prompts
  已完成或已被新阶段替代的 CODEX_PROMPT。

docs/codex_task/archive/handoff
  旧交接文档。保留历史上下文，但不直接代表当前真源。
```

## 当前任务入口

```text
docs/codex_task/current/TASKS_11_UI切片层预览交互配置与多模型评估任务清单.md
docs/codex_task/current/CODEX_PROMPT_11_UI切片层预览交互配置与多模型评估执行指令.md
```

已完成 / 保留参考入口：

```text
docs/codex_task/current/TASKS_00_08_历史阶段文档补齐任务清单.md
docs/codex_task/current/CODEX_PROMPT_00_08_历史阶段文档补齐执行指令.md
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md
docs/codex_task/current/CODEX_PROMPT_09P_R2_OpenVDB实验生产管线Hardening执行指令.md
docs/codex_task/current/TASKS_10_切片输出交付契约与纹理保真验收任务清单.md
docs/codex_task/current/CODEX_PROMPT_10_切片输出交付契约与纹理保真验收执行指令.md
```

## 使用规则

```text
1. 每次只执行用户明确指定的一个任务。
2. 任务开始前读取 AGENTS.md、docs/slice/README.md 和当前任务文件。
3. 不从 archive 中恢复旧任务作为当前任务，除非用户明确指定。
4. 旧 CODEX_PROMPT 只作为历史参考，不作为当前执行入口。
5. 完成任务后必须输出验证命令及结果；未运行的验证要说明原因。
```
