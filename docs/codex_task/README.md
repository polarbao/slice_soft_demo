# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 更新日期：2026-07-12
> 当前阶段：12C-R0 Qt 工作台构建兼容与基线准入

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
12C-R0-03 布局与组件复用基线
```

12C-R0-01 fresh Qt UI build gate 与 R0-02 smoke 基线已通过。12C-R1/R2 必须等待 R0-03 完成。

## 保留参考入口

`current` 目录中的 11、11A、11B、12A、12B 和 12D 文件继续保留，用于追溯、并行专项或后续用户明确指定的任务，不得覆盖当前 12C-R0 入口。已完成阶段状态以 `docs/slice/REPORT` 的最新报告为准。

## 使用规则

```text
1. 每次只执行用户明确指定的一个原子任务；
2. 任务开始前读取 AGENTS.md、12C 阶段交接和当前任务文件；
3. 不从 archive 恢复旧任务作为当前任务，除非用户明确指定；
4. 完成任务后输出实际验证命令及结果；
5. 验证通过后按任务文件要求提交，并停止；
6. 未通过 fresh build gate 时，不进入 12C-R1/R2。
```
