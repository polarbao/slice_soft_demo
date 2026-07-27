# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 更新日期：2026-07-27
> 当前阶段：12E-09C COMPLETE / 12G-TCWS FROZEN / Stage 13 13A-01、13B-01 COMPLETE

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
docs/codex_task/current/TASKS_12E_09B_Qt双模式生产入口任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_09B_Qt双模式生产入口执行指令.md
docs/codex_task/current/TASKS_12E_09A_诊断UI任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_SceneAware诊断UI执行指令.md
docs/codex_task/current/TASKS_12E_09C_XY_DPI任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_09C_XY_DPI执行指令.md
docs/codex_task/current/TASKS_12_13_后续开发计划总览清单.md
docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_13_模型场景排版联合切片与TIFF预览执行指令.md
docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_真实模型拓扑修复执行指令.md
docs/codex_task/current/TASKS_12E_08C_R4_模型导入预检与修复资产准入任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_R4_模型导入预检与修复资产准入执行指令.md
```

当前原子任务：

```text
12E-09B-01..06 COMPLETE；12E-09C-01..06 COMPLETE；
Stage 13 P0 总体文档和 17 个近程原子任务实施准备 COMPLETE；
13A-01、13B-01 代码、单测和回归 COMPLETE；
scene-aware 12E-09A-02 的独立执行文档已补齐，当前 READY；
12G-TCWS 纹理载体/白色分色候选专项 FROZEN，不得实现。
```

12C-R0/R1/R2 已全部完成。12D-R0/R1/R2/R3 已封口，包含 candidate/exact 诊断、一像素 repair、外部背景保护、Qt 展示和三个真实 OBJ 验收。repair 仍默认关闭。

## 保留参考入口

`current` 目录中的 11、11A、11B、12A、12B、12C 和 12D 文件继续保留，用于追溯或并行专项；12E-08C-R4 和 12E-08D 已完成。当前使用独立 09B TASKS/PROMPT 开发 Qt 双模式生产入口。已完成阶段状态以 `docs/slice/REPORT` 的最新报告为准。

## 当前执行阶段 12E

用户于 2026-07-16 要求补充 12A 中 Texture Surface Layer 与 Model Fill Layer 的匹配组合，已建立 12E 文档和任务计划：

```text
docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md
```

12E 当前状态为 `12E-08D-01..06 COMPLETE / 12E-09B-01..06 COMPLETE / 12E-09C-01..06 COMPLETE`。xiao_ma/yecan 两个独立 strict/admitted 真实模型族和四用例候选证据 PASS；爱神/玫瑰/梯田继续作为 0/3 复杂浮雕覆盖缺口。Target State 保持 `slicePipeline.mode=legacy|global_surface_shell`；Legacy 默认，两个 Global Profile 仅显式 opt-in，且禁止 silent fallback。09C 已完成 X=635/Y=600、显式 600/600、两引擎 package、RIP strict、物理比例 Preview 和回归收口。下一正式路线为 09A-02..06；scene-aware PRD/DEV/DEMO/PREP/PROMPT 已补齐。

## 12F Release Runtime 与性能优化专项

用户于 2026-07-16 要求统一 Qt Debug 环境、建立 Debug/Release 运行环境，并把后续切片性能建议整理为专项：

```text
docs/codex_task/current/TASKS_12F_Release运行环境与切片性能优化任务清单.md
docs/codex_task/current/CODEX_PROMPT_12F_Release运行环境与切片性能优化执行指令.md
```

12F-R0 Runtime 环境已完成；12F-02 及后续算法任务均为 `PLANNED / NOT ACTIVE`。这是一条独立 build/performance 专项，不自动替代 12E 语义任务，也不得与 12E production 接入混合实施。

## Stage 13 模型场景与多模型专项

2026-07-24 新增模型俯视、实例变换、最多 11x2 规则排版、多模型联合切片和 TIFF 原生统一预览需求：

```text
docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_13_模型场景排版联合切片与TIFF预览执行指令.md
```

当前已完成 P0 需求/设计/验证、全阶段原子任务准备和 13A-01/13B-01 核心合同。下一步执行
scene-aware `12E-09A-02`，13A-02 已解除功能依赖但按顺序排在其后。13C 必须在
`12E-09A-05` 前完成生产 TIFF 数据源收口。Stage 13 不改变 RGBWSV 协议，也不把多模型失败静默
降级成多个单模型成功。

跨 Stage 12/13 的当前状态、固定顺序和更新规则统一查看
`current/TASKS_12_13_后续开发计划总览清单.md`。

## 使用规则

```text
1. 每次只执行用户明确指定的一个原子任务；
2. 任务开始前读取 AGENTS.md、12D 完成报告、12E-R0 准备文档和当前任务文件；
3. 不从 archive 恢复旧任务作为当前任务，除非用户明确指定；
4. 完成任务后输出实际验证命令及结果；
5. 验证通过后按任务文件要求提交，并停止；
6. 不跳过 TIFF candidate 与 semantic mask exact 的证据边界。
```
