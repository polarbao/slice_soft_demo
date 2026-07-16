# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 更新日期：2026-07-16
> 当前阶段：12D-R3 PREPARED / NOT STARTED

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
12D-07 Repair Enabled 一像素修复（等待用户明确启动）
```

12C-R0/R1/R2 已全部完成。12D-R1 的配置契约、报告骨架和 TIFF candidate，以及 R2 的 semantic mask exact detector 与 repair-disabled TIFF 不变性守门均已完成。12D-R3 已准备但未开始，仍不得在没有用户明确指令时启用 repair。

## 保留参考入口

`current` 目录中的 11、11A、11B、12A、12B 和 12C 文件继续保留，用于追溯或并行专项；当前唯一执行入口为 12D。已完成阶段状态以 `docs/slice/REPORT` 的最新报告为准。

## 已规划但未激活的 12E

用户于 2026-07-16 要求补充 12A 中 Texture Surface Layer 与 Model Fill Layer 的匹配组合，已建立 12E 文档和任务计划：

```text
docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md
```

12E 当前状态为 `PLANNED / NOT ACTIVE`。12E-00 只完成文档准入；任何 C++、Qt、CMake、config 或 production output 修改都必须由用户明确指定一个 12E 原子任务后再开始。

## 12F Release Runtime 与性能优化专项

用户于 2026-07-16 要求统一 Qt Debug 环境、建立 Debug/Release 运行环境，并把后续切片性能建议整理为专项：

```text
docs/codex_task/current/TASKS_12F_Release运行环境与切片性能优化任务清单.md
docs/codex_task/current/CODEX_PROMPT_12F_Release运行环境与切片性能优化执行指令.md
```

12F-R0 Runtime 环境已完成；12F-02 及后续算法任务均为 `PLANNED / NOT ACTIVE`。这是一条独立 build/performance 专项，不自动替代当前 12D 唯一代码执行入口，也不得与未完成的 12D/12E production 语义工作混合实施。

## 使用规则

```text
1. 每次只执行用户明确指定的一个原子任务；
2. 任务开始前读取 AGENTS.md、12D-R2 完成记录、12D-R3 准备文档和当前任务文件；
3. 不从 archive 恢复旧任务作为当前任务，除非用户明确指定；
4. 完成任务后输出实际验证命令及结果；
5. 验证通过后按任务文件要求提交，并停止；
6. 不跳过 TIFF candidate 与 semantic mask exact 的证据边界。
```
