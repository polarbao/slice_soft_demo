# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 更新日期：2026-07-14
> 当前阶段：12C-R2 预览与诊断工作区

本目录存放 Codex 操作任务、执行提示词和历史任务归档。`current` 表示文件仍需保留或可能继续执行，不表示其中每份任务都是当前入口。

## 目录结构

```text
docs/codex_task/current
  当前任务、并行专项和仍需保留的任务文件。

docs/codex_task/archive/completed_tasks
  已完成或被新阶段替代的任务清单。

docs/codex_task/archive/completed_prompts
  已完成或被新阶段替代的执行提示词。

docs/codex_task/archive/handoff
  历史交接文档，只作背景。
```

## 当前唯一执行入口

```text
docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_12C_Qt工作台收口执行指令.md
```

当前原子任务：

```text
12C-R2-01 PreviewWorkspace 与共享层状态
```

12C-R0 与完整 R1 设置管线已完成。当前进入 R2 PreviewWorkspace 与共享层状态；本任务只协调既有预览组件，不重写其底层渲染能力。

## 保留参考入口

`current` 目录中的 11、11A、11B、12A、12B 和 12D 文件继续保留，用于追溯、并行专项或后续任务；当前唯一执行入口仍是 12C。12D-R0 文档已准备，但代码任务不得覆盖当前 12C-R1/R2 入口。已完成阶段状态以 `docs/slice/REPORT` 的最新报告为准。

## 使用规则

```text
1. 每次只执行用户明确指定的一个原子任务；
2. 任务开始前读取 AGENTS.md、12C 阶段交接和当前任务文件；
3. 不从 archive 恢复旧任务作为当前任务，除非用户明确指定；
4. 完成任务后输出实际验证命令及结果；
5. 验证通过后按任务文件要求提交，并停止；
6. 未通过 fresh build gate 时，不进入 12C-R1/R2。
```
