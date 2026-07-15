# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 更新日期：2026-07-15
> 当前阶段：12D-R1 材料闭环配置与候选诊断

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
docs/codex_task/current/TASKS_12D_横截面材料无缝闭环任务清单.md
docs/codex_task/current/CODEX_PROMPT_12D_横截面材料闭环执行指令.md
```

当前原子任务：

```text
12D-04 TIFF 反推候选诊断
```

12C-R0/R1/R2 已全部完成，最终 fresh lane、完整 UI Smoke 和 CTest 已通过。12D-R0 文档准备和 12C 交接门禁均已满足，12D-02 配置契约和 12D-03 报告骨架已完成，下一原子任务为 12D-04；不得提前实现 semantic mask exact diagnosis 或 repair。

## 保留参考入口

`current` 目录中的 11、11A、11B、12A、12B 和 12C 文件继续保留，用于追溯或并行专项；当前唯一执行入口为 12D。已完成阶段状态以 `docs/slice/REPORT` 的最新报告为准。

## 使用规则

```text
1. 每次只执行用户明确指定的一个原子任务；
2. 任务开始前读取 AGENTS.md、12C 到 12D-R1 阶段交接和当前任务文件；
3. 不从 archive 恢复旧任务作为当前任务，除非用户明确指定；
4. 完成任务后输出实际验证命令及结果；
5. 验证通过后按任务文件要求提交，并停止；
6. 不跳过 TIFF candidate 与 semantic mask exact 的证据边界。
```
