# TASKS 12/13 后续开发计划总览清单

> 文档状态：CURRENT CROSS-STAGE EXECUTION DASHBOARD
> 版本：v1.6
> 更新日期：2026-07-27
> 当前代码阶段：12E-09C、09A-01/02 COMPLETE / Stage 13 13A-01..05、13B-01..04、13B-04A COMPLETE
> 当前原子任务：13B-05 fixture 全局 Raster 与联合层合成 READY
> 下一 Gate：13B-05 PASS -> 13B-06 单 package 与 scene report

## 1. 文档职责

本文是 Stage 12 剩余任务和 Stage 13 近程任务的唯一跨阶段执行看板，用于快速回答：

```text
当前处于哪个阶段；
当前应执行哪个原子任务；
任务依赖是否满足；
哪些任务已完成、等待、阻断或冻结；
完成任务后应更新哪些证据。
```

本文不替代以下真源：

| 真源 | 职责 |
|---|---|
| `REPORT_12X_阶段计划与完成度总览.md` | Stage 12 当前状态、完成度和历史快照解释 |
| `REPORT_13_模型场景排版与TIFF原生预览准备状态.md` | Stage 13 准备度、实现事实和外部 Gate |
| 各阶段 PRD/DEV/DEMO | 需求、技术设计和验证口径 |
| 各阶段 TASKS/CODEX_PROMPT | 单个原子任务的执行范围和命令 |
| 当前代码、测试和实际 REPORT | 是否真正实现和通过的 A 级证据 |

若本文与代码或最新阶段 REPORT 冲突，以代码和最新实际验证证据为准，并立即修订本文。

## 2. 状态词

| 状态 | 含义 |
|---|---|
| `COMPLETE` | 代码、测试和对应状态证据已完成 |
| `READY` | 需求、设计、依赖和验证入口已准备，可在用户授权后开发 |
| `WAIT` | 准备文档已存在，但必须等待前置任务 |
| `BLOCKED` | 缺少外部输入或 Gate，当前不能完成指定验收 |
| `PREPARED` | 已有任务级准备，但不能据此宣称代码完成 |
| `PLANNED` | 仅路线级规划，开发前仍需补齐执行文档 |
| `FROZEN` | 已明确冻结，不得进入实现 |
| `NOT STARTED` | 尚无该任务代码和通过证据 |

## 3. 当前阶段快照

| 工作流 | 当前状态 | 剩余数量 | 当前动作 |
|---|---|---:|---|
| 12E-09A Diagnostic UI | 09A-01/02 COMPLETE；09A-03..06 PREPARED | 4 | 等 13C-03 后进入 09A-03 |
| 12E-10 最终收口 | 概念级准备；执行文档不完整 | 4 | 等 13C-03、09A-05 后补齐并执行 |
| 12F 性能专项 | 12F-01 COMPLETE；12F-02..09 NOT ACTIVE | 8 | Stage 12/13 边界稳定后先刷新 12F-02 |
| 12G-TCWS | FROZEN / NO AUTHORIZATION | 0 个激活任务 | 等产品/RIP G1..G8，不实现 |
| 13A 模型俯视与变换 | 13A-01..05 COMPLETE / M13-1 CANDIDATE PASS | 0 | 保持回归 |
| 13B 多模型排版与联合切片 | 13B-01..04、13B-04A COMPLETE；13B-05 FIXTURE READY；13B-06..07 WAIT | 3 | 执行 13B-05 fixture |
| 13C TIFF 原生统一预览 | P0 设计和原子准备完成；代码未开始 | 5 | 13C-01 READY，按固定顺序排在 13B-07 后 |

计数口径：

```text
Stage 12 仅 12E 收口：8 个；
Stage 12 包含 12F 性能：16 个；
Stage 13 近程 P0：17 个；
当前跨阶段近程/已规划原子任务合计：33 个；
Stage 13 中长期 13A-R2、13A-R3、13B-R4 为未拆分 Epic，不计入上述 33 个；
12G-TCWS 已冻结，不计入激活任务。
```

## 4. 固定执行顺序

### Wave 1：身份合同

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 1 | 13A-01 ModelTransform/ModelInstance | `COMPLETE` | 文档准入完成 | 已解锁 13A-02、13B-01 |
| 2 | 13B-01 MultiModelScene/Scene Effective Config | `COMPLETE` | 13A-01 COMPLETE | 已解锁 scene-aware 09A-02、13B-02 |
| 3 | 12E-09A-02 Diagnostic Effective Config | `COMPLETE` | 13B-01 COMPLETE | 已兼容 single_model/scene/current instance |

### Wave 2：俯视、规则排版与联合写包

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 4 | 13A-02 俯视渲染 | `COMPLETE` | 13A-01、09A-02 COMPLETE | 已解锁精确变换和模型列表 |
| 5 | 13A-03 选择与精确变换 | `COMPLETE` | 13A-02、09A-02 identity | 已解锁镜像/preflight |
| 6 | 13A-04 镜像与 post-transform preflight | `COMPLETE` | 13A-03 COMPLETE | 已解锁 13A 收口 |
| 7 | 13A-05 13A 阶段收口 | `COMPLETE` | 13A-04 COMPLETE | M13-1 CANDIDATE PASS |
| 8 | 13B-02 模型列表与实例操作 | `COMPLETE` | 13B-01、13A-05 COMPLETE | 已解锁规则排版 |
| 9 | 13B-03 11x2 规则排版 | `COMPLETE` | 13B-02 COMPLETE | 已解锁碰撞/幅面准入 |
| 10 | 13B-04 幅面、碰撞和逐实例准入 | `COMPLETE / PROD GATE OPEN` | 13B-03 COMPLETE | fixture PASS；生产需设备 buildVolume/轴方向 |
| 10A | 13B-04A 多模型纹理俯视统一展示 | `COMPLETE` | 13B-04 COMPLETE / 用户插入需求 | 全部 visible 实例、贴图、追加自动排版闭环 |
| 11 | 13B-05 全局 Raster 与联合层合成 | `READY FOR FIXTURE` | 13B-04 功能 Gate | 解锁联合写包 |
| 12 | 13B-06 单 package 与 scene report | `WAIT` | 13B-05 | 解锁真实模型矩阵 |
| 13 | 13B-07 真实模型矩阵与收口 | `WAIT / PROD GATE OPEN` | 13B-06 | 生产 GO 还需 buildVolume 和 22 实例预算 |

### Wave 3：TIFF 原生预览与 Diagnostic UI

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 14 | 13C-01 TiffLayerSource 与 LRU | `READY / SCHEDULED` | identity wave 结束 | 解锁合成器 |
| 15 | 13C-02 MaterialPreviewComposer | `WAIT` | 13C-01 | 解锁统一生产预览 |
| 16 | 13C-03 Unified Production Preview | `WAIT` | 13C-02 | 解锁 09A-05、12E-10A |
| 17 | 12E-09A-03 中文参数控件与状态区 | `WAIT` | 09A-02 | 解锁异步分析 |
| 18 | 12E-09A-04 异步分析 Worker | `WAIT` | 09A-03 | 解锁同层语义预览 |
| 19 | 12E-09A-05 同层语义 Preview | `WAIT` | 09A-04、13C-03 | 解锁 09A 收口和 12E-10A |
| 20 | 12E-09A-06 Diagnostic UI 收口 | `WAIT` | 09A-05 | 09A COMPLETE |
| 21 | 13C-04 Preview IO 收口 | `WAIT` | 13C-03 | 允许默认关闭重复生产 PNG |
| 22 | 13C-05 13C 阶段收口 | `WAIT` | 13C-04 | M13-4 |

### Wave 4：Stage 12 最终收口

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 23 | 12E-10A Texture/Fill/Partition 同层预览 | `PLANNED` | 13C-03、09A-05 | 生产/诊断同层口径 |
| 24 | 12E-10B 真实 OBJ/3MF 模式矩阵 | `PLANNED` | 10A | 真实模型证据 |
| 25 | 12E-10C Release/repair/peak-memory 汇总 | `PLANNED` | 10B | 最终工程矩阵 |
| 26 | 12E-10D 用户手册、REPORT 和上下文封口 | `PLANNED` | 10C | 12E COMPLETE |

`12E-10A..D` 开发前必须补齐独立 PRD/DEV/DEMO/TASKS/CODEX_PROMPT；当前“概念级准备”不等于
可直接开发。

### Wave 5：性能工程化

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 27 | 12F-02 Release Benchmark 刷新 | `PLANNED / NOT ACTIVE` | 12E/13 边界稳定 | 冻结新基线 |
| 28 | 12F-03 支撑统计扫描融合 | `PLANNED` | 12F-02 | 逐项性能证据 |
| 29 | 12F-04 Bottom Projection Range | `PLANNED` | 12F-02 | 逐项性能证据 |
| 30 | 12F-05 Layer Compose 扫描融合 | `PLANNED` | 12F-02 | 逐项性能证据 |
| 31 | 12F-06 Relief Occupancy Provider | `PLANNED` | 12F-02 | 逐项性能证据 |
| 32 | 12F-07 增量缓存 | `PLANNED` | 前述 profile 证据 | 逐项性能证据 |
| 33 | 12F-08 Preview/I/O 解耦 | `PLANNED` | 13C-04/05 | 避免与 TIFF 预览重复建设 |
| 34 | 12F-09 性能阶段收口 | `PLANNED` | 12F-02..08 | 12F COMPLETE |

12F-03..08 不得一次性全开。12F-02 重新基准后，只授权有 profile 证据的优化项。

## 5. Stage 13 准备度结论

### 已完成

```text
13A/13B/13C 的 PRD、DEV、DEMO；
Stage 13 决策、路线、依赖矩阵和未决输入 Gate；
17 个近程任务的依赖、建议文件所有权、验证入口和验收输出；
13A-01、13B-01、13C-01 的执行级合同；
13A-01、13B-01、scene-aware 12E-09A-02 的代码、单测和实际状态报告；
13A-02 的代码、单测、UI Smoke 和状态报告；
13A-03/04 的代码、单测、UI Smoke 和状态报告；
13A-05 的统一回归、用户说明、状态报告和 M13-1 候选；
13B-02 的 1..22 实例列表、操作、保存/回读、单测和 UI Smoke；
13B-03 的代码、单测、Qt 排版页、UI Smoke 和状态报告；
13B-04 的代码、单测、UI Smoke、Quick CI 和状态报告；
13B-04A 的多模型统一俯视、贴图显示、自动排版、单测/UI Smoke 和状态报告；
13B-05 的独立 PREP/PROMPT；
单贡献者的固定执行顺序；
与 12E-09A/10、12F、12G-TCWS 的边界。
```

因此，Stage 13 的 P0 需求分析、总体设计和原子任务准备已经完成。当前可等待用户授权后执行
`13B-05` fixture 全局 Raster 与联合层合成。

### 尚未完成

```text
Stage 13 已完成 13A-01..05、13B-01..04 九个任务；联合 layer/package 和生产证据未完成；
设备 buildVolume、原点和机器轴方向仍未提供；
22 实例生产性能预算仍未提供；
13A-R2/R3 和 13B-R4 只到 Epic，不具备开发级详细设计；
13B production GO 仍被外部 Gate 阻断。
```

这些未完成项不阻断 `13B-05` fixture 开发，也不等于 Stage 13 已生产就绪。

## 6. 外部 Gate

| 外部输入 | 当前临时规则 | 阻断范围 |
|---|---|---|
| 设备 buildVolume | optional/unresolved；fixture 必须显式标识 | 13B-04 production、13B-07 GO |
| 机器原点和 X/Y 方向 | UI 使用 +X 右、+Y 上的软件坐标 | 生产坐标映射 |
| 多实例材料 Profile | P0 `scene_profile_only`，不一致 fail-closed | 未来 mixed-profile |
| 22 实例性能预算 | 先记录实测，不虚构 PASS 阈值 | 13B-07 GO |
| 3D 后端 | 13A-R1 使用 Qt 2D；后续单独 Spike | 13A-R2/R3 |

## 7. 每个原子任务的更新规则

每完成一个原子任务，必须同步：

```text
1. 本文：状态、完成日期、实际证据链接、唯一下一任务；
2. 对应 TASKS：任务状态和实际验证；
3. 对应阶段 REPORT：修改文件、命令、结果和剩余风险；
4. REPORT_12X 或 REPORT_13：阶段完成度；
5. docs/slice、docs/codex_task 和 ai_workspace 索引；
6. 若外部输入关闭，更新 DOC_CHECKLIST_13；
7. 若验证未运行或失败，状态不得写 COMPLETE。
```

任务状态推进只能是：

```text
PLANNED/PREPARED -> READY -> IN PROGRESS -> COMPLETE；
或进入 WAIT/BLOCKED/FROZEN；
不得从文档准备直接跳到 COMPLETE。
```

## 8. 当前执行入口

```text
CURRENT：MULTI-MODEL RASTER/COMPOSE WAVE；
COMPLETE：13A-01..05、13B-01..04、13B-04A、12E-09A-02；
M13-1：CANDIDATE PASS；
NEXT：13B-05 fixture 全局 Raster 与联合层合成；
AUTHORIZATION：13B-02 已按用户授权完成并原子提交；
AFTER 13B-05 PASS：进入 13B-06 单 package 与 scene report；production Gate 继续等待设备输入；
13C-01：技术准备完成，按单贡献者顺序稍后执行。
```
