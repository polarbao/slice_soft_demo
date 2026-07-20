# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 更新日期：2026-07-20
> 当前阶段：12D COMPLETE / 12E-08C-R1 COMPLETE / R2-01..03 COMPLETE / R2-04 READY / 双切片模式目标已固化

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

## 下一候选执行入口

```text
docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md
docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_真实模型拓扑修复执行指令.md
```

当前原子任务：

```text
无 active code task；下一原子任务为 12E-08C-R2-04 Post-Repair Strict 与 Attribute Guard
```

12C-R0/R1/R2 已全部完成。12D-R0/R1/R2/R3 已封口，包含 candidate/exact 诊断、一像素 repair、外部背景保护、Qt 展示和三个真实 OBJ 验收。repair 仍默认关闭。

## 保留参考入口

`current` 目录中的 11、11A、11B、12A、12B、12C 和 12D 文件继续保留，用于追溯或并行专项；12E-08A/08B/08C 已完成 diagnostic 与 Release 证据，但真实 OBJ 被 topology 阻断。12E-08D 前必须先执行 12E-08C-R1/R2/R3 修复前置专项。已完成阶段状态以 `docs/slice/REPORT` 的最新报告为准。

## 当前执行阶段 12E

用户于 2026-07-16 要求补充 12A 中 Texture Surface Layer 与 Model Fill Layer 的匹配组合，已建立 12E 文档和任务计划：

```text
docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md
```

12E 当前状态为 `12E-08C RELEASE BUDGET BLOCKED / R1 COMPLETE / R2-01..03 COMPLETE`。CPU 与 OpenVDB OFF/ON 同 grid conformance、动态 width sweep、纹理传递、内存 Diagnostic Composer、12D 模型域与完整材料域 exact closure、classification-to-raster、legacy regression、Mesh Repair DTO/hash/report skeleton、pre-repair eligibility、generated fixture golden、四 case baseline、cleanup、受约束 weld/winding/component guard 和简单 boundary fill 已可复现；production output 仍缺 non-manifold repair、完整自相交证据、统一 post-strict/attribute validator、可冻结 Release 预算和 12E-08D admission。下一任务为 R2-04。后续 Target State 已固化为 `slicePipeline.mode=legacy|global_surface_shell`，两条生产成功路径共用当前 RGBWSV TIFF writer；该 Router、global production adapter 和 UI 选择器均尚未实现，global 当前仍是 diagnostic-only。

## 12F Release Runtime 与性能优化专项

用户于 2026-07-16 要求统一 Qt Debug 环境、建立 Debug/Release 运行环境，并把后续切片性能建议整理为专项：

```text
docs/codex_task/current/TASKS_12F_Release运行环境与切片性能优化任务清单.md
docs/codex_task/current/CODEX_PROMPT_12F_Release运行环境与切片性能优化执行指令.md
```

12F-R0 Runtime 环境已完成；12F-02 及后续算法任务均为 `PLANNED / NOT ACTIVE`。这是一条独立 build/performance 专项，不自动替代 12E 语义任务，也不得与 12E production 接入混合实施。

## 使用规则

```text
1. 每次只执行用户明确指定的一个原子任务；
2. 任务开始前读取 AGENTS.md、12D 完成报告、12E-R0 准备文档和当前任务文件；
3. 不从 archive 恢复旧任务作为当前任务，除非用户明确指定；
4. 完成任务后输出实际验证命令及结果；
5. 验证通过后按任务文件要求提交，并停止；
6. 不跳过 TIFF candidate 与 semantic mask exact 的证据边界。
```
