# TASKS_16C-06-MEMFLOW 有界流式内存根治专项任务清单

> 文档状态：**ACTIVE / MF-01..03B4A COMPLETE / MF-03B4B 接口已接线 / MF-03X1 COMPLETE**
> 版本：v2.5 ｜ 日期：2026-09-04
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
| MF-03X2a | 主循环有界接线·**用户阻塞配置**（bottom_projection，无岛/无形状/无光油） | PREPARED / **解除阻塞关键路径** | MF-03X1、MF-03B1、MF-03A | - |
| MF-03X2b | 主循环有界接线·支撑耦合簇全模式（岛发现 + 形状 + 光油） | PREPARED / 范围待估算 | MF-03X2a、B2/B3/B4A | - |
| MF-04 | 单实例流式 Staged Package | PENDING / **范围已重定义** | MF-03B4A/B COMPLETE | - |
| MF-05 | 多实例 Global Layer Barrier | PENDING | MF-04 | - |
| MF-06 | Sparse Tile/Span 显式候选 | PENDING | MF-05 | - |
| MF-07 | 自适应生产路由与 Telemetry 接入 | PENDING | MF-06 Gate 或明确跳过 Sparse | - |
| MF-08 | 真实模型、RIP、恢复与性能收口 | PENDING / INPUT OPEN | MF-07、设备输入 | - |

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

## 9. MF-05 多实例 Layer Barrier

**目标：** 1/11/12/22 场景按 global layer 同步合成，释放同层所有实例 buffer。

**验收：** scene 顺序确定、重叠/冲突/stale/层缺失 fail closed；实例完成顺序不影响输出 hash。

## 10. MF-06 Sparse Tile/Span 候选

**目标：** 只处理 active rect/tile/span，降低约 3% 占用场景的空白扫描。

**验收：** `123.stl` 17 个连通分量、小组件、边界、支撑、光油 halo 不丢失；未通过前不允许自动路由。

## 11. MF-07 自适应生产路由

**目标：** 在作业开始前按显式内存预算和已实现能力选路；Host 只展示 Worker 权威 telemetry。

**验收：** 小作业 retained、大作业 bounded；无法满足预算时明确失败；不运行中途回退；Profile hash
和输出协议不变。

## 12. MF-08 收口

**矩阵：** `123.stl`、Reality 5/5、标准甲片、Stage 15 fixture，S0/S3/S4，support/material/varnish，
1/11/12/22，cold/warm，Retained/Bounded/Sparse candidate。

**出口：** 逐层 hash、RIP strict、取消恢复、wall/CPU/Peak Working Set 和 build identity。正式设备
SLA/内存上限缺失时，只完成工程 Gate，不宣称 production SLA PASS。

## 13. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
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
