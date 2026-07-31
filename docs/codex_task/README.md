# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 更新日期：2026-07-31
> 当前阶段：03D-LIBTIFF、12E-09D PREPARED；当前下一原子任务为 03D-06

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
docs/codex_task/current/TASKS_03D_LibTIFF兼容迁移任务清单.md
docs/codex_task/current/CODEX_PROMPT_03D_LibTIFF兼容迁移执行指令.md
docs/codex_task/current/TASKS_12E_09D_生产纹理厚度与单材料材质任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_09D_生产纹理厚度与单材料材质执行指令.md
docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md
docs/codex_task/current/TASKS_12E_09B_Qt双模式生产入口任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_09B_Qt双模式生产入口执行指令.md
docs/codex_task/current/TASKS_12E_09A_诊断UI任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_SceneAware诊断UI执行指令.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_05_同层语义Preview执行指令.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_06_诊断UI阶段收口执行指令.md
docs/codex_task/current/TASKS_12E_09C_XY_DPI任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_09C_XY_DPI执行指令.md
docs/codex_task/current/TASKS_12E_10_双模式最终闭环任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_10_双模式最终闭环执行指令.md
docs/codex_task/current/TASKS_12_13_后续开发计划总览清单.md
docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_13_模型场景排版联合切片与TIFF预览执行指令.md
docs/codex_task/current/CODEX_PROMPT_13A_03_选择与精确变换执行指令.md
docs/codex_task/current/CODEX_PROMPT_13A_04_镜像与变换后预检执行指令.md
docs/codex_task/current/CODEX_PROMPT_13A_05_模型俯视与变换阶段收口执行指令.md
docs/codex_task/current/CODEX_PROMPT_13B_02_模型列表与实例操作执行指令.md
docs/codex_task/current/CODEX_PROMPT_13B_03_11x2规则排版执行指令.md
docs/codex_task/current/CODEX_PROMPT_13B_04_幅面碰撞与逐实例准入执行指令.md
docs/codex_task/current/CODEX_PROMPT_13B_05_全局Raster与联合层合成执行指令.md
docs/codex_task/current/CODEX_PROMPT_13B_06_单Package与SceneReport执行指令.md
docs/codex_task/current/CODEX_PROMPT_13C_03_UnifiedProductionPreview执行指令.md
docs/codex_task/current/TASKS_13B_08_场景作业流收口任务清单.md
docs/codex_task/current/CODEX_PROMPT_13B_08_01_批量导入与主切片入口执行指令.md
docs/codex_task/current/TASKS_13D_Qt工作台布局收口任务清单.md
docs/codex_task/current/CODEX_PROMPT_13D_01_顶部作业栏执行指令.md
docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_真实模型拓扑修复执行指令.md
docs/codex_task/current/TASKS_12E_08C_R4_模型导入预检与修复资产准入任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_R4_模型导入预检与修复资产准入执行指令.md
docs/codex_task/current/TASKS_13G_支撑投影铺底与层间连续性任务清单.md
docs/codex_task/current/CODEX_PROMPT_13G_支撑投影铺底与层间连续性执行指令.md
```

当前原子任务：

```text
12E-09B-01..06 COMPLETE；12E-09C-01..06 COMPLETE；
Stage 13 P0 总体文档和 17 个近程原子任务实施准备 COMPLETE；
13A-01..05、13B-01..07、scene-aware 12E-09A-02 代码、单测和回归 COMPLETE；
13A-05 已完成统一回归和 M13-1 候选收口；
13B-02 的 1..22 实例列表、场景操作和 UI Smoke 已完成；
13B-03 的代码、测试、Qt 排版页和状态报告已完成；
13B-04 fixture 功能 Gate 已完成，production Gate 仍等待正式设备幅面；
13B-05 的 Legacy/Global adapter、共享 Grid、联合内存层和状态报告已完成；
13B-06 的单 package、typed scene report、原子发布和 RIP strict fixture 已完成；
13B-07 Debug/Release 真实模型功能矩阵、单 package 和 RIP strict 已完成，production GO 继续等待外部输入；
13B-08 批量导入与当前场景一键切片专项 01..04 已完成，真实 OBJ/3MF 作业流和 RIP strict PASS；
13D 工作台布局专项 13D-01..04 已完成；12E-09A-01..06 已完成；
13E 甲片自动定向与诊断工作流 01..05 已完成，正面 +Z、9 mm 默认和右侧诊断 PASS；
13G-00..07 已完成，Reality 5/5 正反面修正、最大投影铺底、Qt/Effective Config、segment_105 Release/RIP 均 PASS；
03D-LIBTIFF 01..05 COMPLETE，03D-06 READY；未切换默认 Writer；
12E-09D 01..06 执行包 PREPARED，排在 03D 之后、12E-10A 之前；
12G-TCWS 已记录现有 RIP 的 WSV=000 白区信号，但仍为 FROZEN，不得实现。
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

12E 当前状态为 `12E-08D-01..06 COMPLETE / 12E-09A-01..06 COMPLETE / 12E-09B-01..06 COMPLETE / 12E-09C-01..06 COMPLETE / 12E-09D PREPARED`。xiao_ma/yecan 两个独立 strict/admitted 真实模型族和四用例候选证据 PASS；爱神/玫瑰/梯田继续作为 0/3 复杂浮雕覆盖缺口。Target State 保持 `slicePipeline.mode=legacy|global_surface_shell`；Legacy 默认，两个 Global Profile 仅显式 opt-in，且禁止 silent fallback。09C 已完成 X=635/Y=600、显式 600/600、两引擎 package、RIP strict、物理比例 Preview 和回归收口。09D 将把生产纹理参数从只读诊断宽度中分离，并补齐单材料浮雕 W/V 选择；目前没有代码完成证据。03D-LIBTIFF 是第一优先级，完成当前 Writer 基线与双后端 Gate 后再进入 09D；12E-10A..D 保持后续准备状态。

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

当前已完成 P0 需求/设计/验证、全阶段原子任务准备、13A-01..05/13B-01..07 和 scene-aware
`12E-09A-02`。13C-01 TIFF 原生数据源、13C-02 材料合成器和 13C-03 统一生产预览已完成。
真实 UI 截图审计后新增 `13B-08` 和 `13D`：13B-08-01..04 与 13C-01..05
均已完成，13D-01..04 已完成。13C 已在 `12E-09A-05` 前完成生产 TIFF 数据源收口。
Stage 13 不改变 RGBWSV 协议，也不把多模型失败静默
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
