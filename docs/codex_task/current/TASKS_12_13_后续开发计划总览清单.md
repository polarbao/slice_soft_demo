# TASKS 12/13 后续开发计划总览清单

> 文档状态：CURRENT CROSS-STAGE EXECUTION DASHBOARD
> 版本：v3.0
> 更新日期：2026-07-31
> 当前代码阶段：13G-00..07 COMPLETE；Stage 12/13 既有完成项保持不变
> 当前推荐任务：03D-02 READY / WAIT EXPLICIT AUTHORIZATION
> 后续顺序：03D -> 12E-09D -> 12E-10A..D；13F-R1-01..05 保持独立准备

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
| 12E-09A Diagnostic UI | 09A-01..06 COMPLETE / PASS | 0 | 保持回归 |
| 03D-LIBTIFF Writer 兼容迁移 | 03D-01 COMPLETE / 03D-02 READY | 6 | 等待明确授权后接入 vcpkg/CMake/Runtime；默认 Writer 暂不切换 |
| 12E-09D 生产纹理与单材料控制 | 09D-01..06 PREPARED | 6 | 等待 03D 当前范围完成后执行 |
| 12E-10 最终收口 | PRD/DEV/DEMO/PREP/TASKS/PROMPT 完整；10A READY | 4 | 用户授权后按 10A -> 10B -> 10C -> 10D |
| 12F 性能专项 | 12F-01 COMPLETE；12F-02..09 NOT ACTIVE | 8 | Stage 12/13 边界稳定后先刷新 12F-02 |
| 12G-TCWS | FROZEN / NO AUTHORIZATION | 0 个激活任务 | 等产品/RIP G1..G8，不实现 |
| 13A 模型俯视与变换 | 13A-01..05 COMPLETE / M13-1 CANDIDATE PASS | 0 | 保持回归 |
| 13B 多模型排版与联合切片 | 13B-01..08、13B-04A FUNCTIONAL COMPLETE；production INPUT OPEN | 0 | 保持回归并等待设备输入 |
| 13C TIFF 原生统一预览 | 13C-01..05 COMPLETE / M13-4 PASS | 0 | 保持回归 |
| 13D Qt 工作台布局 | 13D-01..04 COMPLETE | 0 | 保持回归 |
| 13E 自动定向与诊断工作流 | 13E-01..05 COMPLETE / FUNCTIONAL PASS | 0 | 保持甲片 +Z 正面、9 mm 默认和右侧诊断回归 |
| 13G 支撑投影铺底与层间连续性 | 00..07 COMPLETE / FUNCTIONAL PASS | 0 | 保持 front-up、0..29 铺底和 RIP 回归 |

计数口径：

```text
当前剩余任务：03D 6 个、12E-09D 6 个；
03D/09D/12E-10 合计待执行：16 个；
另含 12F 性能：8 个；
Stage 13 近程 P0：17 个；
13B-08/13D 共 8 个插入任务均已完成；13E 新增 5 个插入任务并已完成；
Stage 13 中长期 13A-R2、13A-R3、13B-R4 为未拆分 Epic，不计入待执行原子任务；
12G-TCWS 已冻结，不计入激活任务；现有 RIP 白区事实只作为评审输入。
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
| 11 | 13B-05 全局 Raster 与联合层合成 | `FIXTURE COMPLETE` | 13B-04 功能 Gate | 已解锁联合写包 |
| 12 | 13B-06 单 package 与 scene report | `FIXTURE COMPLETE` | 13B-05 | 已解锁真实模型矩阵 |
| 13 | 13B-07 真实模型矩阵与收口 | `FUNCTIONAL MATRIX COMPLETE / PROD INPUT OPEN` | 13B-06 COMPLETE | Debug/Release 功能矩阵 PASS；生产 GO 还需 buildVolume 和 22 实例预算 |

### Wave 2.5：场景作业流插入专项

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 13A | 13B-08-01 批量导入队列 | `COMPLETE` | 13B-07 功能链 | 已解锁场景生产服务 |
| 13B | 13B-08-02 场景生产服务与显式 CLI | `COMPLETE` | 13B-08-01 | 已解锁 Qt 主动作 |
| 13C | 13B-08-03 Qt 切片当前场景 | `COMPLETE` | 13B-08-02 | 当前场景已可产生一个 Package 并回载 TIFF |
| 13D | 13B-08-04 真实模型矩阵与收口 | `COMPLETE / FUNCTIONAL PASS` | 13B-08-03 | 已解锁 13C-04 |

`13B-08` 不改变设备 production Gate；正式设备输入未关闭时只形成 functional PASS。

### Wave 3：TIFF 原生预览与 Diagnostic UI

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 14 | 13C-01 TiffLayerSource 与 LRU | `COMPLETE` | identity wave 结束 | 已解锁合成器 |
| 15 | 13C-02 MaterialPreviewComposer | `COMPLETE` | 13C-01 COMPLETE | 已解锁统一生产预览 |
| 16 | 13C-03 Unified Production Preview | `COMPLETE` | 13C-02 COMPLETE | 已解锁 Preview IO 收口 |
| 17 | 12E-09A-03 中文参数控件与状态区 | `COMPLETE` | 09A-02、13D-04 | 已解锁异步分析 |
| 18 | 12E-09A-04 异步分析 Worker | `COMPLETE` | 09A-03 COMPLETE | 已解锁同层语义预览 |
| 19 | 12E-09A-05 同层语义 Preview | `COMPLETE` | 09A-04、13C-03 COMPLETE | 已解锁 09A 收口 |
| 20 | 12E-09A-06 Diagnostic UI 收口 | `COMPLETE / PASS` | 09A-05 | 09A COMPLETE |
| 21 | 13C-04 Preview IO 收口 | `COMPLETE` | 13C-03、13B-08 COMPLETE | 默认无重复诊断图 |
| 22 | 13C-05 13C 阶段收口 | `COMPLETE / M13-4 PASS` | 13C-04 | 已解锁 13D-01 |

### Wave 3.5：Qt 工作台布局收口

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 22A | 13D-01 顶部作业栏 | `COMPLETE` | 13B-08、13C-05 | 固定顶部主动作与状态摘要已实现 |
| 22B | 13D-02 单一 Context Inspector | `COMPLETE` | 13D-01 | 双右栏已收为唯一检查器 |
| 22C | 13D-03 项目区与诊断 Dock | `COMPLETE` | 13D-02 | 高级工具和诊断入口已收口 |
| 22D | 13D-04 响应式与阶段收口 | `COMPLETE` | 13D-03 | 已解锁 12E-09A-03 |

### Wave 3.6：甲片自动定向与诊断工作流插入专项

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 22E | 13E-01 专项准备与证据冻结 | `COMPLETE` | 13D COMPLETE | 根因与阶段边界冻结 |
| 22F | 13E-02 AutoOrient 确定性修复 | `COMPLETE` | 13E-01 | 等价候选稳定选择 `rotate_x_90` |
| 22G | 13E-03 产品默认高度 9 mm | `COMPLETE` | 13E-02 | core、Qt 和产品 Profile 同步 |
| 22H | 13E-04 诊断信息架构调整 | `COMPLETE` | 13E-01 | 右侧预检与诊断、底部任务详情 |
| 22I | 13E-05 回归与阶段收口 | `COMPLETE / FUNCTIONAL PASS` | 13E-02..04 | Quick CI、UI Smoke 和三模型方向证据 PASS |

### Wave 3.7：TIFF Writer 兼容与性能第一优先级

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 22J | 03D-01 当前合同与 Writer-only Release 基线 | `COMPLETE` | 03D 文档包完整 | 已冻结手写 Writer 标签、像素、strip/tile、错误和耗时/内存基线 |
| 22K | 03D-02 vcpkg/CMake/Runtime 接入 | `READY / WAIT EXPLICIT AUTHORIZATION` | 03D-01 COMPLETE | `TIFF::TIFF` 可选依赖和运行时部署闭环 |
| 22L | 03D-03 LibTIFF stripped Writer | `PREPARED / WAIT 03D-02` | 03D-02 | stripped 解码像素、标签与 RIP 等价 |
| 22M | 03D-04 LibTIFF tiled Writer 与错误收口 | `PREPARED / WAIT 03D-03` | 03D-03 | tiled 等价、错误码和清理路径闭环 |
| 22N | 03D-05 正负向兼容矩阵 | `PREPARED / WAIT 03D-04` | 03D-04 | handwritten/libtiff package、Reader、bad-package 矩阵 PASS |
| 22O | 03D-06 Release 性能 Gate | `PREPARED / WAIT 03D-05` | 03D-05 | p50/峰值内存满足文档 Gate，或明确 NO-GO |
| 22P | 03D-07 默认后端决策与收口 | `PREPARED / SEPARATE AUTHORIZATION` | 03D-06 | 只有 GO 且用户明确授权才切换默认 Writer |

03D 固定保持 `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`、不压缩、contiguous、
stripped/tiled。该阶段不新增压缩、BigTIFF、多 IFD 或 planar separate。

### Wave 3.8：生产纹理厚度与单材料材质收口

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 22Q | 12E-09D-01 合同与配置映射 | `PREPARED / WAIT 03D` | 03D 当前范围完成 | 诊断参数与生产参数不再混用 |
| 22R | 12E-09D-02 生产纹理设置模型 | `PREPARED / WAIT 09D-01` | 09D-01 | Legacy 层数/等效厚度与 Global 物理宽度各自生效 |
| 22S | 12E-09D-03 单材料 Relief W/V Resolver | `PREPARED / WAIT 09D-02` | 09D-02 | white/varnish 材料合同与闭环一致 |
| 22T | 12E-09D-04 Qt 生产控件 | `PREPARED / WAIT 09D-03` | 09D-03 | 用户能区分诊断宽度和生产设置 |
| 22U | 12E-09D-05 一键切片、报告和 Smoke | `PREPARED / WAIT 09D-04` | 09D-04 | Effective Config、package、preview/report 同源 |
| 22V | 12E-09D-06 Release 矩阵与阶段收口 | `PREPARED / WAIT 09D-05` | 09D-05 | 真实模型、W/V、Legacy/Global 支持范围和 RIP strict 证据 |

### Wave 4：Stage 12 最终收口

| 序号 | 任务 | 状态 | 前置 | 完成 Gate |
|---:|---|---|---|---|
| 23 | 12E-10A Texture/Fill/Partition 同层预览 | `READY / WAIT USER AUTHORIZATION` | 13C-03、09A-05/06 | 生产/诊断同层口径 |
| 24 | 12E-10B 真实 OBJ/3MF 模式矩阵 | `PREPARED / WAIT 10A` | 10A | 真实模型证据 |
| 25 | 12E-10C Release/repair/peak-memory 汇总 | `PREPARED / WAIT 10B` | 10B | 最终工程矩阵 |
| 26 | 12E-10D 用户手册、REPORT 和上下文封口 | `PREPARED / WAIT 10A..C` | 10C | 12E COMPLETE |

`12E-10A..D` 的独立 PRD/DEV/DEMO/PREP/TASKS/CODEX_PROMPT 已于 2026-07-29 补齐；准备完成
不等于代码完成；09A-06 已完成，10A 当前等待用户明确授权。

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

因此，Stage 13 的 P0 需求分析、总体设计和原子任务准备已经完成。13A 和 13B 功能开发已经闭环，
批量导入、显式场景 CLI、Qt 当前场景主动作和真实模型作业流矩阵已经完成。`13C-03` 已实现，
13C、13D-01..04、13E-01..05、13G-00..07 与 12E-09A-01..06 已完成。当前没有未完成的
13G 原子任务；`03D-01` 已完成，`03D-02` 是下一候选原子任务但等待明确依赖修改授权，
12E-09D 和 12E-10A 按顺序等待。

### 尚未完成

```text
Stage 13 原始范围 13A-01..05、13B-01..07、13C-01..05 已完成；13B-08-01..04 已完成，
13D 已完成；13B 正式设备生产证据仍为 INPUT_OPEN；
设备 buildVolume、原点和机器轴方向仍未提供；
22 实例生产性能预算仍未提供；
13A-R2/R3 和 13B-R4 只到 Epic，不具备开发级详细设计；
13B production GO 仍被外部 Gate 阻断。
```

这些未完成项不阻断 `13C-03` 开发，也不等于 Stage 13 已生产就绪。

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
CURRENT：03D-01 COMPLETE / 既有 Stage 12/13 回归保持；
COMPLETE：13A-01..05、13B-01..07、13B-04A、12E-09A-01..06；
M13-1：CANDIDATE PASS；
NEXT：03D-02 READY，等待明确依赖修改授权；
AFTER：03D -> 12E-09D-01..06 -> 12E-10A..D；
FROZEN：12G-TCWS 仅保留 RIP 白区合同评审，不实现；
AUTHORIZATION：13B-02 已按用户授权完成并原子提交；
13B-06：FIXTURE COMPLETE，单 package、scene report 和 RIP strict 已闭环；
13B-07：Debug/Release 功能矩阵完成；production Gate 继续等待设备输入和 22 实例预算；
13C-01：代码、定向测试、Debug 构建、UI self-test 和 Quick CI 完成；
13C-02：代码、定向测试、Debug 构建、UI self-test 和 Quick CI 完成；
13B-08-01..04：批量导入、显式场景生产服务、Qt 当前场景动作和真实作业流矩阵均已完成；
13C-03：TIFF 原生统一生产预览已完成；
13C-04/05：Preview IO 与阶段证据链已完成；
13B-08：FUNCTIONAL COMPLETE / PRODUCTION INPUT OPEN；
13D：13D-01..04 COMPLETE，工作台布局、持久化与响应式 Gate 已关闭。
```
