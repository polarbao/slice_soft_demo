# REPORT_16C-06-MEMFLOW 有界流式内存根治当前状态

> 状态：**ACTIVE / MF-00..03B3 COMPLETE / MF-03B4 PREPARATION PARTIAL**
> 日期：2026-08-21
> 任务真源：`TASKS_16C_06_MEMFLOW_有界流式内存根治专项任务清单.md`

## 1. 当前状态

用户已授权针对大幅面稀疏模型开启根治专项，并允许准备完成后进入开发。MF-00 已完成：决策、
技术设计、实施准备、原子任务和执行边界已建立。

当前生产代码仍以 Retained Dense 为主。16C-05 的 Mask 按需分配、统计融合和单实例 Buffer 移动
继续有效；MF-01 提供路由纯合同，MF-02 消除了 `run_slicer -> LegacySceneLayerAdapter` 的层交接
复制，MF-03A 提供 compact occupancy range 和单层物化 API，MF-03B1 提供 range-derived
pre-shape 支撑需求和 caller-owned 单层物化。上述能力均未把 Production Service
切换到 Bounded 路由，不能表述为完整流式化。

MF-03B2 现已提供 bounded outer-boundary 与 unsupported discovery 顺序扫描器、compact 需求和
逐源层事件摘要；MF-03B3 进一步提供 InternalVoid/Shape、post-shape footprint、compact report sink
和逐层 replay digest。两者都只属于非生产能力，尚未包含 BaseProjection/final varnish/material
重放，也没有连接 Production Service。

## 2. 当前数据事实

`123.stl` 的容量压力来自约 10.39～11.00 亿 pixel-layer 样本，而不是约 4.1 万三角形。六通道
Dense 输出本身约 5.8～6.1 GiB；历史非空占比约 3.06%。因此：

```text
Dense Streaming 首先解决峰值内存/分页；
Sparse Tile/Span 才进一步减少空白 CPU 扫描；
两者必须分开验证。
```

## 3. 已冻结边界

```text
协议、通道、极性、默认采样、姿态、材料和支撑语义不变；
Writer staging/strict validation/atomic publish 不放松；
小作业保留 Retained Dense；
Sparse 未过独立 Gate 前不自动选择；
设备 SLA 未提供，最终 production Gate 保持 INPUT_OPEN。
```

## 4. MF-01 实际结果

新增 `src/slicer_core/pipeline/RasterMemoryBudget.*`：

```text
route = retained_dense | bounded_dense_stream | blocked
预算未设置 -> retained_dense
预算内 -> retained_dense
超预算 + Bounded 能力可用 + 窗口在预算内 -> bounded_dense_stream
否则 -> blocked
```

所有 Raster 字节乘加均检查 `uint64_t` 溢出。`123.stl` 观测 Grid 的合同测试得到：

```text
pixelsPerLayer              7,369,346
retainedRgbwsvBytes         6,234,466,716
retainedRasterBytes 下界   10,390,777,860
boundedWindowBytes          221,080,380  （3 层、4 B/pixel Mask）
4 GiB 预算路由              bounded_dense_stream
```

该值是主要 Raster 下界，不含模型、纹理、allocator、JSON 和 TIFF 库，不能冒充实际 Peak Working
Set。

## 5. MF-02 / MF-03A / MF-03B1 实际结果

```text
MF-02  Legacy owned layer 同步背压；RGBWSV/semantic 在 producer 最后读取后移动
       const callback 保持兼容；双 callback、owned+写出、Global owned 均提前拒绝
       Adapter 取消/失败/异常清空部分层；非法状态稳定映射 Failed；独立用例 7/7 PASS

MF-03A primary/四 subsample compact ranges；caller-owned 单层 materializer
       DTO 构造后不可变并仅在 Build 时完整校验；Materialize 热路径不重复扫描全部 ranges
       S0/S3/S4 对独立 Retained 实现全层逐字节相等；阈值空洞不被 min/max 填平
       GeneralMesh 候选边界、层号、buffer、input kind 均 fail closed；257x32 生成对照 PASS

MF-03B1 move-only range-derived support demand；双 Mask caller-owned pre-shape materializer
         Bottom/Full/Upper/Unsupported 开闭区间和优先级与 retained oracle 全层零差异
         错误事实、别名、非二值 Mask 和 GeneralMesh fail closed；逐层热路径无分配
```

MF-02 只减少层交接复制，Adapter 仍保留完整 Raster；MF-03A/03B1/03B2 目前只建立能力，不接生产。因此实际
Peak Working Set 和 `123.stl` 总耗时尚未形成新 A/B 结论。

## 6. MF-03B2 实际结果

```text
P1 逐层 outer varnish / upper boundary 对独立 retained 公式全等；封闭孔不误判为 outer
P2 preliminary previous base 只含 model + Bottom/Full/Upper，明确排除 Unsupported/Shape/Base/Varnish
compact unsupported top、逐源层事件和 totals 对 dense retained oracle 全等
4/8 连通、严格 overlap/area 边界、层序、提前 Finish、错误尺寸/二值/别名/GeneralMesh fail closed
upper boundary 必须包含 model；错误调用不推进状态，可由同一层正确重试
P1/P2 ConsumeLayer 分配计数均为 0；新 scanner 未接生产路径
```

## 7. 验证状态

MF-03B3 当前结果：

```text
B1 final demand -> InternalVoid -> Shape -> type sync 顺序与 retained 对照全等
post-shape footprint 为全层 support OR；compact report 不保留 component pixels
SHA-256 replay digest 重复确定，并覆盖 upper-boundary 与 policy/report 域
mask-only ConsumeLayer 零分配并复用 caller/scratch buffer；固定 digest golden 冻结 canonical 编码
错误输入不推进状态；sink 异常不提交 caller output，并永久终止 scanner
新 scanner 仅由 support 模块自身和 Stage 16 测试引用，生产零接线
```

| Gate | 状态 |
|---|---|
| 文档和上下文互链 | PASS |
| MF-01 独立用例 | 8/8 PASS |
| MF-01 Release CTest | 1/1 PASS |
| MF-02 独立用例 / Release CTest | 7/7 / 1/1 PASS |
| MF-03A S0/S3/S4 等价与错误 Gate | PASS |
| MF-03B1 retained oracle / 错误与别名 Gate | PASS |
| MF-03B2 retained oracle / 状态机 / 零分配 Gate | 7/7 PASS |
| MF-02/03A/03B1/03B2/03B3 + outer varnish Release CTest | 6/6 PASS |
| MF-03B3 retained oracle / digest golden / scratch / 错误 Gate | 6/6 PASS |
| 既有 support shape / `slicer_cli` 链接 | 11/11 PASS / PASS |
| Router / Global 兼容定向测试 | 2/2 PASS |
| Scene Adapter 现有套件 | 11/12 PASS；修改前已存在的平移断言仍失败 |
| Dense Streaming 逐层 hash | NOT STARTED |
| 单实例 Package/RIP | NOT STARTED |
| 多实例 Barrier | NOT STARTED |
| Sparse candidate | NOT STARTED |
| `123.stl` 同请求 A/B | INPUT SNAPSHOT PENDING |

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-21 | v1.5 | MF-03B3 完成：非生产 InternalVoid/Shape/footprint/compact report/replay digest Gate 通过；生产仍为 Retained Dense。 |
| 2026-08-20 | v1.4 | MF-03B2 完成：bounded P1/P2 scanner、retained oracle、错误恢复和热路径零分配 Gate 通过；生产仍为 Retained Dense。 |
| 2026-08-19 | v1.3 | MF-03B1 完成：move-only 支撑需求计划、双 Mask caller-owned 单层物化、retained oracle 和 fail-closed Gate 通过；仍未接生产，B2..B4 继续部分准备。 |
| 2026-08-18 | v1.2 | MF-02/03A 完成：owned 层交接、取消/失败清理、Global fail-closed、compact occupancy range 和单层物化 Gate 通过；生产仍为 Retained Dense，MF-03B 仅部分准备。 |
| 2026-08-18 | v1.1 | MF-01 完成：预算/路由纯合同、123 大 Grid 精确下界、场景 RGBWSV/逐实例 Mask 分离计数、溢出和 fail-closed 用例通过；MF-02 转 PREPARED，未接入生产。 |
| 2026-08-18 | v1.0 | 建立专项状态报告；MF-00 完成，MF-01 启动，其余 Gate 保持未开始。 |
