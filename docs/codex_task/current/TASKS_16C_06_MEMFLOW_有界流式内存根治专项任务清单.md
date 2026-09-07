# TASKS_16C-06-MEMFLOW 有界流式内存根治专项任务清单

> 文档状态：**ACTIVE / 两项原始阻塞均已实测解除 / 余 MF-03X2b、MF-07、MF-08（均非阻塞）**
> 版本：v4.13 ｜ 日期：2026-09-07
> 定位：Stage 16C-06 的唯一原子任务状态真源；承接 12F-06 和 13B-05 流式化债务
> 决策：`docs/slice/DOC/DOC_DECISION_16C_06_MEMFLOW_有界逐层流式内存根治.md`
> 方案：`docs/slice/DEV/DEV_16C_06_MEMFLOW_有界逐层流式切片设计.md`
> B4B 准备：`docs/slice/DOC/DOC_PREP_16C_06_MEMFLOW_MF_03B4B_材料最终重放实施准备.md`

---

## 1. 固定边界

```text
不改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print
不改 Legacy/S0/P0 默认语义，不解锁 16B-04 / 16D-05
不改 SPI v1、11 导出、15 能力、Worker 文件合同
不默认启用 OpenVDB，不引入第三方依赖
先 Dense Streaming，再多实例 Barrier，最后 Sparse
任何输出漂移都停止扩大接入并保留 Retained Dense
正式设备 SLA/内存上限缺失时最终 production Gate 保持 INPUT_OPEN
```

## 2. 状态表

| 卡号 | 任务 | 状态 | 依赖 | 完成日期 |
|---|---|---|---|---|
| MF-00 | 决策、方案、准备、数据上下文同步 | COMPLETE | 用户授权 | 2026-08-18 |
| MF-01 | RasterMemoryBudget 估算与路由纯合同 | COMPLETE | MF-00 | 2026-08-18 |
| MF-02 | Owned Layer Producer/Sink 合同 | COMPLETE | MF-01、MF-02/03 准备补充 | 2026-08-18 |
| MF-03A | Occupancy Range/单层 Materializer | COMPLETE | MF-02 | 2026-08-18 |
| MF-03B1 | Range-derived Support Demand 与 caller-owned pre-shape 单层物化 | COMPLETE | MF-03A、B1 合同 Gate | 2026-08-19 |
| MF-03B2 | Geometry/outer-boundary 与 unsupported discovery 扫描 | COMPLETE | MF-03B1、B2 retained oracle Gate | 2026-08-20 |
| MF-03B3 | Shape/footprint 扫描、compact report 与 replay digest | COMPLETE | MF-03B2、重放 Gate | 2026-08-21 |
| MF-03B4A | Verified support/BaseProjection/outer-varnish 最终重放 | COMPLETE | MF-03B3、B4A 准备 Gate | 2026-08-21 |
| MF-03B4B | Material/Stage 15/closure 最终重放 | PREPARED | MF-03B4A COMPLETE、B4B 组合 Gate | - |
| MF-03X1 | 主循环有界接线·表面光油容器（逐层独立） | COMPLETE | MF-03B4B 接口接线 | 2026-09-04 |
| MF-03X2a | 主循环有界接线·**用户阻塞配置**（bottom_projection，无岛/无形状/无光油） | COMPLETE `6787930` | MF-03X1、MF-03B1、MF-03A | 2026-09-04 |
| MF-03X2b①| 准入放开·full vertical projection | COMPLETE（四判据逐字节全等） | MF-03X2a | 2026-09-06 |
| MF-03X2b②| 准入放开·shape 档 | **DEFERRED / 已定责**：见 9.6，收益只剩一半且需求面窄 | MF-03X2b① | - |
| MF-03X2b②-0 | shape 档前置：堵静默空转 + 消除逐字副本 + 补对拍盲区 | COMPLETE（不改行为） | MF-03X2b① | 2026-09-06 |
| MF-03X2b | 主循环有界接线·支撑耦合簇全模式（岛发现 + 形状 + 光油） | PREPARED / 范围待估算 | MF-03X2a、B2/B3/B4A | - |
| MF-03X3 | 稀疏列剪枝·compose 与内部空腔（用户 2026-09-05 提出耗时优化） | COMPLETE（第一批） | MF-03X2a | 2026-09-05 |
| MF-03X4 | 按幅面固定开销清理·relief_columns 归还与 compose 缓冲稀疏重置 | COMPLETE `5c6b8b7` | MF-03X3 | 2026-09-05 |
| MF-03X5 | 通道统计稀疏化（空列份额解析补齐） | COMPLETE `b8e082e` | MF-03X4 | 2026-09-06 |
| MF-04 | 单实例流式 Staged Package | **COMPLETE**（补测试时顺带修掉一处 Begin 阶段越界，见 8.2） | MF-03B4A/B COMPLETE | 2026-09-06 |
| MF-05 | 多实例 Global Layer Barrier | **COMPLETE** `7d7a627`（峰值与层数脱钩） | MF-03X2a | 2026-09-06 |
| MF-06 | Sparse Tile/Span 显式候选 | **已跳过**（用户 2026-09-04 同意；X3 稀疏列剪枝已取走核心收益） | MF-05 | - |
| MF-07a | Worker 权威内存 telemetry | COMPLETE（此前硬编码为 0） | MF-05 | 2026-09-06 |
| MF-07b | Host 停止伪造 telemetry | **COMPLETE**（前半不再伪造 available；后半两类数据分区展示） | MF-07a | 2026-09-06 |
| MF-07c | 峰值预估器（纯函数） | COMPLETE（四实测点全部高估覆盖） | MF-07a | 2026-09-06 |
| MF-07d | 预算字段与开始前路由 | TODO —— 接生产前须先有多材质大幅面实测点 | MF-07c | - |
| MF-07e | 消除场景路径中途回退 | **CLOSED**：验收已实质满足，见 11.4 | MF-05 | 2026-09-06 |
| MF-08 | 真实模型、RIP、恢复与性能收口 | PENDING / INPUT OPEN | MF-07、设备输入 | - |
| MF-09 | 层循环耗时优化 | **第一档 COMPLETE（View + workspace 复用，−11.6%，CLI+报告口径）；⚠ 对生产路径收益为零（§14.1.9）；三步剪枝已挂起（见 §14.4.1）** | MF-03X3 | - |
| MF-10 | MATVOL 逐列求交优化（多材质大栅格 gridSetup 占 92%） | **NEW / 待估** | - | - |
| MF-11 | 层循环并行化 | **DEFERRED / 已定责**：产品路径被屏障锁步，见 §14.3 | MF-09 实测曲线 | - |
| MF-12 | 生产路径耗时构成测量 | **COMPLETE**：见 §14.4.1 —— 层计算只占 14%，包发布占 48% 且其中 99.9% 是读回校验 | MF-09 | - |
| MF-13 | 包发布的读回全量校验 | **a+b 档 COMPLETE**：生产口径端到端 `publish −52.9%`、`total −29.8%`（见 §14.5.4）；已定性为 **CPU 界限** | MF-12 | - |

## 2.1 当前可继续的任务（2026-09-06 盘点）

> 盘点口径：以 git 提交为准核对，而非沿用表内旧状态 —— 本次盘点前
> MF-03X2a 与 MF-03X4 已完成却仍标 PREPARED，MF-05 的六个子步骤也未反映。

### A. 直接解除用户阻塞（最高优先）

| 任务 | 内容 | 剩余量 | 收益 |
|---|---|---|---|
| **MF-05 步骤4 第二步** | 写入侧流式：能力摘要改用合成统计 + 服务接线 | 两处改动 + 一轮验证 | 双模型峰值 130 GB -> 百 MB 级，**用户双模型可用** |

这是本专项唯一还在「直接失败」的问题。前置全部就位（写入会话、合成逐层出入口、
补齐与写出分离），且已确认能力摘要不必新造统计（见 9.5.5）。

### B. 收益明确但不解阻塞

| 任务 | 内容 | 剩余量 | 收益 |
|---|---|---|---|
| MF-03X5 | 通道统计稀疏化 | 一处改动 + 一轮验证 | 单模型耗时再降约 1/3 的一份整幅面往返 |
| MF-03X2b | 支撑耦合簇全模式（岛发现/形状/光油档） | 未估算 | 让这些档也享受 X2a 的内存收益 |

MF-03X5 的风险已登记：改错会让通道统计**悄悄偏差**而非报错，需单独一轮验。

### C. 专项收口类

| 任务 | 内容 | 说明 |
|---|---|---|
| MF-03B4B | Material/Stage 15/closure 最终重放 | 接口已接线（`77b19cf`），主体未做 |
| MF-04 | 单实例流式 Staged Package | **范围已重定义**：CLI 路径本就逐层流式，本卡只剩原子发布/staging 语义，而那已由写入器 session 化（`46fedc2`）覆盖大半 |
| MF-06 | Sparse Tile/Span | 用户 2026-09-04 已同意跳过；X3 的稀疏列剪枝已取走其核心收益 |
| MF-07 | 自适应生产路由与 Telemetry | 需先有 MF-05 完整落地作为路由目标 |
| MF-08 | 真实模型/RIP/恢复/性能收口 | INPUT OPEN：正式设备 SLA 与内存上限仍缺 |

### D. 建议顺序

```text
[x] MF-05 步骤4 第二步     已完成，双模型阻塞解除
[x] MF-03X5               已完成
[x] MF-03X2b 第一档        full vertical projection 已放开
1   MF-07                 自适应生产路由，MF-05 已提供路由目标
2   MF-03B4B 主体
3   MF-03X2b shape 档      **需先定剪枝策略，见下**
4   MF-08                 待设备输入
```

**MF-03X2b shape 档已定责暂缓，不是遗漏。** 只读静态查证确认 R-8 成立
且比预判严重（详见报告 §5.12.2）：有邻域写入的是四项运算而非只有膨胀；
后果是「输出少一圈 + 缓冲永久残留 + 统计逐层放大」，一少一多会互相掩盖。
放开前必须先选定剪枝策略，推荐 (a) shape 档一律不剪枝 —— 无需等价性论证，
且保留 X2a 的全部收益。`base_projection` 与 `outer_varnish` 两档属于
「要先有有界替代品」，与 shape 档不同类，本轮未评估。

**顺序已变：** 原表把 MF-03X2b 排在 MF-07 之后（理由是「非阻塞」）。
现在阻塞项已清空，改按「风险早暴露」排 —— X2b 要动的是支撑几何的准入判据，
是本专项剩余项里唯一还可能产生逐字节漂移的一项，宜先做完再谈路由。

### E. 原始阻塞验收（2026-09-06）

本专项由两项真实生产阻塞发起，两项均已实测解除：

| 阻塞 | 改前 | 现在 |
|---|---|---|
| `a-2/0.2.obj` @10um 耗时约 20 分钟 | 18.9 分钟 / 22~34 GB | **9.44 分钟 / 0.84 GiB** |
| `0.2.obj`+`0.3.obj` @10um 内存不足失败 | 外推 270 GB，**必然失败** | **`valid=1` / 1.91 GB / 31.3 分钟** |

验收口径的边界（耗时无改前对照、峰值取工作集为上界、单次运行未做 min-of-N）
见报告 §5.11.4。

---
## 3. MF-00 文档与上下文同步

**目标：** 把对话中的根因、范围、顺序、风险、数据同步和验收转为仓库真源。

**出口：**

```text
Decision、DEV、PREP、TASKS、执行指令、状态报告互链；
Stage 16 总任务把 16C-06 指向本清单；
docs/slice、docs/codex_task、AGENTS Active Work Entry、project profile 状态同步；
不覆盖并行 RIPFLOW 改动。
```

## 4. MF-01 RasterMemoryBudget 纯合同

**目标：** 用溢出安全纯函数估算主要 Raster 内存下界，并只在能力已实现时选择 Bounded 路由。

**实现：**

```text
新增 RasterMemoryBudget.h/.cpp；
route = retained_dense | bounded_dense_stream | blocked；
预算 0 保持 retained_dense；
估算只覆盖 RGBWSV + 调用方声明的 Mask 字节；
不接入 Production Service，不改变当前作业行为。
```

**验收：**

```text
无效维度/实例/窗口 fail closed；
全部 uint64 乘加防溢出；
小 Grid 在预算内选择 retained；
123 大 Grid 在 4 GiB 预算且 bounded 能力可用时选择 bounded；
能力不可用或窗口仍超预算时 blocked；
Release 定向 CTest PASS。
```

**实际结果（2026-08-18）：** 新增 `RasterMemoryBudget` 纯 C++20 合同和独立 CTest。预算未设置
时保持 `retained_dense`；只有显式声明 Bounded 能力并且窗口估算在预算内时才选择
`bounded_dense_stream`，否则 `blocked`。全部乘加执行 `uint64_t` 溢出检查。对观测 Grid
5197 x 1418 x 141、4 B/pixel Mask、三层窗口的估算为：Retained RGBWSV
6,234,466,716 B、主要 Raster 下界 10,390,777,860 B、Bounded 窗口 221,080,380 B；在
4 GiB 预算下选择 Bounded 候选。独立用例 8/8 PASS，Release CTest 1/1 PASS。该合同尚未接入
Production Service，不改变当前作业路径。

## 5. MF-02 Owned Layer Producer/Sink

**目标：** 建立同步、拥有语义、可背压的内部 Layer 合同；测试消费，不切生产路由。

**验收：** 层 buffer 地址可移动、层序/取消/消费者失败明确、旧 const callback 保持兼容。

**准备补充：** `DOC_PREP_16C_06_MEMFLOW_MF_02_03_开发准备补充.md` 已冻结 callback 互斥、
最后读取后移动、owned 模式禁止 producer 写出、稳定错误映射、同步背压和部分结果清理。

**实际结果（2026-08-18）：** Legacy 新增同步 owned callback 和稳定
`Accepted/Cancelled/Failed` 结果；旧 const callback 保持兼容，两者互斥。owned payload 只在 producer
最后读取后移动，owned + producer 写出、Global 路由和非法组合均提前拒绝。Legacy Scene Adapter
改用移动交接，并在取消/失败/异常时清除部分层；非法消费者状态稳定映射为 `Failed`。独立用例
7/7、Release CTest 1/1 PASS；既存
scene adapter 平移断言仍为修改前相同的 1 项失败，其余 11 项 PASS。

## 6. MF-03A Occupancy Range/单层 Materializer

**目标：** `LayerOccupancyProvider` 保存 primary/四个 subsample 的独立层区间，并物化到调用方提供的
单层 buffer；旧完整 Mask 实现保留为独立 Retained 对照，不接生产路径。

**验收：** S0/S3/S4 全层零差异，非连续阈值不被 min/max 填平，buffer/层号错误 fail closed。

**实际结果（2026-08-18）：** 新增 primary/四 subsample 独立 compact ranges 与 caller-owned 单层
materializer；range DTO 构造后不可变并仅在构建时完整校验，热路径不再逐层重复全量校验。
S0/S3/S4 对独立 Retained 实现逐层逐字节相等，阈值内部空洞保留，调用方 buffer 地址复用。
非法层号、buffer 尺寸、input kind 和 Layer Slab GeneralMesh 均 fail closed；新增 257 列 x 32 层
确定性生成对照。Release 构建及 `stage16_layer_occupancy_provider_tests` PASS；该 API 尚未接入生产路径。

## 7. MF-03B Support/varnish/material 多遍重放

完整 MF-03B 继续保持未准入，并拆为 MF-03B1..B4。MF-03B1 只建立由既有层范围事实驱动的
Support Demand 纯合同和调用方持有的 pre-shape 单层物化；不检测岛、不执行 shape/baseProjection/
varnish/material，不接 `run_slicer`。MF-03B2..B4 的遍次和语义边界以开发准备补充文档为准。

**MF-03B1 验收：** 四类 pre-shape 支撑开闭区间和优先级与独立 brute-force 对照全层零差异；
General Mesh、错误范围、错误层号或 buffer 尺寸 fail closed；重复物化确定且复用调用方 buffer；
Release 定向测试和 MF-03A 回归通过。

**MF-03B1 实际结果（2026-08-19）：** 新增 move-only `BoundedSupportDemandPlan`、双输入 Mask 的
caller-owned 单层物化和公共 `SupportTypePriority`；计划构建期完整校验事实，逐层热路径无分配，
输入/输出任意字节区间重叠、非二值 Mask、错误边界及 General Mesh 均 fail closed。257 x 32
确定性 heightfield 对 retained 四阶段循环逐层比较，Mask/Type 零差异；输出复用、强异常边界和
计划持有事实均有独立用例。Release `/W4 /WX` 构建通过，MF-03B1、MF-03A、MF-02 CTest 3/3
PASS，既有 support shape 11/11 PASS，`slicer_cli` 链接通过。新合同未接 `run_slicer`，生产仍为
Retained Dense。

**MF-03B2 准备裁决（2026-08-19）：** B2 仅新增两个顺序、bounded、非生产合同：P1 逐层把
model 物化为 outer-varnish/upper-boundary 并累计 `upperBoundaryLastLayer`；P2 使用 B1 的
lower/full/upper preliminary support、previous model 与当前 model 检测 unsupported island，输出
逐列 `unsupportedTopExclusiveLayer` 和逐源层事件摘要。P2 的 previous base 禁止包含此前发现的
unsupported、InternalVoid、Shape、BaseProjection 或 varnish。调用方中止时不得 Finish 半结果；
General Mesh、乱序层、错误 Mask/Policy 均 fail closed。B2 不接 `run_slicer`，不改变 retained 生产。

**MF-03B2 实际结果（2026-08-20）：** 新增 move-only P1/P2 顺序 scanner。P1 使用调用方复用的
outer/upper buffer，按 retained 的 8 邻域外部空域与物理椭圆膨胀生成同层 outer varnish，并只保留
逐列 upper-boundary last；P2 仅从 B1 preliminary plan 重放 Bottom/Full/Upper previous support，输出
compact unsupported top 与逐源层事件。独立 dense retained oracle、4/8 连通、严格 overlap/area
阈值、previous-base 排除域、乱序/提前 Finish/非二值/别名/GeneralMesh/upper 包含 model 均通过。
两个 `ConsumeLayer` 热路径由分配计数验证为零堆分配。Release Ninja 核心源和测试 `/W4 /WX`
构建通过；B2/B1/03A/02 与 outer-varnish CTest 5/5 PASS，support shape 11/11 PASS，`slicer_cli`
链接并报告 `0.2.0-dev`。新 scanner 仅由测试引用，未接 `run_slicer` 或 Production Service。

**MF-03B3 准备裁决（2026-08-20）：** B3 只实现非生产 P3 scanner。固定顺序为 B1 final plan
逐层物化、InternalVoid、Shape、SupportType 同步、post-shape footprint OR、canonical replay digest；
禁止提前执行 BaseProjection、outer/surface varnish priority、最终统计、material/closure 或写包。
InternalVoid 与 Shape 完全沿用 retained 的连通、阈值、操作顺序、回滚和报告口径；报告通过同步 sink
逐层交接且不保留 component pixel 列表。Result 仅保留 O(XY) footprint、逐层固定长度 digest 和总计。
错误输入必须在状态修改前 fail closed，同层可修正重试；未完整消费不得 Finish。B3 不接
`run_slicer`、Production Service、Profile、SPI 或 Worker。

**MF-03B3 实际结果（2026-08-21）：** 新增 move-only `BoundedSupportShapeScanner`，按冻结顺序执行
B1 final demand 单层物化、InternalVoid、retained Shape、SupportType 同步、post-shape footprint 和
canonical SHA-256 replay digest。compact report 通过同步 sink 交接且不保存 component pixel 列表；
Result 只保存 O(XY) footprint、逐层 32-byte digest 和 totals。错误层序、尺寸、非二值、别名、
GeneralMesh、提前 Finish 均 fail closed；sink 异常会终止 scanner 且不提交 caller output。
独立 retained oracle 对 support/type/footprint/compact report 字段全等；mask-only 热路径零分配、
caller buffer 地址复用、固定 canonical digest golden 及 input/policy 域覆盖通过。重复 Finish、Finish 后
Consume 和失败 sink 后继续调用均 fail closed。
Release `/W4 /WX` 构建通过；B3/B2/B1/03A/02 与 outer-varnish CTest 6/6 PASS，既有 support shape
11/11 PASS，`slicer_cli --version` PASS。新 scanner 未接 `run_slicer`、Production Service 或应用；
生产仍为 Retained Dense。

**完整验收：** S0/S3/S4、support/type/baseProjection/varnish、Stage 15/closure 逐层 diff=0；
真实模型不减少连通分量或支撑连续层。

**MF-03B4A 准备裁决（2026-08-21）：** B4A 只实现非生产 verified support final replay：复用 B3
canonical 路径在 caller output 前逐层校验 digest；随后按 B3 footprint 应用 BaseProjection，保留
`prepend_below_model` 前 N 层 type 覆盖；再按 outer-varnish priority 清 support/type 并输出 cleared
overlap evidence；最后累计 final support/type/connectivity。Result 不保留 layer x pixel 栈，不接
`run_slicer`、Production Service、Profile、SPI、Worker、TIFF、preview 或 report 文件。

**MF-03B4A 验收：** disabled/overlay/prepend/clamp/model priority/type 覆盖、Base 后 varnish 清理、
cleared evidence、final totals/connectivity 与独立 retained oracle 逐层零差异；identity/digest、层序、
尺寸、二值、别名、sink/Finish/cancel fail closed；caller buffer/scratch 复用；Release MEMFLOW、outer
varnish、support shape 与 CLI 回归通过，生产未接线。

**MF-03B4A 实际结果（2026-08-21）：** 新增 non-production
`BoundedSupportFinalReplayScanner`。B3 replay identity 现包含 B1 final plan 的 canonical SHA-256，
verified Consume 在内部 report sink、caller output 和状态提交前校验逐层 digest；通过后严格执行
BaseProjection、outer-varnish support/type 清理及 final support/type/connectivity 扫描。逐组件
area/bbox 仅以同步 span 交接，Result 只保留 Base summary 与 O(1) totals，不保留 layer x pixel 栈。
独立用例 10/10 覆盖 disabled/overlay/prepend/clamp/model priority、dense Base oracle、4/8 连通、
identity/digest、生命周期、别名/二值/尺寸、sink 异常与 caller buffer 地址复用。Release `/W4 /WX`
目标构建通过；MF-02/03A/03B1/03B2/03B3/03B4A + outer-varnish CTest 7/7 PASS，既有 support shape
11/11 PASS，`slicer_cli --version` 报告 `0.2.0-dev`。全仓生产引用检查为空，未写 Package/RIP，
生产仍为 Retained Dense。

**MF-03B4B 准备裁决（2026-08-21）：** B4B 独立消费 B4A 同层结果，顺序固定为 compose -> Stage 15
white carrier -> closure exact -> optional repair -> re-detect -> repair 后 channel/semantic/material totals。
被 outer varnish 清掉的 support 只恢复进 closure `supportRequiredMask`，不进入 final support stats。
B4B 不写包、不接生产；只有 B4A COMPLETE 后才可单独开工。

**MF-03B4B 准备补充（2026-08-21）：** 多 Agent 只读审计发现原准备未冻结 public DTO、facts
identity、caller output 提交时机、取消/错误状态机及 closure workspace，先判定 NO-GO 并补充专项 PREP。
现已冻结 retained canonical helper 提取、11 个 semantic mask、canonical digest、sink 成功后提交、销毁式
取消、固定 workspace/热路径零分配、独立 retained oracle 和 Release 组合 Gate，结论转为
`PREPARED / IMPLEMENTATION GO`。Stage 15 保留 retained eligible-branch 语义，MaterialPolicy/texture/
Stage 15 counter 保留 compose-time 口径；生产仍为 Retained Dense。

## 7.1 MF-03X 主循环有界接线

来源：`docs/slice/REPORT/REPORT_16C_06_MEMFLOW_主循环接线可行性探查_2026_09_04.md`。

该探查推翻两个此前认知：`slicer_cli` 的 TIFF 输出**早已逐层流式**（故 MF-04 的
内存收益不成立，其范围重定义为原子发布/staging 语义），真正瓶颈是七个 mask
整栈驻留 —— 10um 场景下合计 **72.62 GB**，而三层窗口只需 156 MB。

**故 MF-03X 是接线工作，不是新能力开发**：B1/B2/B3/B4A/B4B 的有界替代品全部
已 COMPLETE 且各有 retained oracle，但此前没有一处接进 `slicer.cpp` 主循环。

### MF-03X1 表面光油容器（COMPLETE，`0d2a1bf`）

**目标：** `outer_surface_masks` / `inner_surface_masks` 由全层构建改为按层物化。

**选它作起点的理由：** 该算法逐层独立 —— 每层只读本层 model mask，层间无依赖，
故**不涉及** MF-03B2「悬空岛向下回写」那类时序等价风险；两容器各只有 4 个使用点。

**验收（已通过）：**

```text
零漂移  默认路径 94 层 3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
        与长期基线一致
        gubao04 六材质 129 层 8315b63c42e3f6a90faef6aa693f4d9f3443e95af1245268b86d8cf812a2aee1
        与接线前逐字节一致
门禁    接线净增 127 行，同步下沉表面光油几何簇后净减 137 行（5,988 行）
        注：slicer.cpp 早在 642d29e 已登记 G2 豁免，门禁两种情况都会 PASS，
        下沉并非门禁所迫；但该豁免 reason 只覆盖 MATOPQ，不含 MEMFLOW，
        故本专项不依赖它，坚持每次接线同步下沉（见报告 §6 第 3 条）
```

**收益界定（不可夸大）：** 这两个容器仅在 `surface_varnish.enabled` 时分配，而该
字段**默认 false**。用户的 10um 阻塞配置未开启表面光油，故本卡**对该阻塞场景收益
为零**。它的价值是给开启光油的工艺（多图层透明→光油等预设）拆掉这个天花板。
详见探查报告 §3.1。

**副产物（后续共同前置）：** `geometry/SliceGridSpec.h` 提出 `GridSpec` 与
`mask_index`。它们原在 `slicer.cpp` 匿名命名空间内，是**所有** mask 构建函数的
共同参数类型；提头后，后续任何 mask 构建函数下沉都不再需要先解决符号共享。

**同时删除：** 全层版 `BuildSurfaceVarnishMasks()` 与 `struct SurfaceVarnishMasks`
—— 接线后已无调用者。原注释称「保留作零漂移对照」，但无人引用，留着只是负债。

### MF-03X2 支撑耦合簇

**范围：** `model_masks`、`support_masks`、`support_type_maps`、
`outerVarnishMasks`、`upperBoundaryMasks`。

**为什么不能继续「逐容器替换」：** 这六个全部通过一个整栈进、整栈出的函数耦合：

```cpp
// slicer.cpp:2031
SupportGenerationResult generate_support_masks(
    ..., const std::vector<std::vector<std::uint8_t>>& model_masks,
         const std::vector<std::vector<std::uint8_t>>& upper_boundary_masks, ...);
```

且 `model_masks`（41 处引用）、`support_masks`（19 处）、`support_type_maps`
（12 处）**各有恰好一处** `.at(target_layer)`，都落在同一个悬空岛向下回写循环内。
只换其中之一，那处回写就会失去其余两者的整栈视图。

**故替换单元不是「容器」，而是「把 `generate_support_masks` 整体改走
B1/B2/B3/B4A 的有界路径」。** 工作量不可按 MF-03X1 线性外推。

**验收：** 沿用 §5.2 判据 —— 三项零漂移全等 + `a-2/0.2.obj` @10um 的
`peakWorkingSetBytes` 由 22~34 GB 降至百 MB 级 + `model/stl/suoguo-baseline/`
八项既有基线不上升。

#### MF-03X2a 用户阻塞配置（PREPARED / 解除阻塞关键路径）

**为什么可以先只做这一档：** 逐项核对用户 `a2_probe.json` 配置下的实际激活项后
发现，本卡涉及的三个耦合难点**在该配置下全部不激活**：

| 难点 | 守卫条件 | `bottom_projection` 下 |
|---|---|---|
| `.at(target_layer)` 悬空岛向下回写 | `placement_policy.unsupported_only_enabled` | **false**（`support_mode_includes_unsupported()` 只认 `unsupported_only` 与 `bottom_projection_plus_unsupported`） |
| 支撑形状优化整栈进出 | `support_shape_policy.enabled` ← `shape_enabled` | **false**（`config.h:312` 默认 false，探针未设） |
| `outerVarnishMasks` / `upperBoundaryMasks` | 光油离散化 / `includes_outer_varnish_shell` | **false**（探针未配 `outerVarnish`） |

**故该配置下不存在任何无界随机访问。** 剩余的向下遍历只有 bottom-projection 的
`for (layer_index in [0, lower_layer))`，它是**按列**的 —— 每列填到该列最低模型层，
是 `support_source_layers` / `column_ranges` 的纯函数，而这两个归约主循环**已在算**。

**关键推论：本档不需要 B2/B3，故报告 §6 第 1 条「B2 时序等价是最大风险」在本档
不适用。** 所需能力是 MF-03B1（Range-derived Support Demand）+ MF-03A
（LayerOccupancyProvider）—— 两者均已 COMPLETE。

**目标：** 该配置下 `model_masks` / `support_masks` / `support_type_maps` 三个整栈
（31.11 GB）改为按列区间推导 + 按层物化。

**验收：**

```text
零漂移  三项既有判据全等（默认路径 94 层 / gubao04 129 层 / tm2-5 全通道）
        —— 注意默认路径与 gubao04 均为 bottom_projection，本档直接覆盖
阻塞    a-2/0.2.obj @10um 的 peakWorkingSetBytes 由 22~34 GB 降至百 MB 级，
        totalMs 由 20~31 分钟显著下降（换页消失）
        0.2.obj + 0.3.obj 双模型不再内存不足（若仍不足则需 MF-05 Barrier）
回归    model/stl/suoguo-baseline/ 八项既有基线（2,658~3,316 MB）不得上升
守卫    非 bottom_projection 配置必须仍走 retained 路径，按 mode fail-safe 分流，
        不得把未验证的有界路径应用到岛发现/形状/光油档
```

#### MF-03X2b 全模式（PREPARED / 范围待估算）

**范围：** X2a 之外的档 —— `unsupported_only`、
`bottom_projection_plus_unsupported`、`full_vertical_projection`、
形状优化开启、两种光油开启。

**最大风险：** B2 时序等价。「先全层 preliminary support、再顺序发现岛并向低层
回写」的时序必须逐字节等价，否则支撑连通性会变。B2 已有 retained oracle，但
**接线时的调用顺序**仍需逐层 digest 比对。

## 7.2 MF-03X3 稀疏列剪枝（COMPLETE 第一批）

**来源：** 用户 2026-09-05 提出「切片时间仍太久，20 分钟完成一次」。

**先测再改。** 层循环内插临时子计时器，用 a-2/0.2.obj @0.1mm（143 层，
7,369,346 列）取分布：`compose_layer` 55.9%、内部空腔洪泛 32.5%，两者占 88%。

**根因：** `relief_report` 实测只有 **186,103 列（2.53%）**有模型，
其余 97.47% 在任何一层都既无模型也无支撑，而每层每一遍都扫满幅面。
包围盒剔除无效 —— 模型 bbox 就是整块幅面，它只是稀疏。

**判据（精确等价，非近似）：** `compose_layer` 的分支链没有末尾 else，
三个 mask 皆零的列不写任何字节。活动表须覆盖三者并集，且**不能**简单取
「有模型的列」—— 面内被围住的空腔（环形件孔心）在所有层都无模型却要写支撑。
`BuildBoundedActiveColumns` 改按「无模型且能经无模型列连到边界」排除。

**效果（a-2@0.1mm）：** `layerComputeMs` 101,014 -> 46,726（-53.7%），
`totalMs` 106,585 -> 52,145（-51.1%）。三判据逐字节全等。

**一处反直觉发现：** 只做剪枝而不复用缓冲，内部空腔耗时几乎不降 ——
真正的开销是每层新建两个 7.37 MB 缓冲。**按幅面计的固定开销与按占用计的
工作量是两笔账**，只算后者会得出错误结论。

## 7.3 MF-03X4 按幅面固定开销清理（PREPARED）

**范围：** 同类「每层新建整幅面缓冲」还剩三处，合计仍占每层约 300 ms：

```text
compose_layer 的输出缓冲       每层新建 w*h*6 = 44.2 MB，当前最大单项
model / support 单层物化       每层 std::fill 整幅面后只写活动列
analyze_support_connectivity   每层新建 w*h 的 visited
                               （已下沉为 support/SupportConnectivityAnalysis 备改）
```

**改法：** 与 MF-03X3 第 3 点相同 —— 缓冲跨层复用、每层只重置活动列。

**未做原因：** 本轮先交付已验证的部分，不是判断它们不值得做。

**验收：** 沿用零漂移四判据 + a-2/0.2.obj @10um 的 totalMs 继续下降。

## 8. MF-04 单实例流式 Package

> **范围已重定义（2026-09-04）。** 探查报告实测 `slicer.cpp:5364` 的
> `WriteRgbwsvProductionLayerTiff` 位于层循环【内部】，`layer` 为本层局部变量、
> 写完即释放，`RgbwsvProductionLayerView` 只持 `std::span` 不拷贝 ——
> **`slicer_cli` 的生产路径早已逐层流式，不累积。**
> 原描述「Writer 累积全部层后逐层写」只对整包发布入口
> `WriteRgbwsvProductionPackage` 成立，CLI 不经过它。
> 故本卡的**内存收益不成立**，价值重定义为「原子发布与 staging 语义」。
> 内存收益由 MF-03X 承担。

**目标：** 单实例 full-grid 从 Producer 逐层进入 staging Writer，取消与故障不发布半包。

**验收：** TIFF/manifest/report/preview/RIP strict 与 Retained Dense 全等；取消和 Writer 故障不发布
半包。~~Peak Working Set 明显下降~~ —— 该判据移交 MF-03X2。

### 8.1 复核：功能已由 MF-05 覆盖，缺的是测试（2026-09-06）

逐条对验收：

| 验收项 | 现状 |
|---|---|
| 逐层进入 staging Writer | **已实现**：`RgbwsvProductionPackageSession` 的 Begin/AppendLayer/Finish 三段，MF-05 已把它接进场景路径并默认启用 |
| 输出与 Retained Dense 全等 | **已验证**：四判据逐字节全等，digest 与长期基线相同 |
| 故障不发布半包 | **已实现**：会话未 Finish 即析构时 RAII 回滚清 staging；MF-05 期间顺带修掉「`Finish()` 从未置 `State::finished`，成功发布后析构仍跑一遍恢复」这处遗漏 |
| 取消不发布半包 | **行为已存在但缺专门测试**。MF-05 定位那十一项回归时，整条因果链的末端正是「包写一半被协作式取消 -> 会话 RAII 回滚清 staging -> 包从未发布」—— 那次是被误触发的，但它反过来证明该语义确实生效 |

**故本卡的剩余工作只有一项：补两个专门测试**（作业中途取消、Writer 故障注入），
把上表最后两行从「被一次误触发间接证明」变成「被断言直接钉住」。
不补也不影响现有功能，补了才能把这张卡关掉。

### 8.2 补测试时撞到一处真实缺陷（2026-09-06，本卡 COMPLETE）

先说一个查证结果：**`RgbwsvProductionPackageSession` 此前在测试里零覆盖。**
它是 MF-05 引入并【默认启用】的逐层发布入口，却只被场景路径的端到端用例
间接带到过。补测试的第一个动作就是给它写直接用例。

新增两条：

| 用例 | 钉住什么 |
|---|---|
| `abandoned_session_publishes_nothing` | 写一半层后**不调用 Finish** 直接析构 -> 不得留 manifest、不得留任何 `package.staging.*` 目录 |
| `finished_session_survives_destruction` | 正常 Finish 后包已发布；**并在会话析构之后**才检查产物，从而一并钉住「成功 Finish 后析构不再回滚」——那正是 MF-05 顺带修掉的 `State::finished` 遗漏 |

#### 缺陷：Begin 阶段会按 `grid.layerCount` 遍历空的 `request.layers`

第一条用例照生产用法写（逐层路径不预置整栈，`request.layers` 清空），
一跑就是 `0xc0000409`。查下去是真缺陷，不是用法不当：

```text
会话构造 -> ValidateRequest(request, false)      // 只关了 compositionReady
        -> writtenLayerCount 仍是 nullopt
        -> layersInMemory = !writtenLayerCount.has_value() = true
        -> for (layerIndex < request.grid.layerCount)
               request.layers.at(layerIndex)      // layers 恒为空 -> 越界
```

该循环上方的注释写着「逐层会话下 layers 恒为空……跳过它，否则这里必然越界」
—— **但跳过的判据在 Begin 阶段并不成立**。

**生产至今没踩到，纯属巧合**：会话建立时 `writeRequest.grid.layerCount` 恰好
还是 0（grid 由合成后补齐，见 `MultiModelProductionService` 的注释），
循环遍历 0 次。那是巧合不是保证 —— **一旦把 grid 的补齐提前（一个完全合理的
重构方向），Begin 就会当场崩**。

**修法一行**：`ValidateRequest(request, false, 0)`，显式声明「逐层模式、
已写 0 层」。第三参数为 0 时 `layersInMemory` 为假，循环被正确跳过；
而「层数齐备」那条校验由 `compositionReady` 守卫，Begin 时本就不执行，
故传 0 不会误判层数不足。

测试保持「清空 `request.layers`」的生产语义 —— 改成预置整栈也能过，
但那样这条回归保护就没了。

## 9. MF-05 多实例 Layer Barrier

> **升级为双模型阻塞的关键路径（2026-09-05）。** 依赖改为 MF-03X2a，不再等 MF-04
> —— MF-04 的范围已重定义为 CLI 路径的原子发布语义，与本卡无先后关系。

**目标：** 1/11/12/22 场景按 global layer 同步合成，释放同层所有实例 buffer。

### 9.1 为什么必须做（量化根因见报告 §5.6）

`MultiModelProductionService.cpp:813` 把**全部实例切完**才一起合成，而单实例
`SceneInstanceRaster` 持有**全部层**，每层含 6 通道输出加 4 张归属 mask：

```text
每列每层 10 B/实例
用户场景 1418 x 5197 x 1429  ->  103.7 GiB【每实例】，双模型 207 GiB
```

`LegacySceneLayerAdapter.cpp` 的 `ownedlayercallback` 逐层 `push_back` 且不释放，
使 MF-02「Owned Layer Producer/Sink」合同「消费即释放」的用意归零。

### 9.2 设计：生产者线程 + 层屏障

合成第 L 层只需各实例的第 L 层。障碍在于实例是 **instance-major** 切的，
而合成要 **layer-major**。`ownedlayercallback` 本来就是逐层交付的，
故只需让每个实例在自己的线程里跑，并在回调内等屏障：

```text
每实例一个生产者线程
  产出第 L 层 -> 存入该实例的单层槽 -> 等待「本层已被消费」信号
合成线程
  等齐全部 N 个槽 -> 合成为 global 第 L 层 -> 写出 -> 释放全部槽 -> 放行生产者

峰值 O(实例数 x 列数)，与层数无关
用户双模型 10um：2 x 44.2 MB + 合成缓冲 ≈ 132 MB（现为 207 GiB）
```

### 9.3 必须守住的语义

```text
确定性   合成顺序按 scene 内实例次序，【不按线程完成先后】，否则输出 hash 会抖
取消     任一线程 ThrowIfCancellationRequested 后，屏障须唤醒全部等待者并传播
异常     单实例抛错必须让屏障失效并让其余线程尽快退出，不得死锁
fail closed  重叠/冲突/stale/层缺失沿用既有 admission 判据，不因并发放宽
层数不齐  各实例 localgrid.layercount 可不同，屏障须按 global layer 对齐，
          缺层的实例在该层贡献空槽而非阻塞
```

### 9.4 范围修正（2026-09-05，实施前逐段读代码后）

§9.1 只算了每实例栅格，**低估了**。完整驻留链是三份整栈，且合成期间同时在场：

```text
N 份 每实例 SceneInstanceRaster   10 B/列/层 x N   -> 双模型 207.4 GiB
1 份 SceneLayerComposeResult      6 B/列/层        ->        62.2 GiB
                                                   合计 ≈ 269.6 GiB
```

`ComposeSceneLayersConsuming` 只覆盖**单实例快路径**（实为
`ComposeSingleInstanceConsuming` 的转发），多实例仍走 Borrowed，借用不释放。

**最关键的约束：合成侧流式化受类型不变量阻挡。**
`ValidatedSceneLayerComposeResult` 的构造要求
`layers.size() == grid.layercount` —— 「全部层同时在场」是该类型的**证据契约**，
存在目的是让下游 report/package 免于重扫每个 RGBWSV 字节。
故必须先把该证据由「一次性全量」改为「逐层累积」，否则要么破坏不变量，
要么让下游退回全量重扫（把省下的内存换成一次全量扫描）。

**这是触及既有架构不变量的改动，不是接线量级。**

### 9.5 分步与验收

> **范围第三次修正（2026-09-05）。** 实施编排层前查依赖，发现还有【写入器侧】：
> `WriteValidatedMultiModelSceneProductionPackage` 接收整个 `composition`，
> 内部交给 `WriteRgbwsvProductionPackage`（整包入口）。
> 好消息是逐层写 TIFF 的能力早就存在（`WriteRgbwsvProductionLayerTiff`，
> CLI 单模型路径一直在用），故这块是「让场景路径改用已有入口 + manifest/report
> 改逐层累积」，不是从零造能力。

> 三次修正的规律值得记下：**每次都发现范围比预想大，但同时也发现让工作变小的
> 东西**（合成主循环本就 layer-major、实例校验本就逐层、逐层写 TIFF 本就存在）。
> 隔着推测估工作量两个方向都会偏，只有读代码才准。

```text
步骤 0  SceneMemoryBench 多实例验证台                    COMPLETE
步骤 1  SceneLayerBarrier 纯同步原语 + 并发单测          COMPLETE 94fafd8
步骤 2  LegacySceneLayerAdapter 的 ownedlayercallback 改为存单层槽并等屏障
步骤 3  合成侧改逐层合成、合成即写出、不累积 layers
步骤 4  ValidatedSceneLayerComposeResult 的闭合证据改为逐层累积，
        使「已验证」不再等价于「全部层在内存里」
步骤 4b 写入器侧 session 化（见 §9.5.3）
步骤 5  实测用户双模型 0.2 + 0.3 @10um
```

### 9.5.1 验证台与「修好前」基线（实测）

`stage16c06_scene_memory_bench <层厚> <实例数>` 构造场景并跑生产服务。
判据不是绝对值，而是**峰值随层数的斜率** —— 用户场景要 270 GB，无法直接跑对照。

| 层厚 | 实例 | 层数 | 峰值 |
|---|---|---|---|
| 1.0mm | 1 | 15 | 2.12 GB |
| 0.5mm | 1 | 29 | 3.15 GB |
| 0.25mm | 1 | 58 | 5.29 GB |
| 1.0mm | 2 | 15 | 3.84 GB |

```text
单实例   每层 73.7 MB（两段增量 73.6 / 73.8，线性）+ 常数底约 1.0 GB
双实例   每层 189 MB（超过 2 倍：全局幅面也随实例并排而变宽）
外推     189 MB/层 x 1,429 层 = 270 GB，与 §9.4 静态推算的 269.6 GiB 吻合
```

**两条独立路径（读代码估算、跑基准外推）得到同一数量级，故根因判断可采信。**
修好后的判据：同样四组的峰值应与层数**无关**（只随实例数与列数变化）。

### 9.5.6 ✅ 流式发布已启用（2026-09-06 完成）

`kStreamingPackageWriteEnabled = true`。MF-05 目标达成。

#### 最终效果

| 场景 | 层数 | 峰值 |
|---|---|---|
| 单实例 1.0mm | 15 | 1.019 GB |
| 单实例 0.5mm | 29 | **1.019 GB** |
| 单实例 0.25mm | 58 | **1.019 GB** |
| 双实例 1.0mm | 15 | 2.02 GB |
| 三实例 0.25mm | 58 | 2.98 GB |

**峰值只随实例数与列数变化，与层数无关。** 对照改前：双实例仅 15 层就要 3.84 GB，
斜率 189 MB/层，用户 0.2+0.3 @10um 外推 270 GB。

全量回归 10 失败 / 229 —— 与关闭时完全一致，无新增失败。

#### 曾挡住它的两处，及其根因链

**一、发布收尾越界**（第九处整栈假设）：`ValidateRequest` 按 `grid.layerCount`
遍历 `request.layers` 做逐层内容校验，逐层路径下 layers 恒为空。
修法是把该段抽成 `ValidateProductionLayer`，两条路径共用 —— 整栈路径照旧在
`ValidateRequest` 里逐层调，逐层路径改由 `AppendLayer` 收到每层时调，
**校验强度不降级，只是时机从「全部到齐后一次」变成「到一层校一层」**。

**二、11 项回归**，根因是一条完整因果链，起点只是一个分母：

```text
会话在合成【之前】建立 -> request.grid.layerCount 此刻仍为 0
-> expectedLayerCount = 0 -> 逐层进度上报 current=N total=0
-> WorkerProtocol 判 current > total 为 file_contract_v1 语法违规
-> WorkerClient 据此写取消标记 -> Worker 的 FileCancellationToken 立刻为真
-> 包写到一半被协作式取消 -> 会话 RAII 回滚清掉 staging -> 包从未发布
-> E_PACKAGE_NOT_FOUND / missing key packageDir / baseline package was not published
```

第二处差异是时序：`scene_package_write` 的 78% 锚点原在合成之后，而逐层回调发生
在合成之中，事件序列成了 `72 -> 95 -> … -> 78`，Worker 又判「percent 倒退」。

**三处修复**：会话增 `SetExpectedLayerCount` 并由服务在栅格相位后按各实例
`localgrid` 的最大层数喂入；78% 锚点上移到会话建立之前（非流式保持原位，用
`has_value` 守卫避免重复上报）；进度去重条件 `current < total` 在 `total<=0` 时
恒不成立，改为 `std::max(total, 1)` 兜底。

**顺带修掉一处遗漏**：`Finish()` 从未把 `State::finished` 置 true，成功发布后
析构仍会再跑一遍 `RecoverPackageArtifacts`。当前那次恢复恰是无害空操作，
但与「成功 Finish 后不再回滚」的约定不符，且一旦恢复语义变化就会变成删已发布的包。

#### 一条方法论

这条因果链**不是靠逐轮试错找到的** —— 我上一轮基于「这些入口不走流式路径」的
错误前提试了一个候选，无效。真正定位靠的是一次只读的日志+源码静态推证
（并行 agent 完成），它同时纠正了两个前提错误：新增是 11 项不是 10 项；
`streamingInstances` 对单实例恒为 true，故开关一开几乎所有入口都进会话路径 ——
**入口分散是佐证而非反证**。
### 9.5.5 步骤 4 第二步（写入侧流式）的形状

第一步（实例侧流式）已合入 `853bf14`：单实例斜率 73.7 -> 44.2 MB/层。
剩余斜率来自合成结果累积（6 B/列/层），需让写入也逐层。

**已就位：** 写入会话 `46fedc2`、合成逐层出口 `c6e88aa`、
补齐与写出分离（本次）。

**时序难点与解法：** `PrepareMultiModelScenePackageRequest` 依赖 composition，
而会话必须在合成【之前】建立（layersink 要用它）。解法是利用会话持有 request
引用的特性 —— Begin 只用发布身份（packageDir/jobId/attemptId/preview），
合成后再补齐 grid/scene/能力摘要，Finish 时可见。

**第四处整栈假设（本次发现）：** `BuildSceneCapabilitySummary` 会遍历
`raster.layers` 统计每实例的 printPixels 与 min/max x/y/layer，流式下骨架为空。
这次不是校验而是**实质计算**，本以为要新造逐层统计。

**但不必新造：** `SceneInstanceComposeStatistics::RasterStatistics` 的字段
（`printpixels`/`emptypixels`/`minimumx|y|layer`/`maximumx|y|layer`/`grid`）
**完全覆盖**能力摘要所需，而合成侧的 `ValidateLayer` 在流式下已经逐层累积它们
（步骤 2b 的改动）。故只需让能力摘要在 `raster.layers` 为空时改用
`composition.statistics.instances` 里对应实例的统计。

**剩余清单：**

```text
1  BuildSceneCapabilitySummary 支持从合成统计取数（raster.layers 为空时）
2  服务：合成前建会话、layersink 里 AppendLayer、合成后 Prepare + Finish
3  验收：验证台四组峰值与层数【脱钩】；回归不新增失败
```

### 9.5.4 步骤 4：编排层的最终设计（三块前置已全部就位）

实例侧（2a）、合成侧（2b）、写入器侧（4b）均已合入且行为中性，
`MultiModelLayerComposeRequest` 的出入口透传也已打通。剩下只是把它们串起来。

**两处环形依赖，解法已确定：**

```text
一、屏障要按全局层号对齐，而 offsetz 由实例与全局栅格原点之差算出，
    全局栅格又要等所有实例的局部栅格就位。
    解：先跑栅格相位（adapter 的 gridready 在第一层之前触发），
        主线程等齐后检查【所有实例 localgrid.originzmm 相等】——
        相等则各 offsetz 必为 0，局部层号即全局层号，屏障可直接对齐。
        判据只看各实例自身，不需要先有全局栅格。

二、写入会话要在合成之前建立（layersink 要用它），而 writeRequest 里
    grid.widthPx/heightPx/layerCount 看似要等合成结果。
    解：实测 writeRequest 其余字段全部来自 contract 与 request，不依赖
        composition；故把会话【延迟到 layersink 首次调用】时创建 ——
        那时层的宽高已随层送达，全局层数由各实例 localgrid 推出。
```

**退回方案（最后一个设计点）：** z 原点不一致时不能报错（那会让本来能工作的
场景失败），也不能重跑切片。故让 sink 读一个已稳定的标志分流：

```cpp
adapterRequest.layersink = [&](SceneInstanceRasterLayer&& layer) -> bool {
    WaitUntilAlignmentDecided();          // 栅格相位在第一层之前完成，故不会久等
    if (streamingRejected) {              // 退回 retained：照旧累积整栈
        rasterSlots[i].layers.push_back(std::move(layer));
        return true;
    }
    slots[i] = std::move(layer);
    return barrier.DepositAndWait(i, layerIndex);
};
```

**验收：** 验证台四组峰值应与层数【脱钩】（当前斜率 73.7 MB/层，
单实例 1.0/0.5/0.25mm 三档现为 1.59 / 2.62 / 4.76 GB）；
全量回归不新增失败；实例完成顺序不影响输出（不同调度多跑几次比对）。

**风险：** 并发接线 + 退回分流，改错的表现是【挂住作业】或【发半包】
而非报错，需独占一轮的验证预算（回归 + 斜率验证约 25 分钟）。

### 9.5.3 步骤 4b：写入器 session 化（形状与风险）

`WriteRgbwsvProductionPackage`（`RgbwsvPackageWriter.cpp:1024`，共 **330 行**）
内部本就是逐层写：

```cpp
for (const RgbwsvProductionLayer& layer : request.layers) {
    WriteRgbwsvProductionLayerTiff(stagingDir / relativePath, ...);
    // 累积 manifest 条目、逐层 report
}
```

**拆法：** 三段可单独调用，现有整包入口变成三段的薄封装（行为逐字不变）：

```text
BeginRgbwsvProductionPackage(request) -> Session   建 staging 目录与 identity
AppendRgbwsvProductionLayer(session, layer)        写一层 TIFF + 累积 manifest 条目
FinishRgbwsvProductionPackage(session) -> Result   写 manifest/report + 原子发布
```

场景路径的 `composeRequest.layersink` 直接调第二段，层写完即释放。

**风险（为何单独一轮）：** 这 330 行承载 **staging 与原子发布**语义 ——
取消与写失败时不得发布半包。把大量局部状态（stagingDir、profile、manifest
数组、identity）提进 session 结构，容易在错误路径上漏掉回滚。
**改错的后果是发出半包，而不是报错**，故必须配合取消/失败注入用例一起验，
不适合与其他改动混在一批里。

### 9.5.2 步骤 2/4 的已知形状（下次接手直接用）

```text
现状  MultiModelProductionService.cpp:817
        for (instance) { adapted = AdaptLegacySceneLayers(...); rasters.push_back(...); }
        composeRequest.instances = std::move(rasters);   // 全部切完才合成

目标  SceneLayerBarrier barrier(visibleCount);
        每实例一个线程跑 AdaptLegacySceneLayers，回调内写单层槽后 DepositAndWait
        消费者 for (L) { ready = barrier.AwaitLayer(L); 合成第 L 层; 交给 layersink;
                         barrier.ReleaseLayer(L); }
```

**关键前置已确认可行：** 合成器的实例校验本就是逐层的 ——
`SceneLayerComposer.cpp` 的 `ValidateInstance` 内部是
`for (layerIndex) { ValidateLayer(...); layerStatistics->push_back(...); }`。
故「先整实例校验、再整体合成」可折成「逐层校验 + 逐层合成」，
不需要新造校验逻辑，只需把 `ValidateLayer` 的调用点移进合成的层循环。

**已完成的两半（均行为中性、可独立合入）：**

```text
步骤 3   SceneLayerComposeRequest.layersink    合成侧逐层交出   c6e88aa
步骤 2a  LegacySceneLayerAdapterRequest.layersink  实例侧逐层交出  c5ebdcf
```

**剩下的是一处需要小心的手术（步骤 2b）：**

合成器的 `ValidateInstance`（`SceneLayerComposer.cpp:589`）现在先整实例
走一遍层：它同时做三件事 —— 校验层数齐备（`layers.size() == layercount`）、
逐层调 `ValidateLayer`、累积 `instanceStatistics`（min/max x/y/layer 与通道统计）。
流式化后 `instance.layers` 为空，这三件都要改：

```text
层数齐备   改为在层循环结束时断言「已校验层数 == layercount」
ValidateLayer   调用点移进合成的层循环（该函数本就是逐层的，不必新造）
instanceStatistics   min/max 与通道统计改为逐层累积，收尾部分移到层循环之后
```

另外合成主循环里的
`placement.instance->layers.at(localLayerIndex)`（`SceneLayerComposer.cpp:1128`）
要换成 provider 回调，返回空指针表示该实例本层无内容。

**为什么单独标出来：** 这处改错【不会崩】，而是悄悄写出错误的场景报告
（统计值偏差、闭合证据失真）。必须配合验证台与全量回归一起验，
不适合在长会话尾段赶工。

**仍需处理：** 实例的 `localgrid` 与 placement 要在第一层到达前就位
（gridcallback 先于 ownedlayercallback 触发，可在屏障之外先收齐），
以及 admission 判据（重叠/冲突/stale）保持在开跑前一次性完成、不因并发放宽。

**不宜先合入半截：** 只做步骤 2 后峰值仍有约 62.2 GiB（合成结果），
依旧超物理内存 31.6 GB，用户场景不会因此可用。
步骤 2/3/4 必须一起交付才产生实际收益。

验收   单实例场景输出与现状逐字节全等（先用单实例跑通屏障，N=1 退化为直通）
       双实例 0.2+0.3 @10um 不再内存不足，peakWorkingSet 百 MB 级
       实例完成顺序不影响输出 hash（用不同线程调度跑多次比对）
       既有 scene 相关回归不新增失败
```

### 9.6 用户当前可用的规避

单模型路径已可用：`0.2.obj` 与 `0.3.obj` **分两次作业**各自切片，
每次峰值 1.18 GiB、耗时 10.7 分钟，合计约 21 分钟即可拿到两份包。
这不是修复，只是在 MF-05 落地前的可行替代。

### 9.6 shape 档：三步前置已做完，准入放开延后（2026-09-06）

上一轮把 shape 档标为「需先定剪枝策略」。这一轮做完了精确调研，结论是
**先做三步前置、准入放开延后**，理由与证据如下。

#### 9.6.1 为什么延后

| 理由 | 依据 |
|---|---|
| **收益只剩一半** | 形状优化的四项加法运算会把 activeColumns 之外的列写成非空，而 `ResetBoundedSupportLayer` 只清表内列 —— 那些像素**没有任何一层会清回 0**，会被下一层当作 pre-shape support 读进去再膨胀一圈，单调累积。故形状必须全幅面跑，等于交还 97.47% 的列剪枝（时间收益），只保住内存收益 |
| **需求面窄** | `shape_enabled` 默认 `false`；全仓只有三个样例开它，其中只有 `three_mf_real_01_support_shape.json` 同时是 relief + 有界可准入。目前没有证据表明有真实用户在 10um 大幅面 relief 场景下开 shape |
| **现状是安全的** | 当前 `Reject("support_shape_enabled")` 让这类配置走 retained 全栈 —— 慢、吃内存，但**结果正确**。放开后一旦踩中上面那条缓冲残留，失败模式是「输出少一圈 + 统计逐层放大」的静默漂移，比「内存不够跑不动」难查得多 |

**放开的正确前提**：先有具体用户场景要求，且按既有规矩先出授权文档留痕。

#### 9.6.2 已做完的三步（都不改行为，各自有独立价值）

**一、堵死一个静默陷阱。** 有界路径下 `support_generation.support_masks` 整栈为空
（`generate_support_masks` 根本没被调用）。若只把那条 `Reject` 删掉，
`OptimizeSupportShape` 的层循环会跑 **0 次**，却仍报 `enabled = true`、
added/removed 全 0 —— **不崩、不报错，产出一份没做过形状优化的包**。
已在主循环加守卫：`shape_enabled && boundedReliefSupport.eligible` 直接抛。
准入当前仍拒绝该档，故它不可能触发；它存在是为了让**将来放开那道准入**时立刻失败。

**二、消除两份逐字副本。** 形状优化后的类型图同步逻辑此前有三份逐字副本：
主循环的整栈版、`BoundedSupportShapeScan` 的逐层版、以及测试里的参考实现。
前两份都是生产代码，各自漂移不会被任何断言发现。已提为共享定义
`SynchronizeSupportShapeTypesForLayer`（做法与 `set_support_pixel` 一致），
整栈版逐层调用它。**测试那一份有意保留** —— 对拍的 oracle 必须独立于被测实现，
复用同一份定义会让比对退化成自反。

**三、补对拍盲区 —— 这一步的发现比预期严重得多。**

#### 9.6.3 「已有 CI 级哨兵」这个判断是错的

调研时认为 `BoundedSupportShapeScanTests` 的 retained-oracle 对拍已经把两份实现
钉住了，是「本次放开最重要的既有资产」。**补齐比对后发现它几乎什么都没测。**

逐条查证的结果：

```text
夹具是 7x7、模型为 5x5 方框，BuildPlan 只在 0 号像素设需求
  -> pre-shape support 只有一个孤立像素
  -> 被 min_component_area_px = 2 剔除干净
  -> 膨胀无源；唯一另一个支撑分量是被模型【四面围住】的 3x3 空腔，
     其邻域全是模型，CanWriteSupportPixel 一律挡住
  => added 恒为 0：膨胀、闭运算、水平桥接、垂直桥接【四项加法全部空转】
  => max_added_support_ratio = 20.0 又使超比例回滚永不触发
  => 实际只验到了「最小面积剔除」一步
```

同时 `CompactReportMatches` 对 `post.components` / `filteredComponents` /
`bridgedGaps` **只比 size 不比内容** —— 两份实现只要条目数相同，内容与顺序
全错也照样通过。桥接记录尤其危险：它的顺序由「水平全扫完再走垂直」这条同序
约定决定，而那正是两份实现最容易分家的地方。

**已补齐：**

| 补的东西 | 效果 |
|---|---|
| `post.components` / `filteredComponents` / `bridgedGaps` 的内容与顺序比对 | 三处「只比 size」的盲区消除 |
| `BuildPlan` 支持指定需求像素 | 可以把支撑放在模型**外侧**，让膨胀有处可写 |
| 档二：需求像素在外侧 + ratio 20.0 | **四项加法真正生效**，且断言 `anyAdditions == true` 钉住 |
| 档三：同夹具 + ratio 0.0 | **回滚分支必然触发**，且断言 `anyRollback == true` 钉住 |
| 档一：保留原夹具 | 原有覆盖不丢，并显式断言它**不**触发加法 |

后两条断言是关键：少了它们，把 ratio 改小、把需求像素挪个位置都只是换数字 ——
被测路径依旧没被走到，而测试照样全绿。这正是原用例的失效方式。

**结果：三档全过。** 即两份实现在四项加法与回滚分支上确实逐字节一致 ——
调研的静态比对结论至此得到机器验证，而在此之前它只是静态结论。

#### 9.6.4 一处顺带钉住的 oracle 缺陷

补齐 `filteredComponents` 比对时先红了一次：scanner 报层号 1、oracle 报 0，
面积与 bbox 完全一致。**不是实现漂移**，是 oracle `OptimizeSupportShapeForLayer`
的已知缺陷 —— 它把单层包成一元 vector 再走整栈实现，故报告里的 `layer_index`
恒为 0。已改为分别断言两侧各自的正确值并注明缘由：一旦哪天 oracle 改成报真实
层号，这里会立刻红，提醒把断言改回直接相等。

---

## 10. MF-06 Sparse Tile/Span 候选

**目标：** 只处理 active rect/tile/span，降低约 3% 占用场景的空白扫描。

**验收：** `123.stl` 17 个连通分量、小组件、边界、支撑、光油 halo 不丢失；未通过前不允许自动路由。

## 11. MF-07 自适应生产路由

**目标：** 在作业开始前按显式内存预算和已实现能力选路；Host 只展示 Worker 权威 telemetry。

**原验收（2026-09-06 之前）：** 小作业 retained、大作业 bounded；无法满足预算时明确失败；
不运行中途回退；Profile hash 和输出协议不变。

**修订后验收（2026-09-06，用户已授权按建议改写；理由见 11.0，逐条对应）：**

| # | 修订后 | 相对原文改了什么 |
|---|---|---|
| A | **能力准入**：配置能走 bounded 就走 bounded，不能则 fail-safe 退回 retained，`reason` 记明是哪一项 | 替换「小作业 retained、大作业 bounded」。该准则与作业大小无关 —— 两条路径输出逐字等价而 bounded 内存严格更低，故**预算永远选不出 retained** |
| B | **预算准入**：给定预算时，按【已建模的分配项】估算峰值；估算不满足即在作业开始前带命名错误码失败 | 原文「无法满足预算时明确失败」未限定依据。`slicer_core` 全程不观测自身内存，只能按解析模型估 —— 写明这一点，才不是一个兑现不了的承诺 |
| C | **预估器单向正确**：允许高估、**禁止低估** | 新增。低估的表现形式恰是「承诺了预算然后 OOM」，比不做路由更糟，故验收不能写成「误差在 X% 以内」 |
| D | **不运行中途回退** | 保留原文。**已实质满足，见 11.4** |
| E | Profile hash 与输出协议不变 | 保留原文。硬约束见 11.1 下方 |
| F | Host 只展示 Worker 权威 telemetry | 保留原文。MF-07a 已做 Worker 侧、MF-07b 前半已做 Host 侧 |

**被删除的一条，及删除理由：** 「小作业 retained」在有实测证明「小作业走 retained
更快」之前**不实现**。按大小切换路径需要一个门限，而目前没有任何数据支持该门限的
存在与取值 —— 凭空造一个出来，只会让小作业无谓地多吃内存。
若后续实测发现逐层重建对小作业有显著耗时代价，再按【耗时】立独立准则，
而不是塞回这条以内存为名的验收里。

### 11.0 对原验收的三条质疑（2026-09-06 调研后提出）

动工前做了一次只读现状调研，结论是**原验收有两条需要改写、一条需要点名**。
先写在这里，因为按字面实现会做出错的东西。

**质疑一：「小作业 retained、大作业 bounded」这条方向可疑。**
bounded 与 retained 在输出上逐字等价，而 bounded 内存严格更低。那么
「小作业为什么要故意用更多内存」？按内存预算路由，答案永远是「能 bounded
就 bounded」—— **预算根本选不出 retained**。能不能 bounded 是由
`EvaluateBoundedReliefSupportPath` 的能力准入决定的，与预算无关。

若保留 retained 的真实理由是**速度**（小作业省掉逐层重建的开销），
那这条的准则是耗时、不是内存，与同卡的「无法满足预算时失败」不是一个维度。
**处置：拆成两条独立准则，能力准入与预算准入各自表述。**
在有实测证明「小作业 retained 更快」之前，不实现按大小切换 —— 那是一个
没有依据的门限。

**质疑二：「无法满足预算时明确失败」目前只能降格。**
`slicer_core` 全程不观测自身内存（`ProcessMemoryStats.h` 在 `slicer.cpp`、
`pipeline/`、`support/` 里零引用），预算只能对着**分配清单的解析模型**判，
而分配器开销、TIFF 写出缓冲、`texture_runtime`、`materialVolumePlan`、
OpenVDB 后端都不在那份清单里。
**处置：改写为「按已建模的分配项估算，估算不满足即失败」**，
否则是一个兑现不了的承诺。且预估器的验收必须是**单向的（允许高估、禁止低估）**
—— 低估的表现形式恰恰是「承诺了预算然后 OOM」，比不做路由更糟。

**质疑三：「不运行中途回退」这条只关于场景路径，卡面没点名。**
单模型路径已经合规：准入在采样之前判定，层循环里只读同一个已定结论，
没有任何改路点。真正的活全在 `MultiModelProductionService`：生产者线程
**先启动**，主线程再按各实例 `originzmm` 是否相等判 Streaming / Rejected，
而每个生产者的 layersink 在每一层都等这个决定再分流。
走进 `Rejected` 的作业会静默退回累积整栈（实测斜率 189 MB/层），
**任何预算承诺当场失效且用户看不到提示**。这一条才是该验收的确切目标。

### 11.1 子卡拆分

| 卡 | 范围 | 验收 | 风险 | 状态 |
|---|---|---|---|---|
| MF-07a | Worker 权威内存 telemetry | Worker 与 CLI 的 `peakWorkingSetBytes` 同口径；`SLICE_TIMING` 仍过协议解析；包字节与 profileHash 不变 | 低 | **COMPLETE** |
| MF-07b | Host 停止伪造 telemetry | Worker 未声明 available 时不再强行置真 | 中 | **COMPLETE（前半）** |
| MF-07c | 峰值预估器（纯函数、不接生产） | 四个实测点全部高估覆盖；大作业裕度 1.26~1.38x | 高 | **COMPLETE** |
| MF-07d | 预算字段与开始前路由 | 预算字段放 **profile 之外**；profileHash 与包字节逐字节不变；超预算带命名错误码失败 | 中 | TODO |
| MF-07e | 消除场景路径中途回退 | —— 见 11.4，该验收**已实质满足** | ~~最高~~ 已重估 | **CLOSED（无需改行为）** |

**MF-07d 的一条硬约束（已查证）：** profileHash 是对**整份 profile JSON 文档**
（剔除自声明的 `profileHash` 键）做 sha256，不是白名单字段。故预算字段
**绝不能加进 profile**，否则 hash 必变、六处强制点会 fail-closed。
放在 Worker 请求顶层或 CLI 参数则完全不受影响
（`file_contract_v1.request.schema.json` 顶层是 `additionalProperties: true`）。
`SLICE_TIMING` 同理：解析器是 required-keys + 首字段必须 `engine=`，
对额外键宽容，故增补字段是向后兼容的。

~~**MF-07e 为何风险最高：**~~ **该评估已被证伪，见 11.4。**
原文（保留作对照）：它要把 localgrid 的 originz 语义复制到 `run_slicer`
之外（第二份实现）；删掉 fallback 等于把今天能跑（只是吃内存）的场景变成
硬失败。**两条都建立在「不对齐的场景真实存在」这个前提上，而该前提不成立。**

### 11.2 MF-07a 实施记录（2026-09-06 COMPLETE）

**修的是一处真缺陷，不是新功能。** Worker 的 `SLICE_TIMING` 行把内存
硬编码成 `workingSetBytes=0 peakWorkingSetBytes=0`，且 result JSON 的
`timing` 对象里连内存字段都没有。于是「Host 只展示 Worker 权威 telemetry」
从源头就无从谈起 —— 宿主只能拿自己的轮询观测值补齐，再把结果标成
`available: true`（`HostSliceJobController`），那正是 MF-07b 要拆掉的。

改动：Worker 在收尾处采一次 `CaptureProcessMemoryStats()`，
**SLICE_TIMING 行与 result JSON 共用同一组数**（分两次采会让 JSON 侧的
peak 恒 >= 行侧，因为峰值单调不减，对拍时那点差额会被当成两条通道不一致），
并补上此前缺失的 `memoryAvailable=` 字段，口径与 `slicer_cli` 完全一致。

**一并补了断言。** 原测试只查 `SLICE_TIMING` 这个字符串是否存在、不查值
—— 所以它被写死成 0 多久都不会有人发现。现按「可得则必须非零」钉住：
平台不支持时 `available` 为假、允许为 0，一旦声明 available 就不能再报 0。
result JSON 侧同样钉住三个字段的存在性与 peak 非零。

### 11.3 MF-07b 实施记录（2026-09-06 前半 COMPLETE）

`HostSliceJobController` 原先在 Worker 未声明 `available` 时**强行置真**，
于是宿主自己的轮询估算会被当作 Worker 权威 telemetry 展示 —— 那正是本条
验收要消除的。已改为只反映 Worker 的真实声明：无权威数据时 `available`
保持假，面板据此不展示细分耗时；补齐值仍留在 `timing` 里并标 `approximate`，
供诊断查看，但不再冒充权威。

**正常路径行为不变，有据可查：** `slicer.cpp:4200` 无条件设
`profile.available = true`，且七个耗时字段 Worker 全都提供，
故补齐与置位在 Worker 正常返回时**本就不触发** —— 这段一直是只在 Worker
沉默时才生效的兜底，而它兜的方式是撒谎。验证：`hostflow_hb06_slice_job`
（断言「成功作业必须返回 Worker 核心细分耗时」，即 `available` 为真）
通过；hostflow / 14e 全组 38 项中 4 项失败，**全部在既有失败基线内**。

**后半已完成（2026-09-06，用户授权后实施）。** 见 11.3.1。

#### 11.3.1 后半：两类数据分区，而不是一概不显示

**先说一个前半引入的退步。** 前半只是不再伪造 `available`，补齐值仍混在
`timing` 里；而面板的判定是 `if (timing.available)`，于是这些值一概不显示
—— **作业失败时用户什么诊断信息都看不到，而那恰是最需要信息的时候**。
把「不诚实」换成「什么都不说」并不是正确的终点。

后半的做法是让两类数据**分开存放、分区展示**：

| | 原始 | 前半之后 | 现在 |
|---|---|---|---|
| `timing.available` | **`true`（伪造）** | `false` | `false` |
| 宿主观测值位置 | 混在 `timing` 内 | 仍混在 `timing` 内 | **独立字段 `observedtiming`** |
| 面板细分耗时 | 显示，**当作权威** | **完全不显示** | 显示，**引擎栏标注非权威** |
| 引擎栏文案 | `失败前阶段进度估算` | 同左 | `宿主估算·非 Worker 权威（失败前阶段进度）` |

`timing` 从此只装 Worker 自己报的东西；`observedtiming` 装宿主按
`pollResolutionMs` 轮询估出来的，并原样带上 `approximate` / `source` /
`activePhase` / `hostElapsedMs`，让面板能说明这些数字是怎么来的。

#### 11.3.2 顺带发现第二处冒充

改到一半发现 `FinishTransportFailure`（通信失败路径）里有一句
`m_completion.timing = FinalizeObservedTiming(...)` —— **把宿主观测值
直接赋给 `timing`**。那是与补齐处同一类的冒充，只是走的是另一条分支，
前半没有覆盖到。已一并改为赋给 `observedtiming`，`timing` 保持空。

这一处也说明「Host 只展示 Worker 权威 telemetry」这条验收，
光看一处赋值是判断不了的 —— 得把所有给 `timing` 赋值的路径都过一遍。

#### 11.3.3 改动面与验证

调用链需要贯通：`SigCompleted` 信号 -> `HostMainWindow::OnSliceJobCompleted`
-> `HostSliceJobPanel::ShowCompletion` -> `ApplyTiming`，四处签名各加一个
`observedTiming` 参数；`ApplyTiming` 内部据 `authoritative / estimated`
选择取值源。

验证：`slicer_host_sim` 编译通过；`hostflow_hb06_slice_job`
（断言「成功作业必须返回 Worker 核心细分耗时」，即 `available` 为真）通过
—— 即正常路径行为未变，这与前半的判断一致：
`slicer.cpp` 无条件设 `profile.available = true`，
故补齐与分区在 Worker 正常返回时本就不触发。

---

### 11.4 MF-07e 重估：那条验收已实质满足（2026-09-06）

**动工前先查证了一个前提，结果前提不成立，于是这张卡不需要改任何行为。**

#### 11.4.1 事实

生产路径上 `SceneRaster.localgrid.originzmm` **恒为 0**。

```text
slicer.cpp  构造 SliceRunRasterGrid 时最后一个字段写的是字面量 0.0
   -> LegacySceneLayerAdapter 的 gridcallback 原样拷进 localgrid.originzmm
   -> MultiModelProductionService 的对齐判定读它
```

全仓 grep `originzmm =` / `originZmm =`，生产代码里再无第二处赋值
（`MultiModelSliceOrchestrator` 那处写的也是 `0.0`）；只有四个单测会设非零，
用于直接构造 raster 夹具、不经过切片路径。

#### 11.4.2 推论

对齐判定是：

```cpp
bool aligned = producerCount > 0U && !barrier.Failed();
for (...) if (!slot.gridreceived || slot.grid.originzmm != slots[0]->grid.originzmm)
              aligned = false;
```

既然 `originzmm` 恒相等，那个不等式**恒不成立**。`Rejected` 只可能来自其余三项：

| 触发条件 | 含义 |
|---|---|
| `producerCount == 0` | 没有可见实例 |
| `barrier.Failed()` | 屏障已失效，作业正在中止 |
| 某 slot `!gridreceived` | 该实例在报出栅格前就结束（失败或取消） |

**三者都意味着作业已经或即将失败。** 即：**正常路径上不存在「运行中途改路」。**

#### 11.4.3 对验收的结论

MF-07 的「不运行中途回退」这条**已实质满足**，无需改动行为。
单模型路径本就合规（准入在采样前判定、层循环内只读同一结论）；
场景路径的运行时分叉在正常路径下恒走同一支。

#### 11.4.4 更正上一轮的表述

上一轮（11.0 质疑三）写的是：

> 走进 `Rejected` 的作业会静默退回累积整栈（实测斜率 189 MB/层），
> **任何预算承诺当场失效且用户看不到提示**。

**这句技术上正确，但漏掉了触发条件——那三种情况全是错误路径。**
按它读出来的印象是「正常作业可能悄悄退回并吃掉 270 GB」，而实际不会。
189 MB/层那个数字是 MF-05 之前**所有**场景作业的斜率，不是 Rejected 分支特有的。
两者被并列在一起，读起来像是后者仍在发生。**保留原文于此，不删。**

#### 11.4.5 为何仍不把 Rejected 改成报错

走到那里时，真正的失败原因在生产者侧、由 `collectProducerResults` 带出。
在 layersink 里另造一个错误只会**遮蔽**它 —— 用户看到的会是「流式对齐失败」
而不是「模型 B 加载失败」。此处累积的整栈随后连同错误一起丢弃，
代价是一次性的，换来的是错误归因不被污染。

#### 11.4.6 将来什么时候要重开这张卡

一旦引入**实例 Z 偏移**（各实例 `originzmm` 不再恒等），该判据就成为真正的
运行时分叉，届时必须重新评估：那时「不对齐」是合法的业务场景而非错误，
退回整栈才会成为真实的内存风险。代码注释已就地标注这一条。

---

### 11.5 MF-07c 实施记录（2026-09-06 COMPLETE）

`src/slicer_core/system/SlicePeakMemoryEstimate.{h,cpp}`，纯函数、**不接生产**。

#### 11.5.1 契约是单向的

**允许高估，禁止低估。** 这不是「误差在 X% 以内」的精度要求 ——
低估的表现形式是「承诺了预算然后 OOM」，比不做路由更糟；
高估只会让一个本可跑通的作业被拒，用户立刻发现并调高预算。
故每一处取舍都选偏大的一侧：活动列表按列数上界计（不猜 2.53% 的稀疏度）、
条件分配拿不准时按分配计。

#### 11.5.2 不复制任何常量

所有逐列字节数由 `sizeof` 就地取得，条件判据直接调用生产函数本身
（例如表面光油走 `SurfaceVarnishMasksRequired`，而不是照抄它的 `enabled` 判断）。

理由不是洁癖：**抄一份就等于埋一处必然漂移的常量，而漂移方向不可控 ——
可能正好导致低估**，那正是本卡唯一禁止的失败方向。

为此把 `ReliefColumnInfo` 从 `slicer.cpp` 下沉到
`geometry/ReliefColumnInfo.h`（它此前只在 `slicer.cpp` 内使用，18 处引用不变）。
该结构每列一份，10um 下 736 万列即数百 MB，是模型里必须计入的大项。
顺带使 `slicer.cpp` 再减 11 行。

#### 11.5.3 建模了什么、没建模什么

逐列项（O(列数)，两条路径共有）：`relief_columns`、两份 `column_ranges`、
`support_source_layers`、compose 输出缓冲、表面光油两个单层。
有界路径另加：`spans`、活动列表、三个单层缓冲。

整栈项（O(列数 x 层数)，**仅 retained**）：`model_masks`、`support_masks`、
`support_type_maps` 三份恒有（后两者的 resize 在 `support.enabled` 检查**之前**）；
外光油开启时再加 `outer_varnish_masks` 与 `upper_support_boundary_masks`；
shape 开启时再加一份 `originalSupportMasks` 整栈深拷贝。

**未建模**（清单写在头文件里，必须随代码更新）：进程与运行时固定开销、
`texture_runtime` 的纹理像素、`materialVolumePlan` 与 OpenVDB 体数据、
模型网格本身、以及**分配器未归还给 OS 的部分** —— 注意实测 peak working set
含最后这一块，故实测值天然高于「分配量之和」，这也是必须留余量的原因之一。

余量取 **256 MiB**，是由实测点标定的**经验上界**，不是推导值。

#### 11.5.4 实测校验结果

四个点全部被高估覆盖：

| 场景 | 估算 | 实测 | 裕度 |
|---|---|---|---|
| r01 @0.05mm（bounded） | 275 MiB | 85 MiB | 3.21x |
| gubao04 @0.05mm（bounded） | 279 MiB | 126 MiB | 2.21x |
| a-2 @10um（bounded） | 1,134 MiB | 902 MiB | **1.26x** |
| a-2 @10um（retained，专项介入前） | 31,129 MiB | 22,528 MiB | **1.38x** |

小作业裕度 2~3 倍是固定余量占主导所致，无害 —— 小作业本来就不触及预算。
**大作业裕度收敛到 1.26~1.38 倍**，那正是预算判定真正起作用的区间。

一处交叉验证：retained 估算 30.4 GB，与报告 §3.1 里**独立**算出的
31.11 GB 几乎重合 —— 两次计算路径不同（一次逐项手算、一次由代码按
`sizeof` 求和），结果吻合是对建模清单的一次旁证。

#### 11.5.5 测试怎么钉住它

`stage16c06_slice_peak_memory_estimate_unit_tests`：

- **禁止低估**：四个实测点逐一断言 `predicted >= measured`，并把裕度打印出来
  —— 只知道「没低估」不够，高估十倍的模型同样没用，而模型一旦漂移，
  最先变的就是这个比值；
- **有界估算必须与层数无关**：15 层与 1429 层结果逐字节相同，且有界路径
  不得含任何 `scalesWithLayers` 的项。这是 MF-03X2a/MF-05 的核心成果，
  也是判断模型有没有抄错路径的判据。对照断言 retained 必须随层数增长；
- **准入未过时带出原因**：报 reason 而不是给一个永远达不到的数字；
- **退化输入**：列数为 0 时只剩余量（不能报 0 —— 那会让预算判定误以为
  「不占内存」）；层数非法时逐列项照旧、整栈项必须消失。

#### 11.5.6 它还不能做什么

**没有接进生产路径**，因为路由需要预算字段，而那是 MF-07d 的范围。
本卡只交付一个可独立验证的纯函数。

「禁止低估」目前只对上表四个点被机器证实。它们覆盖了两个数量级与两条路径，
但**不构成普遍证明** —— 尤其是未建模项里的纹理与体数据，在贴图极大或
OpenVDB 开启的配置下可能超过 256 MiB 的余量。接进生产前应当先补这两类场景的
实测点；若余量不够，正确的处置是**提高余量**而不是让它低估。

---

## 12. MF-08 收口

**矩阵：** `123.stl`、Reality 5/5、标准甲片、Stage 15 fixture，S0/S3/S4，support/material/varnish，
1/11/12/22，cold/warm，Retained/Bounded/Sparse candidate。

**出口：** 逐层 hash、RIP strict、取消恢复、wall/CPU/Peak Working Set 和 build identity。正式设备
SLA/内存上限缺失时，只完成工程 Gate，不宣称 production SLA PASS。

## 14. MF-09/10/11 耗时优化（2026-09-07 立卡）

用户 2026-09-06 提出：**不改硬件的前提下，大画幅切片耗时还有多少空间**。
本节是核查结论与据此拆出的三张卡。

### 14.0 先更正我自己的一个判断

初次答复时我看到「18 核机器、层循环完全串行、`layerComputeMs` 占 91.2%」，
就给出了「并行化可拿 3~5 倍」的预期。**只读核查之后这个判断要收回**，
理由见 14.3 —— 产品路径上那个循环**本来就被屏障锁步**，并行它拿不到吞吐。

留下这段而不是抹掉，是因为那个错误有代表性：**看到「串行 + 占比高 + 多核空闲」
就断定可以并行，漏掉了「它为什么串行」**。这里的答案是「因为下游要求按层序消费」，
而那不是循环自己的性质。

### 14.1 MF-09：层循环剩余整幅面 pass 剪枝（先做这张）

MEMFLOW 的 2.53% 活动列剪枝**只覆盖了四处**：`compose_layer` 主循环、
`MaterializeReliefModelLayer`、`AddInternalVoidSupportForLayer`、
`AnalyzeRetainedMaterialLayerChannels`。层循环里**至少还有六处整幅面 pass 漏网**：

| 位置 | 说明 |
|---|---|
| `SupportConnectivityAnalysis.cpp:19,27` | 每层新建并清零 7.37 MB 的 `visited`，再做整幅面扫描；**且 `slicer.cpp` 的调用点在 `config.support.enabled` 守卫【之外】**，支撑关闭时它照跑不误、全命中 continue |
| `MaterialVolumePlan.cpp:451` | `MaterializeMaterialOwnershipLayer` 整幅面 |
| `slicer.cpp` MATVOL 填补循环 | 整幅面 |
| `slicer.cpp` compose 的 `whiteScratch` / `effectiveRgb` | 两次整幅面读 + 一次整幅面写，**外加每层两次新分配** |
| `slicer.cpp` opacity varnish 循环 | 整幅面 |
| `build_texture_preview_mask` | 双重整幅面循环（仅 preview 层） |

**为什么先做这张：** 等价性论证与 MF-03X2b 已做过的是同一套
（「分支链没有末尾 else，故表外列不写任何字节」），
风险已知、验证路径已趟熟；且它**不增内存、不碰屏障、不碰进度协议、
不引入任何数据竞争** —— 与并行化正相反。

**第一步必须是量，不是改。** 在层循环内加细粒度计时，把 `layerComputeMs`
这 516 秒拆到上述各段。剪枝能吃掉多少，先有数再动手 ——
本专项已经吃过一次「凭直觉优化」的亏（§5.7 记录：只做剪枝而不复用缓冲，
内部空腔耗时几乎不降，因为按幅面计的固定开销与按占用计的工作量是两笔账）。

### 14.1.1 测量结果：瓶颈不是我们以为的那六处（2026-09-07）

按「先量后改」插桩实测（a-2/0.2.obj，143 层快速档，`layerCompute` 约 45 秒）：

| 段 | 耗时 | 占比 |
|---|---|---|
| **材料闭合语义分析** | **38,723 ms** | **约 86%** |
| 支撑连通性统计 | 2,336 ms | 5% |
| 物化三件套（X2a 已剪枝） | 948 ms | 2% |
| 通道统计（X5 已剪枝） | 216 ms | 0.5% |
| compose（X3 已剪枝） | 182 ms | 0.4% |

**已经剪过的三处合计只占 3%。** 若按 14.1 原计划去剪那六处整幅面 pass，
几乎白干 —— 这正是「先量后改」要防的事，本专项 §5.7 已经吃过一次同类的亏。

**根因是一个没人预料会激活的默认值：**

```text
config.h 里 MaterialClosureConfig::enabled 默认 = true   <- 配置文件里根本没写这一项
  -> collectMaterialClosureExact = enabled && write_reports = true
  -> MaterialClosureSemanticLayerInput 声明在层循环【内】
  -> 11 个 mask 各 assign(pixelCount, 0)
  -> 736 万列 x 11 B = 81 MB/层，1429 层合计约 116 GB
```

即**任何不显式关闭它的作业都在付这笔钱**。

生产路径（DLL -> Worker -> 场景路径）还要更贵：那里有 `ownedlayercallback`，
`collectMaterialClosureSemantic` **恒为真**，比 CLI 多跑一段
`PopulateRetainedMaterialClosureEmptyMask`。

### 14.1.2 一次失败的尝试，及它证伪了什么

**做法**：把 `MaterialClosureSemanticLayerInput` 提到层循环外跨层复用
（新增就地填充入口 `Reset...InPlace`，因为 `input = Initialize(...)` 是移动赋值、
会丢弃已有 buffer）。思路与 MF-03X4 对 compose 输出缓冲的处理同源。

**结果：没有收益。** 公平 A/B（同一快速档，各跑三次取最小）：

```text
改前  75,338 / 62,389 / 61,579  ->  min = 61,579 ms
改后  73,274 / 68,940 / 78,808  ->  min = 68,940 ms
```

两组波动范围重叠（22% 与 14%），故不能断言它更慢；但**可以断言没有任何证据
支持它更快**。据此回退，不留无收益的复杂度。

**它证伪的假设**：我以为那 86% 的开销在 `malloc/free`。**不是。**
开销在 `assign(pixelCount, 0)` 的**写入本身** —— 那 81 MB/层无论对象复不复用
都要写；复用只省掉分配器的簿记，相比之下微不足道。

**因此正确方向只剩一条：不写那些字节**，即给闭合语义输入加活动列剪枝
（实测该场景只有 2.53% 的列有模型）。前置是等价性验证：
表外的列在闭合语义上是否恒为空、下游分析是否不依赖它们 ——
判据与 X3「compose 的分支链没有末尾 else」同源，但**必须独立验证**，
不能沿用。

**还有一处未拆分**：那 38.7 秒里，`assign` 的写入与
`InitializeSemantic` 末尾那个逐像素循环（算 `expectedOccupiedDomainMask`，
7.37M 次/层）各占多少，尚未分开测。剪枝方案要同时覆盖两者才有意义，
故下一步应先把这两段拆开量。

### 14.1.3 再拆一层：初始化也不是大头，真正的开销在分析函数

上一节把矛头指向「每层新建 11 个 mask」。**继续拆分之后，那个判断也要修正。**

在 `InitializeSemantic` 内部分别计时（同一快速档）：

```text
closureAssignMs   6,202 ms    11 个 assign 合计，占 closure 段的 16%
closureLoopMs     1,935 ms    末尾逐像素循环，占 5%
合计              8,137 ms
closure 段总计   38,723 ms    -> 还差 30,586 ms
```

**即：整个初始化只占 21%。** 就算把 assign 全部省掉，上限也只有 16% ——
这从另一个角度印证了 14.1.2 那次回退是对的（复用对象连这 16% 都拿不到，
因为字节照样要写）。

**那 30 秒在 `AnalyzeMaterialClosureSemanticLayer`**，它有三笔整幅面开销：

| 开销 | 规模（10um，736 万列） |
|---|---|
| 四次 `std::fill` 重置 workspace 的四个 gap mask | 29.5 MB/层 |
| 从边界洪泛算 `externalBackgroundMask` | 整幅面 |
| `for (y) for (x)` 主判定循环 | 736 万次/层 |

### 14.1.4 等价性论证（剪枝的前置，已完成）

主循环的判据是：

```cpp
candidateGap = layerEmptyMask[i] != 0
            && expectedOccupiedDomainMask[i] != 0
            && workspace.externalBackgroundMask[i] == 0
```

活动列表外的列，按 `BuildBoundedActiveColumns` 的定义是
**「所有层都无模型，且能经其他无模型列连到幅面边界」**。于是：

- `expectedOccupiedDomainMask` = `modelEnvelope || supportRequired || outerVarnishShell`
  三者对表外列**全为 0** → **第二个条件不成立**；
- 且这类列按定义就是外部空白，`externalBackgroundMask` 必为 1
  → **第三个条件也不成立**。

**故表外列恒不是候选间隙，剪枝是精确等价的**，且两条判据互为旁证。
这一条与 X3 的判据同源，但**是独立论证的**，不是沿用。

### 14.1.5 下一步的方案（未实施）

按收益排序：

1. **主判定循环改为只扫活动列** —— 直接省掉 97.47% 的迭代，等价性已由 14.1.4 论证；
2. **四次 `std::fill` 改为只重置活动列** —— 前提是 workspace 跨层复用
   （表外列从不被写，故恒为 0）。注意 14.1.2 的教训：**这一条单独做收益有限**
   （fill 只是那 30 秒里的一部分），要与第 1 条一起做；
3. **洪泛不能简单剪枝** —— 它恰恰要遍历外部空白来确定边界连通性。
   但表外列的结果是**解析可知的**（恒为外部背景），故可跳过实际洪泛、直接置位。
   这一条最需要小心，建议最后做并单独验证。

**实施前必须先加 `activeColumns` 参数贯通到
`MaterialClosureSemanticDetector`**，那是本方案的主要改动面。
验收沿用本专项硬要求：零漂移四判据逐字节全等 + 快速档 min-of-3 的 A/B。

### 14.1.6 MF-09 第一档落地：改走 View 版本 + workspace 跨层复用（COMPLETE）

§14.1.5 把「主判定循环剪枝」排在收益第一位。**但在动剪枝之前，先发现了一笔
更简单、且无需任何等价性假设的开销**：主循环调的是 owning 便利版本

```cpp
AnalyzeMaterialClosureSemanticLayer(input, connectivity, maxGapPx)   // owning
```

它在库内部做了两件按幅面计价的事，**与算法本身无关**：

| 开销 | 规模（10um，736 万列） |
|---|---|
| 每层新建 workspace，`Prepare` 里 7 次 `resize` + `traversalQueue.reserve` | 约 110 MB/层 |
| 返回前 7 次 `assign(view.begin(), view.end())` 把 mask 拷进 owning 结构 | 约 51.6 MB/层 |

而 **View 版本两者都没有**，且与 owning 版本是同一实现 —— owning 版本本身就是
「Prepare 一个临时 workspace、调 View 版本、把结果拷出来」。所以改走 View 版本
**不需要任何等价性论证**，只需保证 workspace 跨层复用不串味（见下）。

**为什么这次复用有收益，而 §14.1.2 那次没有：**

```text
14.1.2 复用的是输入缓冲，其填充是 assign(n, 0)  -> 复用后 n 字节照样要写
14.1.6 复用的是 workspace，其准备是 resize(n)   -> 对已有容量是 no-op（不写）
```

同一个「跨层复用」的做法，收益取决于被复用者是用 `resize` 还是 `assign` 准备的。
这一条值得记住。

**串味风险已排除（这是本卡唯一的新风险）：** View 版本借出的 7 个 mask 在入口
处全部被重置 —— 分析侧 6 次 `std::fill` 加 `BuildExternalBackgroundMask` 自身的
`fill`；修复侧 2 次 `std::copy` 加 7 次 `std::fill`。逐一核对过，没有遗漏项。
并补了一条单测直接钉住它（见下）。

**同步下沉（G2）：** 整段 exact 闭合逐层处理（分析 + 可选修复 + 复检 + 计数回填）
从 `slicer.cpp` 下沉为 `material/MaterialClosureExactLayerPass.{h,cpp}` 的
`RunMaterialClosureExactLayerPass`，**`slicer.cpp` 净减 25 行**（+22/−47），
本专项继续不依赖 MATOPQ 那条豁免。副产物：`repairValues` 原先每层重算一次
（`ResolveMaterialClosureRepairValues(config)` 在层循环内），现在随
`MaterialClosureExactLayerRequest` 在循环外解析一次。

#### 实测（交错 A/B，同一时间窗，a-2/0.2.obj 143 层快速档）

```text
                layerComputeMs
base   44,318 / 47,637 / 42,014   -> min 42,014
mf09   39,154 / 37,132 / 37,286   -> min 37,132   -11.6%
两组区间不重叠（mf09 最差 39,154 < base 最好 42,014）
peakWorkingSetBytes 两侧同为 905 MB 量级，无内存回退
```

**方法上的一条教训（比数字更重要）：** 第一次测量拿改后结果去比
§14.1.2 留下的旧基线（min 61,579 ms），算出 **−45%**。那是错的 ——
当天 02:49 的机器负载远高于此刻，同一份 base 二进制现在只跑 42,014 ms。
**跨时间窗比较在本机没有意义**（§短基准波动达 47%）。故重新编出 base 二进制
与 mf09 交错跑三对，取上表。`−45%` 不成立，真实收益是 `−11.6%`。

#### 零漂移（用户指定资产，逐字节）

同一配置分别用 base / mf09 二进制跑，比对全部层 TIFF 的拼接 sha256 与
`material_closure_report.json`：

| 用例 | 层数 | layers sha256 | closure 报告 |
|---|---|---|---|
| `finger_suoguo/a-3/0.2.obj` @0.1mm | 143 | `647ec538…` 一致 | `2d0a8740…` 一致 |
| `suoguo-baseline/qiegejiapian-zxl.stl` | 45 | `43ae7994…` 一致 | `51d26e51…` 一致 |
| `suoguo-baseline/suoguo-hcc.stl` | 48 | `6eb44fea…` 一致 | `7467180c…` 一致 |

**三个用例的 `gapPixels` 全为 0**，即真实资产跑不到修复分支。故修复路径**不能**
由这三条对拍覆盖 —— 它由单测覆盖，且断言里先钉住「参照实现确实修了一个像素」
再比对，避免整条空转（见 `oracle-tests-can-be-entirely-vacuous` 那类坑）。

新增两条单测（`material_closure_semantic_detector_unit_tests`，已确认实际执行）：

```text
exact_layer_pass_matches_owning_repair_composition
    修复路径下与旧 owning 组合逐字节比对：层通道 + 语义 mask + 全部计数
exact_layer_pass_is_stable_across_workspace_reuse
    先用有间隙的层把 workspace 写脏，再用同一 workspace 跑无间隙的层，
    与全新 workspace 的结果全等 —— 直接钉住「跨层复用不串味」
```

#### 全量回归与门禁

```text
构建     BUILD_EXIT=0（含新增 material/MaterialClosureExactLayerPass.cpp）
回归     10 失败 / 230，落在本工作树既有的 10~11 失败区间内（MAX_PATH 抖动）
门禁     ValidateSourceSizeGuard.py PASS；slicer.cpp 净减 25 行
```

失败项里唯一需要单独定责的是 `scene_layer_adapters_unit_tests`
（`legacy_adapter_applies_admitted_instance_transform` 的
「translation preserves local layer bytes and dimensions」）。**已实测定责为既有失败**：
把本卡改动 stash 掉、重编该 target 再跑，**同一用例、同一条断言照样红**。
另有 `production_mode_catalog_unit_tests` 为 Not Run（找不到可执行文件），与本卡无关。

判据上它本来也不可能由本卡引起：该用例比较的是**同一份代码**的 baseline 与
translated 两次输出，任何一致的行为改变都会在两侧同时出现而相互抵消。

#### 结论与下一步

第一档收益 **−11.6%**，比 §14.1.5 预期的量级小，因为它只拿掉了「用便利接口的
代价」（约 160 MB/层的分配、归零与拷贝），**没有触及那 736 万次/层的主判定循环**。
§14.1.5 的三步方案仍然成立且未实施，其中第 1 条（主循环只扫活动列）依然是剩余
收益里最大的一块，等价性论证已在 §14.1.4 完成。

### 14.1.7 第三次「先量后改」，第三次推翻自己的排序：70% 在洪泛

§14.1.5 把三步方案按预估收益排成「主判定循环 > 六次 fill > 洪泛（最需小心、
放最后）」。**把 `AnalyzeMaterialClosureSemanticLayer` 内部再拆三段实测，
这个排序完全反了**（同一快速档，`layerComputeMs` 38,237）：

| 段 | 耗时 | 占 layerCompute |
|---|---|---|
| **`BuildExternalBackgroundMask` 边界洪泛** | **26,987 ms** | **70.6%** |
| `for(y)for(x)` 主判定循环 | 1,075 ms | 2.8% |
| 六次整幅面 `std::fill` | 517 ms | 1.4% |

**即 §14.1.5 排第一的那条只值 2.8%，排最后的那条占 70%。**
本卡至此已经三次靠实测推翻自己的排序（§14.1.1 推翻六处 pass、§14.1.3 推翻
「开销在初始化」、本节推翻三步方案的次序）。**这条规律要当成纪律用，不是巧合。**

洪泛为什么贵：它要访问全部「空白且与边界连通」的像素 —— 该场景 97.47% 的列
在任何层都空，即每层要 BFS 约 718 万个像素，每个像素做 2 次整数除法定位、
8 个邻居各一次 `IsInside` + `PixelIndex` 乘法 + `layerEmptyMask[]` 读
+ `external.at()`（**带边界检查**）；队列还是 `std::vector<std::size_t>`，
最坏要写读 718 万 × 8 B ≈ 57 MB。

### 14.1.8 这项诊断的总代价：占 layerCompute 的 92%、总墙钟的 83%

直接把它关掉对照（`"materialClosure": {"enabled": false}`，其余配置逐字相同，
同一二进制、同一时间窗、on/off/on/off 交错各两次取最小）：

```text
                 layerComputeMs      totalMs
enabled = true       37,405           42,133
enabled = false       2,953            7,072      -92.1% / -83.2%
层 TIFF 143 层拼接 sha256 两侧同为 647ec538… —— 【逐字节一致】
```

**层输出完全不受影响，关掉它只少一份 `material_closure_report.json`。**
且本卡实测的三个真实资产 `gapPixels` 全为 0 —— 这份报告在真实资产上从未发现
任何东西。根因仍是 §14.1.1 那条：`MaterialClosureConfig::enabled` 默认 `true`，
而配置文件从不写它，**任何不显式关闭它的 CLI 报告作业都在付这笔钱**。

⚠ **这不构成「建议默认关掉」的结论。** 它是诊断能力，关不关是产品决策，
需要用户裁定；本卡只负责把代价与影响面量出来。

### 14.1.9 【重要修正】生产路径根本不跑这段，MF-09 对它收益为零

上面所有数字都出自 **CLI 且开报告** 的路径。查证生产路径（用户实际用的
`PrintApp -> slicer_module.dll -> slicer_worker.exe`）后发现：

```cpp
// pipeline/LegacySceneLayerAdapter.cpp:159   场景/生产适配器
options.write_reports = false;
```

而闭合精确分析的开关是

```cpp
collectMaterialClosureExact = material_closure.enabled
    && (options.write_reports || repairMaterialClosure);
```

`MultiModelProductionService` 通过 `AdaptLegacySceneLayers`
（`MultiModelProductionService.cpp:1060`）走这个适配器，故
**诊断模式下生产路径的 `collectMaterialClosureExact` 恒为 false** ——
整段闭合精确分析（含那 70% 的洪泛）在生产路径上从不执行。

**由此得出三条必须写明的结论：**

1. **MF-09 第一档的 −11.6% 只作用于「CLI 且开报告」，对生产路径收益为零。**
   与 MF-03X1 那次同类（那次是「用户配置没开表面光油，故收益为零」）。
   §14.1.5 剩下的三步剪枝同理，**做完对生产路径也仍是零**。
2. 生产路径仍会付 `collectMaterialClosureSemantic` 那部分（它由
   `ownedlayercallback` 恒为真），即每层 11 个整幅面 mask 的 `assign`
   —— §14.1.3 实测约占 closure 段的 16%，但那是在开报告的口径下测的，
   在生产口径下的绝对占比**尚未测量**。
3. **本专项此前所有耗时数字（含 a-2@10um 的 18.9 -> 9.44 分钟）都是 CLI 口径。**
   引用给用户时必须标注口径；生产路径的实际耗时构成**从未测过**。

**故下一步不是继续剪 CLI 的洪泛，而是先量生产路径。** 见 MF-12。

### 14.4 MF-12：生产路径耗时构成测量（NEW，先量后改）

**为什么立这张卡：** §14.1.9 查明 CLI 报告路径与生产路径**跑的不是同一段代码** ——
生产路径把 `write_reports` 写死为 false，闭合精确分析（CLI 口径下占 layerCompute
的 92%）在那边根本不执行。本专项迄今所有耗时数字都是 CLI 口径，
**生产路径的耗时构成从未测量**。在没量之前，任何针对生产耗时的优化都是猜。

**这正是 §5.7 与 §14.1.1/14.1.3/14.1.7 那条规律的第四次应用：**
前三次是「量错了段」，这次是**量错了路径** —— 后者更隐蔽，因为两条路径共用
`run_slicer`，看起来像同一件事。

**要量什么：**

```text
1. 生产路径（DLL -> Worker -> MultiModelProductionService）一次真实作业的
   分段耗时：模型载入 / 网格 / 采样 / 支撑 / 层计算 / 合成 / 写盘 / 屏障等待
2. 层计算内部：collectMaterialClosureSemantic 那 11 个 assign 的绝对占比
   （§14.1.3 的 16% 是开报告口径下的相对值，不能搬过来）
3. SceneLayerBarrier 的 DepositAndWait 实际阻塞多久 —— 它同时是 MF-11
   并行化被判 DEFERRED 的原因，若阻塞占比高则该结论需要复核
```

**怎么量（待定，两条候选）：**

- `apps/slicer_ui_host_sim` 已有完整宿主链路与 `HostSliceJobController`，
  且 MF-07b 已在其中接了 timing 面板 —— 优先考虑从这里驱动，代价最小；
- 或在 `MultiModelProductionService` 内加与 `SLICE_TIMING` 同构的分段计时
  （生产代码加计时需按本仓 profileLevel 约定走，不能裸插桩）。

**验收：** 得到一张生产口径的分段表，并据此重排 MF-09/10/11 的优先级。
**在这张表出来之前，不再对 CLI 专有路径做任何优化**（洪泛剪枝就此挂起）。

**注意口径标注：** 该表出来后，§0 摘要与探查报告里所有耗时数字都要补口径标注
（CLI+报告 / CLI 无报告 / 生产），否则会被误引用。

### 14.4.1 MF-12 实测结果：生产口径与 CLI 口径几乎没有共同点（COMPLETE）

**怎么量的：** `stage16c06_scene_memory_bench` 走的就是
`RunMultiModelProductionService` —— 与 DLL/Worker 生产路径同一条。
它返回的 `result.profile` 一直带着分段耗时，**只是从未打印**。本卡把
`BENCH_PROFILE` 与 `BENCH_INSTANCE` 两行加上，无需任何生产代码插桩。

**资产：** `a-2/0.2.obj`，0.1mm，143 层，1418×5197，单实例。

```text
totalMs                90,090
├─ modelLoadMs            229    0.3%
├─ gridSetupMs            229    0.3%
├─ sliceProcessingMs   46,397   51.5%   = 场景 compose 窗口
│     BENCH_INSTANCE：coreSliceMs 13,065 ／ composeMs 45,507
│     两者【并发】：生产者写完第 L 层即被 SceneLayerBarrier 阻塞等消费，
│     故不可相加；真正的逐层计算是那 13,065 ms
└─ outputWriteMs       47,242   52.4%
      ├─ tiffWriteMs    3,781    4.2%
      └─ packagePublishMs 43,449 48.2%   ← 单项最大
```

**与 CLI 口径对照（同一资产、同一层厚）：**

| | CLI+报告 | CLI 无报告 | 生产 |
|---|---|---|---|
| totalMs | 42,133 | 7,072 | 90,090 |
| 层计算 | 37,405（92%，其中洪泛 70%） | 2,953 | 13,065（14.5%） |
| 主要开销 | 闭合诊断 | — | 包发布读回校验 + compose |

**三条口径的瓶颈互不相同。** 生产路径比 CLI+报告还慢一倍，但慢的地方完全不同 ——
CLI 慢在闭合诊断（生产路径不跑），生产慢在包发布与 compose（CLI 便宜得多）。

**对既有任务卡的影响（必须重排）：**

| 卡 | 原判断 | MF-12 后的判断 |
|---|---|---|
| MF-09 | 层循环耗时优化，第一档 −11.6% | **对生产路径收益为零**（§14.1.9） |
| §14.1.5 三步剪枝 | 剩余最大一块 | 只作用于 CLI+报告，**挂起** |
| MF-10 | MATVOL `gridSetup` 占 92% | 生产口径 `gridSetupMs` 只有 229 ms（0.3%），**须用生产口径重估** |
| MF-11 | 层循环并行化，DEFERRED（屏障锁步） | **结论加强**：可并行的 coreSlice 只占 14.5%，即便完美并行，上限也只有约 14% |

### 14.5 MF-13：包发布的读回全量校验（NEW，单项最大）

把 publish 阶段拆成六段实测（同一作业）：

```text
[PUB] ext1Ms=4.6  strictValidateMs=43,402.8  renameMs=1.8
      ext2Ms=9.9  identityMs=11.5  cleanupMs=1.3   totalMs=43,440.9
```

**`internal::ValidateSlicePackageArtifact(stagingDir)` 一项占了 publish 的 99.9%**
（`RgbwsvPackageWriter.cpp:1468`）—— 即**把刚写完的整包重新读回来做严格校验**。
发布本身（同父目录 rename）只有 1.8 ms。

**规模外推：** 该作业包约 5.9 GB / 143 层，读回校验 43.4 s。同资产 10um 是
1429 层、约 59 GB，**读回校验单项就约 7 分钟**。

**它验了什么（`rip_reader.cpp:685` 的 `ValidateSlicePackageImpl`，已查清）：**

```text
包级   manifest schema / grid / channelOrder / channelCount / bitDepth
       / sampleFormat / planarConfig / storageMode / compression / polarity
       / printValue / emptyValue / layers 数组长度
逐层   文件存在 -> read_rgbwsv_tiff【完整解码六通道整幅面】
       -> 比对 storageMode、compression、宽高
       -> ValidateLayerStatistics：把解码出的 channel_stats 与 manifest 里
          该层记录的统计逐项比对
       -> merge_channel_stats + 记下 channel_checksums
```

**它不是可以随手删的东西：** 这是 fail-closed 的正确性闸门，且
`PublishStagedPackage` 的「同父 rename 保住已严格校验的字节」这一保证正建立在它
之上（源码注释明写）。

⚠ **一条已被否掉的方向（记下来防止再想到）：** 「写盘时单趟校验，用写入器手上
那份内存字节边写边验」看着等价性最强，**其实完全不等价** —— 读回校验的目的正是
**验证落盘的字节**，用内存字节算会让写入器 bug、截断、磁盘坏块全都无从发现。
这条判据一旦被绕过，那句「同父 rename 保住已严格校验的字节」就不再成立。

**故候选方向只剩三条：**

```text
A  并行化逐层校验：143 层各自独立，只有 merge_channel_stats 与
   layer_checksums 需要同步。语义【完全不变】—— 同样读盘、同样解码、同样比对。
B  与写盘重叠：层是逐个写完的，可以边写边校验【已落盘】的层。语义不变，
   只压墙钟不省 CPU。注意 compose 已被屏障锁步，重叠空间要先测。
C  只验结构与标识、不重新解码像素。代价最低但【减弱了判据】，属产品决策，
   须用户裁定，本专项不自行决定。
```

**A 的收益取决于这 43.4 s 是 I/O 界限还是 CPU 界限，尚未测量。**
现有旁证倾向 CPU：同一份数据 `tiffWriteMs` 只用 3,781 ms 写完
（约 1,560 MB/s，走系统写缓存），读回却是 43.4 s（约 137 MB/s，慢 11 倍）；
而每层要解码 1418×5197×6 ≈ 44 MB 并逐字节累计统计与校验和，
143 层合计约 6.3 GB 的字节级运算，`rowsPerStrip=64` 又意味着每层约 82 个小条带。
**但这仍是推断，不是实测。** 动手前必须先量出 A 的上限：

```text
前置测量  在 read_rgbwsv_tiff 内部把「解码」与「统计/校验和」分开计时，
          再看多线程校验的实际吞吐曲线（本机 18 线程）
```

**未测之前不动手** —— 本专项已四次因为「先量后改」推翻自己的排序，
而本节的 A 方向本身就是在查清校验内容后**推翻了自己十分钟前写下的 A**。

**验收口径：** 生产口径（`stage16c06_scene_memory_bench`）的 `packagePublishMs`
与 `totalMs`，min-of-3 交错 A/B；零漂移仍按四判据 + 用户指定资产逐字节。

#### 14.5.1 MF-13a：去掉每层被读两次（已改，收益低于噪声）

查 `read_rgbwsv_tiff`（`tiff_io.cpp:443`）时发现一处纯浪费：

```cpp
read_rgbwsv_tiff(path)
    ReadFile(path);              // 整个文件（本场景每层约 44 MB）
    ParseIfdEntries(data, path);  // 只为判断条带还是瓦片
    -> read_rgbwsv_stripped_tiff(path)
           ReadFile(path);            // 【又读一遍同一个文件】
           ParseIfdEntries(data, ...); // 【又解析一遍同一个 IFD】
```

即每个被校验的层都被读两次、IFD 解析两次。已改为把已读缓冲与已解析 IFD 传下去
（新增 `ReadTiledFromBuffer` / `ReadStrippedFromBuffer`，公开的单参版本降为薄封装
以免影响其他调用方）。**语义完全不变：同一批字节、同一套判据。**

**实测：收益低于噪声。** 为排除整作业口径的写盘/删盘干扰，改用
`rip_reader_test --package` 对**同一个已写好的包**（5.9 GB / 143 层）反复跑，
只跑校验本身、不写不删，四对交错：

```text
base   40,035 / 88,439 / 227,072 / 75,322  -> min 40,035 ms
mf13a  39,542 / 84,148 /  52,048 / 45,419  -> min 39,542 ms   -1.2%
```

**−1.2% 落在噪声里，不能算收益。** 量级上本来也只该有 3% 左右 ——
第二次 `ReadFile` 由系统文件缓存供给，代价是 44 MB 的分配与拷贝
（143 层合计约 6.3 GB），不是真正的磁盘 I/O。

**按本专项既定做法处理：效果低于噪声时，按算法依据取舍。** 它是**严格更少的
工作量**（少一次整文件读、少一次 IFD 解析），且没有引入新的复杂度或新判据，
故**保留**，但**不得对外宣称收益数字**。

⚠ **同时要记下这批测量的污染源：** 跑上表时机器上有 **18 个 MSBuild 进程**
（另一个会话在同一工作树里构建）。这解释了同一份二进制、同一份输入为何在
40,035 ~ 227,072 ms 之间跳（5.7 倍）。**整作业口径那一轮先测出的
「publish −42%」就是这个污染的产物，不成立** —— 该轮 base 的 publish 在
54,090 ~ 117,497 ms 间跳，而隔离测量证明真实差异只有 1.2%。

**故 §14.5 的前置问题（那 43.4 s 是 I/O 界限还是 CPU 界限）此刻测不了。**
现有数据只能说：单线程校验吞吐约 148 MB/s，且**看不到缓存预热效应**
（base#1 40,035 之后紧接的 mf13a#1 是 39,542，几乎相同），
倾向 CPU 界限但**不足以下结论**。**待机器空闲后重测，再决定 A（并行化）
是否值得做。**

**功能证据（非性能）：** 上表 8 次校验（其中 4 次走新代码）全部 EXIT=0，
在真实 143 层包上通过；`ctest -R "tiff|rip"` 16/16 通过。

#### 14.5.2 【更正 14.5.1】MF-13a 的「收益低于噪声」是错的：实为 −34.5%

§14.5.1 判定 MF-13a「−1.2%，低于噪声」，并据此写下「按算法依据保留、不宣称收益」。
**那个结论是污染的产物，现予更正。**

当时机器上有 18 个 MSBuild（另一会话在同一工作树构建）。等它跑完后，用同一套
隔离口径（`rip_reader_test --package` 对同一个已写好的包只跑校验、不写不删）
重测，并**同时记录 CPU 时间**（CPU 时间基本免疫争用）：

```text
                        wall(min of 3)     cpu(min of 3)
MF-13a 之前（704c161）      11,713 ms         11,562 ms
MF-13a 之后（0c270e9）       7,675 ms          7,609 ms     -34.5% / -34.2%
两组区间完全不重叠，且各自波动 < 1%（11,713~11,774 / 7,675~8,480）
```

**即那次「每层被读两次」的修复真实收益是 −34.5%，不是 −1.2%。**
量级上我原先估「约 3%」也错了 —— 第二次 `ReadFile` 虽由文件缓存供给，但它要
**新分配 44 MB 并拷贝**（新页还要被内核清零），再加一次完整 IFD 解析；
这笔内存流量与统计主循环是同一量级。

**提交 `0c270e9` 的说明里写的「收益低于噪声」应以本节为准。**
教训已并入记忆：并行会话的构建会把同一份二进制、同一份输入的耗时拉出 5.7 倍差，
交错 A/B 抵消不了这种量级的争用；**基准前后都要查 `tasklist`，并优先看 CPU 时间**。

#### 14.5.3 MF-13b：逐字节七件事改为直方图（COMPLETE，−31.7%）

§14.5 的前置问题「43.4 s 是 I/O 界限还是 CPU 界限」**已由代码判定并被实测确认**：

```cpp
// tiff_io.cpp  AccumulateContiguousChannelStats（改前）
for (每个像素) for (6 个通道) {
    result.channel_checksums[channel] += value;        // 1
    TiffChannelStats& stats = result.channel_stats[channel];
    stats.min_value = std::min(...);                   // 2
    stats.max_value = std::max(...);                   // 3
    stats.empty_pixels        += value == 255U;        // 4
    stats.print_pixels        += value != 255U;        // 5
    stats.full_print_pixels   += value == 0U;          // 6
    stats.partial_print_pixels += value != 0U && value != 255U;  // 7
}
```

**每个字节约七件事**，且内层按 `channel` 变址、无法向量化。整包每个字节都要走
一遍：5.9 GB 的包合计约 44 GB 标量运算。**实测 CPU 时间约等于墙钟
（5,141 / 5,272 ms），确认是 CPU 界限，单线程。**

**改法：逐通道 256 桶直方图。** 每字节只做一次 `++histogram[channel][value]`，
收尾时从 6×256 个计数导出全部统计。判据逐项等价：

```text
校验和    原为逐次 += value，等于 sum(value * count)
min/max   按值升序遍历非空桶取 min/max，与逐字节取 min/max 同值
四个计数  分别是 count(255)、count(!=255)、count(0)、count(0<v<255)
跨条带    直方图是本次调用的局部量，导出时仍按原样合并进 result
```

**实测（隔离口径，安静机器，交错三对）：**

```text
                    wall(min)    cpu(min)
MF-13a（改前）       7,744 ms     7,594 ms
MF-13b（改后）       5,290 ms     5,188 ms     -31.7% / -31.7%
区间完全不重叠，各自波动 < 4%
```

**a + b 合计：`11,482 -> 5,272 ms`（−54.1%），CPU `11,234 -> 5,141`（−54.2%）。**
两条独立测量相乘（0.655 × 0.683 = 0.447）与合并实测（0.459）互相印证。

**等价性网（这条是本卡的关键，因为既有对拍验不出统计的错）：**

⚠ `tiff_writer_equivalence_unit_tests` 里原有的 `ResultsAreEquivalent`
**验不出统计算错** —— 它两侧都调 `read_rgbwsv_tiff`，共用同一份统计实现，
统计一旦错会在两侧同时错、比较照样相等；而且整条用例在 LibTIFF 不可用时被跳过。
这正是 `oracle-tests-can-be-entirely-vacuous` 那类坑。

故新增一条**独立参照**的对拍 `channel_stats_match_independent_reference`：
就地用改前的逐字节写法算出参照值，与读取器输出**逐字段**比对
（六个统计字段 + 六个通道校验和 × 六种像素模式），且**放在 LibTIFF 早退之前**
以保证永不被跳过。

**并已验证它非空转：** 把直方图导出里的 `value * count` 故意改回 `value`，
测试当场红（`FAIL stats oracle: checksum mismatch case 0`），改回后复绿。

**其他证据：** `rip_reader_test` 在真实 48 层包上 PASS，`--summary` 输出与改前
逐字节相同；逐层校验本身会比对 `printPixels`/`emptyPixels`（48 层 × 6 通道）。

**回归：** 11 失败 / 230，落在本工作树既有的 10~11 区间内。比上一轮多的那一条
`slicer_stage14e04b_capability_coverage_test` 已读输出定责为 **MAX_PATH 抖动**：
`failed to write report: ...package.staging.job-slice-<pid>-<ts>.attempt-<32hex>/reports/...`
约 290 字符，而 job/attempt id 每次都变，故路径长度在阈值上下浮动
（`hostflow_ha03_qt_end_to_end` 同因）。与本卡无关。TIFF/统计相关的六条
（`tiff_writer_equivalence` / `tiff_writer_contract` / `rip_reader_resolution` /
`multi_model_package_writer` / `rgbwsv_production_package_writer` /
`material_closure_semantic_detector`）全部通过。

#### 14.5.4 端到端验收（生产口径，MF-13a + MF-13b 合计）

按 §14.5 定的验收口径跑（`stage16c06_scene_memory_bench`，
a-2/0.2.obj @0.1mm、143 层、单实例，old/new 交错三对）：

```text
                packagePublishMs                    totalMs
old   62,735 / 54,921 / 46,806 -> min 46,806   124,910 / 108,642 / 96,754 -> min 96,754
new   23,328 / 22,042 / 24,404 -> min 22,042    77,127 /  67,896 / 76,425 -> min 67,896
                        -52.9%                             -29.8%
两项的区间都完全不重叠（new 最差 24,404 远低于 old 最好 46,806）
```

**publish 的 −52.9% 与隔离口径的 −54.1% 相符**，互为印证。
`old` 一侧波动较大（46.8~62.7 s）—— 这一轮末尾机器上还有 1 个 MSBuild，
且 bench 每次要写再删 5.9 GB；`new` 一侧则很稳（22.0~24.4 s）。

**改写后的生产口径耗时账（a-2/0.2.obj @0.1mm）：**

| 段 | 改前 | 改后 |
|---|---|---|
| 包发布（读回全量校验） | 46,806 ms（48%） | **22,042 ms** |
| compose 窗口 | 约 46,400 ms（其中生产者实算仅约 13,100） | 未动 |
| TIFF 写盘 | 约 3,800 ms | 未动 |
| **总计** | **96,754 ms** | **67,896 ms** |

**下一个大头已经清楚：compose 窗口那约 46 s 里，生产者真正的逐层计算只有约
13 s，其余约 33 s 是屏障等待与消费侧 compose。** 它现在是最大的单项
（约占改后总时长的一半），且与 MF-11（层循环并行化）判为 DEFERRED 的依据同源
（`SceneLayerBarrier` 锁步），**故下一步是先量清那 33 s 的构成**，
而不是直接去做 §14.5 的候选 A（并行化逐层校验）。

候选 A 仍然可做、依据也更强了（已确认 CPU 界限、单线程），但它现在只针对
剩下的 22 s，收益上限低于那 33 s。**排序按实测，不按先想到哪个。**

### 14.2 MF-10：MATVOL 逐列求交

多材质大栅格实测 `gridSetupMs` 占 **92%**（gubao04 XY 放大 4 倍，639 秒 / 691 秒），
而层计算只占 5%。**这条路径的瓶颈根本不在层循环**，MF-09/MF-11 对它都无效。
`MaterializeMaterialOwnershipLayer` 是 O(列数 × 三角数)。需要独立方案，待估。

### 14.3 MF-11：层循环并行化 —— DEFERRED，四条理由

**一（决定性）：产品路径上该循环已被屏障锁步。**

```text
slicer.cpp 的 ownedlayercallback
  -> LegacySceneLayerAdapter 的 layersink
  -> MultiModelProductionService 的 barrier.DepositAndWait(slotIndex, layerIndex)
```

`SceneLayerBarrier` 的语义是：生产者写完第 L 层后**阻塞**，直到消费者
（合成 + 写包）消费掉第 L 层才放行；类注释明写「每实例一个生产者线程、
一个消费者线程」，`AwaitLayer` 还特意注明「按实例序升序 —— 否则输出 hash 会抖」。

**故并行 18 路的结果是 18 个线程全堵在 `DepositAndWait` 上：吞吐不变、内存 ×18。**
要真拿到加速，得同时改造屏障（允许乱序 deposit）+ `SceneLayerComposer`
+ `RgbwsvProductionPackageSession`（其 State 里 `layers`/`writtenLayerCount`
全是顺序流式的）—— 那是三个模块的重构，不是「给循环加个并行」。

那个 91.2% 的实测来自 **`slicer_cli` 单模型直写路径**（无 ownedlayercallback、
TIFF 直接写盘）。那条路径确实能并行，但**它不是产品路径** ——
立卡前必须先确认用户的实际生产路径走哪条。

**二：与本专项目标直接冲突。** 专项的全部价值是把峰值压到百 MB 级；
按跨层复用缓冲清单估算，18 路并行会推回 **1.6~4.6 GB**
（最小配置约 12 B/列 x 736 万列 x 18；开 MATVOL + closure 则约 35 B/列）。
在同一个专项分支上做，等于自己拆自己的验收判据。

**三：内存带宽大概率先饱和。** 该循环算术强度极低（全是 uint8 掩码的逐列读写、
`fill_n`、洪泛），每层要流过 44 MB 的 compose 输出加若干整幅面 pass。
桌面双通道内存下，3~6 线程可能就打满。**这条无法靠只读核查证实 ——
必须先用 2/4/8/12/18 五个点实测扩展曲线**（且按既有规矩，短基准要多次取最小值）。

**四：Amdahl 上限没有 18×。** 串行余量 8.8%（1 - 516,877/566,649），
18 线程理论加速约 7.2×，无限线程也只有 11.4×。

**若将来重启，核查已列出必须处理的清单**（择要）：
`lastOwnedMaterial` 是唯一的真·串行前缀依赖（MATVOL 路径，直接影响落盘 RGB）；
八个顺序敏感的 `push_back` 容器需改按下标预分配；`texture_runtime.report` 与
`material_policy_report` 的裸 `++` 是最容易被漏掉的竞争点（顺序无关 ≠ 无竞争）；
进度上报有**两条**协议规则（percent 与 current 各一条，且 `current` 未被钳位）；
`profile.*_ms` 是逐层墙钟求和，并行后会变成 N 倍虚数，让人误判「并行没效果」。

---

## 13. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-07 | v4.13 | 新增 §14.5.4 端到端验收：生产口径 old/new 交错三对，`packagePublishMs 46,806 -> 22,042`（**−52.9%**）、`totalMs 96,754 -> 67,896`（**−29.8%**），两项区间都完全不重叠；publish 的 −52.9% 与隔离口径的 −54.1% 相符，互为印证。并据此重排下一步：**compose 窗口那约 46 s 现在是最大单项**（其中生产者实算仅约 13 s，其余约 33 s 是屏障等待与消费侧 compose，约占改后总时长一半），故先去量清那 33 s，而不是接着做 §14.5 的候选 A（并行化逐层校验）—— 后者只针对剩下的 22 s。 |
| 2026-09-07 | v4.12 | 新增 §14.5.2（**更正**）与 §14.5.3（MF-13b COMPLETE）。§14.5.2：MF-13a 的「−1.2%、低于噪声」是 18 个 MSBuild 争用下的假结论，等机器空闲后用同一隔离口径并记录 CPU 时间重测，实为 `11,713 -> 7,675 ms`（**−34.5%**，区间不重叠、各自波动 <1%）；提交 `0c270e9` 说明里的「收益低于噪声」应以本节为准。§14.5.3：把 `AccumulateContiguousChannelStats` 的每字节约七件事（校验和 + min + max + 四个计数器，且按通道变址无法向量化）改为逐通道 256 桶直方图，每字节只做一次自增、收尾导出，判据逐项等价；实测 `7,744 -> 5,290 ms`（**−31.7%**），CPU 时间同幅下降，**确认校验是 CPU 界限、单线程**（CPU≈墙钟），§14.5 的前置问题就此解答。a+b 合计 `11,482 -> 5,272 ms`（**−54.1%**），两条独立测量相乘与合并实测互证。⚠ 关键：既有 `ResultsAreEquivalent` **验不出统计算错**（两侧共用同一实现，且 LibTIFF 缺失时整条跳过），故新增独立参照对拍 `channel_stats_match_independent_reference`（六字段 × 六通道 × 六种像素模式，置于 LibTIFF 早退之前），并**已用故意打断验证它非空转**。 |
| 2026-09-07 | v4.11 | 新增 §14.5.1（MF-13a）。`read_rgbwsv_tiff` 原先读完整文件、解析 IFD 只为判断条带/瓦片，随后调用的单参版本【又把文件读一遍、IFD 再解析一遍】—— 每个被校验的层都被读两次。已改为传递已读缓冲与已解析 IFD（语义完全不变）。**但实测收益低于噪声**：用 `rip_reader_test` 对同一个已写好的包只跑校验、不写不删、四对交错，min 40,035 -> 39,542 ms（−1.2%）；量级上本来也只该有 3%，因为第二次读由文件缓存供给。按本专项既定做法（效果低于噪声则按算法依据取舍）**保留**该改动 —— 它是严格更少的工作量且未引入新复杂度，但**不宣称收益**。⚠ 并记下污染源：测量时机器上有 **18 个 MSBuild**（另一会话在同一工作树构建），同一二进制同一输入在 40,035~227,072 ms 间跳；**先前整作业口径测出的「publish −42%」即此污染的产物，不成立**。§14.5 的前置问题（I/O 界限还是 CPU 界限）此刻测不了，待机器空闲后重测。 |
| 2026-09-07 | v4.10 | §14.5 修正：查清 `ValidateSlicePackageImpl`（`rip_reader.cpp:685`）的实际判据后，**否掉了自己上一版写下的候选 A**（「写盘时用内存字节单趟校验」）—— 读回校验的目的正是验证**落盘**字节，用内存字节算会让写入器 bug、截断、坏块无从发现，那句「同父 rename 保住已严格校验的字节」也就不再成立。候选改为：A 并行化逐层校验（层间独立，语义完全不变）、B 与写盘重叠、C 只验结构不解码像素（减弱判据，属产品决策）。并补上校验内容清单与「43.4 s 是 I/O 界限还是 CPU 界限尚未测量」的前置要求（旁证倾向 CPU：写 1,560 MB/s vs 读回 137 MB/s，且每层要解码 44 MB并逐字节累计统计与校验和）。 |
| 2026-09-07 | v4.9 | 新增 §14.4.1（MF-12 COMPLETE）与 §14.5（立 MF-13）。**生产口径首次量出来**（a-2/0.2.obj @0.1mm，143 层，单实例，总 90,090 ms）：`packagePublishMs 43,449`（48.2%）、compose 窗口 `46,397`（51.5%，其中生产者真正的 `coreSliceMs` 只有 13,065 = 14.5%，其余是屏障等待与消费侧compose）、`tiffWriteMs 3,781`、`gridSetupMs` 仅 229。**与 CLI 口径几乎没有共同点**：CLI+报告 42,133 ms 慢在闭合诊断（生产不跑）、CLI 无报告 7,072 ms、生产 90,090 ms 慢在包发布与 compose。据此重排：MF-09 与三步剪枝对生产收益为零已确认；**MF-10 的「gridSetup 占 92%」须用生产口径重估**（生产口径只有 0.3%）；MF-11 的 DEFERRED 结论**加强** —— 可并行的 coreSlice 只占 14.5%，完美并行上限也仅约 14%。并把 publish 拆六段实测：`strictValidateMs 43,402.8` 占 publish 的 **99.9%**，即把刚写完的整包重新读回严格校验（rename 只有 1.8 ms）；10um 外推该项单独约 7 分钟。立 MF-13，方向是「换成不需重新读盘的等价校验」而非关掉校验，并要求先查清校验内容再动手。bench 侧新增 BENCH_PROFILE / BENCH_INSTANCE 两行输出（生产代码零插桩，探针已回退）。 |
| 2026-09-07 | v4.8 | 新增 §14.1.7~14.1.9。§14.1.7：把分析函数再拆三段实测，**第三次推翻自己的排序** —— §14.1.5 排第一的主判定循环只占 2.8%、六次 fill 占 1.4%，而排最后的边界洪泛占 **70.6%**（26,987 / 38,237 ms）。§14.1.8：把整项诊断关掉对照，`layerComputeMs 37,405 -> 2,953`（−92.1%）、`totalMs 42,133 -> 7,072`（−83.2%），且 143 层 TIFF 逐字节一致 —— 关掉只少一份闭合报告；但关不关是产品决策，本卡只量代价不下结论。§14.1.9 **重要修正**：`LegacySceneLayerAdapter` 把 `write_reports` 写死为 false，而精确分析由 `write_reports || repair` 门控，故 **诊断模式下生产路径（DLL -> Worker -> MultiModelProductionService）从不执行这段** —— MF-09 第一档的 −11.6% 与 §14.1.5 三步剪枝对生产路径收益均为零（同 MF-03X1 那类），且本专项所有耗时数字都是 CLI 口径，生产路径的耗时构成从未测量。据此立 MF-12 先量生产路径，**不再继续剪 CLI 的洪泛**。 |
| 2026-09-07 | v4.7 | 新增 §14.1.6：MF-09 **第一档落地并验收**。在动剪枝之前先拿掉「用 owning 便利接口的代价」——每层新建 workspace（约 110 MB/层）+ 返回前 7 次 mask 拷贝（约 51.6 MB/层），改走同一实现的 View 版本并把 workspace 跨层复用，**无需任何等价性假设**。交错 A/B（同一时间窗、各三次取最小）`42,014 -> 37,132 ms`，**−11.6%**，两组区间不重叠；峰值内存无回退。**并修正一次自己的测量错误**：首次拿改后结果去比 §14.1.2 的旧基线得出 −45%，实为跨时间窗比较 —— 同一份 base 二进制此刻只跑 42,014 ms，故 −45% 不成立。零漂移在用户指定资产（a-3/0.2.obj、qiegejiapian-zxl、suoguo-hcc）上层 TIFF 与闭合报告逐字节一致；三者 `gapPixels` 全为 0，修复路径改由两条新单测覆盖（含「参照实现确实修了一个像素」的非空转断言，与「跨层复用不串味」的对照）。同步下沉 `MaterialClosureExactLayerPass`，`slicer.cpp` 净减 25 行，不依赖 MATOPQ 豁免。§14.1.5 三步方案仍未实施。 |
| 2026-09-07 | v4.6 | 新增 §14.1.3~14.1.5：继续拆分后**修正了上一节的判断** —— 初始化（11 个 assign + 逐像素循环）合计只占 closure 段的 21%，真正的 30 秒在 `AnalyzeMaterialClosureSemanticLayer`（四次整幅面 fill 共 29.5 MB/层、边界洪泛、736 万次/层的主判定循环）。这从另一角度印证 14.1.2 的回退是对的：复用对象连那 16% 都拿不到。并**完成剪枝的等价性论证**：表外列的 `expectedOccupiedDomainMask` 三个分量全为 0，且必为外部背景，故 `candidateGap` 恒 false，两条判据互为旁证 —— 该论证独立完成，未沿用 X3。给出按收益排序的三步方案（主循环剪枝 > fill 剪枝 > 洪泛解析化），并标注洪泛那条最需小心、应最后做。本次仍无代码改动。 |
| 2026-09-07 | v4.5 | 新增 §14.1.1/14.1.2：MF-09 按「先量后改」插桩实测，**结果推翻原计划** —— 瓶颈不是那六处整幅面 pass（已剪过的三处合计仅占 3%），而是**材料闭合语义分析占 86%**；根因是 `MaterialClosureConfig::enabled` 默认为 true 而配置文件从不写它，导致每层新建 11 个整幅面 mask（81 MB/层、1429 层约 116 GB）。并**如实记录一次失败的尝试**：把该对象提到循环外跨层复用，公平 A/B（各三次取最小）显示 61,579 -> 68,940 ms，无任何收益证据，已回退。它证伪的假设是「开销在 malloc/free」—— 实际在 `assign` 的写入本身，那些字节无论复不复用都要写。故正确方向只剩活动列剪枝，且需独立做等价性验证；另标注 38.7 秒里 `assign` 写入与逐像素循环尚未分开量。 |
| 2026-09-07 | v4.4 | 新增 §14：应用户「不改硬件、大画幅耗时还有多少空间」的提问立三张卡。**并更正我自己的初次判断** —— 当时看到「18 核、层循环串行、layerCompute 占 91.2%」就断言并行可拿 3~5 倍，只读核查后收回：产品路径上该循环被 `SceneLayerBarrier` 锁步（生产者写完一层即阻塞等消费），并行 18 路只会全堵在 `DepositAndWait`，吞吐不变而内存 ×18；那个 91.2% 来自 `slicer_cli` 直写路径，不是产品路径。据此 MF-11（并行化）DEFERRED 并记下重启时必须处理的清单。改为先做 MF-09：层循环里还有至少六处整幅面 pass 是 MEMFLOW 剪枝的漏网之鱼（其中 `analyze_support_connectivity` 每层新建清零 7.37 MB 且调用点在 support.enabled 守卫之外），等价性论证与 X2b 同一套、不增内存不碰屏障。MF-09 第一步是**加细粒度计时先把 516 秒拆开**，不是直接改。另立 MF-10：多材质大栅格的瓶颈在 MATVOL 逐列求交（gridSetup 占 92%），与层循环无关。 |
| 2026-08-21 | v2.2 | MF-03B4B 专项准备补齐：冻结 public DTO、facts identity、retained 精确顺序、Stage 15 eligible branch、caller output/sink 强异常边界、closure 固定 workspace、独立 oracle 与实施拆分；结论 PREPARED / IMPLEMENTATION GO，生产仍未接线。 |
| 2026-08-21 | v2.1 | MF-03B4A COMPLETE：实现 plan-bound verified replay、Base/outer-varnish 最终化、逐层 compact connectivity sink 与 fail-closed 生命周期；Release 组合 Gate 通过且生产零接线。MF-03B4B 解除依赖等待但未开工。 |
| 2026-08-21 | v2.0 | 完成 MF-03B4 准备审计并拆为 B4A/B4B；冻结 replay identity/digest checkpoint、Base/varnish/统计、材料/Stage15/closure、生命周期与零漂移 Gate。B4A 转 PREPARED，B4B 等待 B4A。 |
| 2026-08-21 | v1.9 | MF-03B3 COMPLETE：实现非生产 InternalVoid/Shape/footprint/compact report sink/replay digest scanner，retained oracle 与 Release 定向回归通过；B4 和生产接线仍未准入。 |
| 2026-08-20 | v1.8 | MF-03B3 转 PREPARED：冻结 InternalVoid/Shape 精确顺序、type 同步、post-shape footprint、compact report sink、canonical digest 和非生产边界。 |
| 2026-08-20 | v1.7 | MF-03B2 COMPLETE：实现 bounded outer-boundary/unsupported 顺序 scanner、独立 retained oracle、强错误边界和热路径零分配 Gate；仍未接生产，B3/B4 继续部分准备。 |
| 2026-08-19 | v1.6 | MF-03B2 转 PREPARED：冻结 P1 outer-boundary 顺序扫描、P2 unsupported discovery 状态机、compact demand/事件摘要、取消与错误边界及 retained oracle；B3/B4 仍未准入。 |
| 2026-08-19 | v1.5 | MF-03B1 COMPLETE：实现 move-only range-derived support demand 与双 Mask caller-owned pre-shape 单层物化；retained oracle、错误/别名 Gate、Release 定向回归通过，未接生产。 |
| 2026-08-19 | v1.4 | 完成 MF-03B 可执行准备审计并拆为 B1..B4；冻结 P0..P4 多遍顺序、B1 纯合同边界和测试矩阵。仅 MF-03B1 转 PREPARED，完整生产重放仍未准入。 |
| 2026-08-18 | v1.3 | MF-02 COMPLETE：新增 owned layer 同步背压/错误合同并把 Legacy Adapter 改为移动交接；MF-03A COMPLETE：新增 compact occupancy ranges 和 caller-owned 单层物化，S0/S3/S4 与独立 Retained 对照零差异。MF-03B 仅完成部分准备，未接生产。 |
| 2026-08-18 | v1.2 | 完成 MF-02/03 开发准备补充：冻结 owned callback 生命周期与错误合同；将 MF-03 拆为可独立验证的 MF-03A Occupancy Materializer 和高风险 MF-03B 多遍支撑重放。MF-02 转 ACTIVE。 |
| 2026-08-18 | v1.1 | MF-01 COMPLETE：新增溢出安全 RasterMemoryBudget、稳定路由名和 123 大 Grid 预算证据；区分场景 RGBWSV 与逐实例 Mask 占用，独立用例 8/8、Release CTest 1/1 PASS。MF-02 转 PREPARED，生产仍为 Retained Dense。 |
| 2026-08-18 | v1.0 | 用户授权开启 16C-06-MEMFLOW；完成 MF-00 文档准备并启动 MF-01。 |
