# Codex Task 文档入口

> 文档状态：Codex Task Entry
> 生成日期：2026-06-30
> 更新日期：2026-08-06
> 当前阶段：Stage 15 **COMPLETE / 19 OF 19**；Stage 14 **切片侧已收口（2026-08-07）**，外部验收按用户决策延期；HOSTFLOW **ACTIVE / H-A COMPLETE / H-B-01 COMPLETE / H-B-02 NEXT**；Stage 16 **PROPOSED / NOT ACTIVE**
>
> **⬇️ 切片侧收口后的下一步：见下方「切片侧收口后的接续专项」**

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

## 切片侧收口后的接续专项（2026-08-07 新增）

Stage 14 切片侧已收口、`CURRENT_NEXT_TASK = NONE`，外部验收延期。
以下两个**独立补充专项**不占阶段编号，与 Stage 16 互不依赖，是接续执行的候选：

```text
docs/codex_task/current/TASKS_HOSTFLOW_宿主业务流程与场景生命周期补齐任务清单.md
docs/codex_task/current/CODEX_PROMPT_HOSTFLOW_宿主业务流程与场景生命周期执行指令.md  ← 开工入口
docs/codex_task/current/TASKS_RENDER_模型显示与LOD修复补充任务清单.md
```

> HOSTFLOW 已于 2026-08-07 获得 HQ-01 授权并转为 `ACTIVE`；H-A-01 已完成合同受控修订。
> HQ-07 已授权，DTO v1.6 `sceneContext` 与 H-A-02 场景生命周期运行时已通过 Debug/Release 门禁。
> DTO v1.7 与 H-A-03 空场景端到端闭环已通过 Debug/Release 门禁；H-A 全组完成。H-B-01 模型导入与快速预检已通过 Debug/Release 门禁，下一张独立卡为 H-B-02。
> RENDER 仍为 `PROPOSED / NOT ACTIVE`。
> 按本目录使用规则「一次只执行用户明确指定的一个原子任务」，
> **codex 不得自行启动**，须等用户指定卡号（例如「执行 H-B-04」）。
>
> **无需任何新授权即可启动的路径**：`H-B-04 → 05 → 06 → 07 → 08`
> （Profile → 参数 → 切片 → 结果 → 持久化；用既有 fixture scene，不改契约）
>
> **H-A 授权状态**：HQ-01 已授权；每张后续原子卡仍需用户点名执行。

**建议执行顺序（按「晚做的返工成本」排序，非按重要性）**

```text
P0 · 已完成 H-A-01..04 COMPLETE       ABI 场景生命周期（HOSTFLOW）
            R-A-01                       实测甲片三角面数（RENDER，成本极低）
P1 · 当前   H-B-01 COMPLETE；H-B-02..08  宿主业务流程 UI（HOSTFLOW）
            R-B-01/02                    LOD 跳采样修复（RENDER，若 R-A-01 判为 P1）
P2          H-C-01..03 · R-C-01/02 · R-D-01
P3          Stage 16 · 03E 第二步 · R-D-02/03/04
P4          12F-02..09 · unexpected_overlap 缺陷卡
```

> 🔴 **H-A 为什么必须现在做（时机窗口）**
>
> `14A_EXTERNAL_ACK = PENDING` —— 14A-03 与 14A-04-R1 **尚未取得打印侧回签**。
> H-A 若在此窗口内落地，可并入同一批回签，**零额外协调成本**；
> 一旦打印侧完成回签，再加 `addInstance`/`removeInstance` 就是**破坏已回签契约**，代价陡增。
>
> 且切片侧已收口，此刻修订契约**不会打断任何进行中的验证**。
> **这是最后一个能低成本扩展 ABI 的窗口。**
>
> H-A 解决的是 Stage 14 核心目标的唯一未验证项：
> ABI 当前无 `addInstance`，宿主只能自拼 scene JSON → 「打印软件最少改动移植」无法成立。

## 下一候选执行入口

```text
docs/codex_task/current/TASKS_15_纹理纯白区按需补白任务清单.md
docs/codex_task/current/CODEX_PROMPT_15_纹理纯白区按需补白执行指令.md
docs/codex_task/current/TASKS_16_切片几何采样甲片接触姿态与性能专项任务清单.md
docs/codex_task/current/CODEX_PROMPT_16_切片几何采样甲片接触姿态与性能专项执行指令.md
docs/codex_task/current/TASKS_14_切片能力包封装与打印软件集成任务清单.md
docs/codex_task/current/CODEX_PROMPT_14_切片能力包封装与打印软件集成执行指令.md
docs/codex_task/current/TASKS_03E_TIFF压缩兼容与性能任务清单.md
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
03D-LIBTIFF 01..07 COMPLETE，最终 GO_OPTIONAL，默认 Writer 未切换；
03E-01 COMPLETE；03E-02 INTERNAL COMPLETE / EXTERNAL RIP PENDING，PackBits 仅显式实验，默认 none；
12E-09D 01..06 COMPLETE；12E-10A/10B/10C/10D COMPLETE；Stage 12E COMPLETE；
12G-TCWS 已记录现有 RIP 的 WSV=000 白区信号，但仍为 FROZEN，不得实现。
Stage 15 纯白纹理按需补 W 已 **COMPLETE / PRODUCTION ENABLED**（19/19 任务卡，G1–G8 全通过，候选 Profile 已翻转为 enabled/production）。
Stage 14 能力包集成已于 2026-08-04 **授权激活（ACTIVE）**：RIP 六问两轮闭合，S2 权威条款见 `docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md`。
14A 切片侧 01..11 已收口；14A-03 与 14A-04-R1 仍待打印侧书面回签。14B 实施准备门已通过，下一开发任务为 14B-00，14B-06 可并行。14A-08 已 COMPLETE，不要重发 RIP 问卷。
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

12E 当前状态为 `12E-08D-01..06 / 09A-01..06 / 09B-01..06 / 09C-01..06 / 09D-01..06 / 10A..10D COMPLETE`。xiao_ma/yecan Legacy/Global minimum/intermediate/all_texture 与 Texture2D checker 3MF 共 14 行生产 PASS；爱神/玫瑰/梯田 3 行为 `BLOCKED_EXPECTED`。10C 通过 36/36 Release 计量样本和 RIP strict。Target State 保持 `slicePipeline.mode=legacy|global_surface_shell`；Legacy 默认，Global 仅显式 opt-in，禁止 silent fallback。Stage 12E 当前批准范围已封口，后续阶段需独立授权。

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

## Stage 16 几何采样、接触姿态与性能专项

2026-08-06 建立 Stage 16 文档准备，用于评估层体积/2x2 几何采样、甲片接触诊断/受限调平，并整并 12F-02..09、13F-R1-01..05 和 13B 22 实例性能预算：

```text
docs/codex_task/current/TASKS_16_切片几何采样甲片接触姿态与性能专项任务清单.md
docs/codex_task/current/CODEX_PROMPT_16_切片几何采样甲片接触姿态与性能专项执行指令.md
```

当前状态是 `PROPOSED / NOT ACTIVE`。Stage 14 收口前不得执行 Stage 16 代码卡；Stage 14 收口后仍需先完成 16-00 GO/DEFER/NO-GO 复核和用户授权。

## 使用规则

```text
1. 每次只执行用户明确指定的一个原子任务；
2. 任务开始前读取 AGENTS.md、12D 完成报告、12E-R0 准备文档和当前任务文件；
3. 不从 archive 恢复旧任务作为当前任务，除非用户明确指定；
4. 完成任务后输出实际验证命令及结果；
5. 验证通过后按任务文件要求提交，并停止；
6. 不跳过 TIFF candidate 与 semantic mask exact 的证据边界。
```
