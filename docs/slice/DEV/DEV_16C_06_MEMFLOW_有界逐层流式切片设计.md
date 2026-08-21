# DEV_16C-06-MEMFLOW 有界逐层流式切片设计

> 状态：**ACTIVE DESIGN / MF-01..03B4A IMPLEMENTED / MF-03B4B PREPARED**
> 版本：v1.6 ｜ 日期：2026-08-21
> 决策：`DOC_DECISION_16C_06_MEMFLOW_有界逐层流式内存根治.md`

## 1. 设计目标

把当前完整 Layer Volume 的交接方式拆为受控的 Layer Stream，同时保持现有算法、协议、报告、
严格校验和原子发布。采用 `wrap first, move later, rewrite last`：先包装现有 Dense 结果，再缩短
生命周期，最后才替换存储结构。

## 2. 目标数据流

```text
Model / Geometry
  -> Occupancy Range Provider
  -> bounded model/support/material layer window
  -> InstanceLayerProducer
  -> SceneLayerBarrier(globalLayerIndex)
  -> deterministic SceneLayerComposer
  -> StagedPackageLayerSink
  -> incremental statistics/report evidence
  -> strict RIP Reader on staging
  -> atomic publish
```

旧的 Retained Dense 路径保留为小作业路径、对照路径和回退入口。生产路由不允许在运行中从一种
路径静默切换到另一种路径；选择在写出任何 TIFF 前完成。

## 3. 模块边界

| 模块 | 责任 | 禁止事项 |
|---|---|---|
| `geometry/LayerOccupancyProvider` | 提供列范围、当前层/窗口占用 | 不决定材料、支撑或输出 |
| `support/*` | 使用范围/窗口生成最终支撑层 | 不写 TIFF/报告 |
| `pipeline/RasterMemoryBudget` | 估算和选择已实现路由 | 不读取文件、不修改 Profile |
| `pipeline/LegacySceneLayerAdapter` | 把同步 producer 包装为拥有语义的层流 | 不跨线程暴露借用引用 |
| `pipeline/SceneLayerComposer` | 同层确定性合成和统计 | 不发布 package |
| `output/rgbwsv` | staging、逐层 TIFF、报告、严格校验、原子发布 | 不决定几何/材料 |
| `SliceRunTelemetry` | 展示选择和真实水位 | 不参与路由决策 |

所有新核心类型保持 C++20 标准库，不依赖 Qt。SPI/Worker 外部合同首阶段不变。

## 4. 内存预算设计

### 4.1 纯函数合同

`PlanRasterMemoryBudget()` 输入 Grid、可见实例数、每像素常驻 Mask 字节、窗口层数、预算和已实现能力，
输出：

```text
route = retained_dense | bounded_dense_stream | blocked
pixelsPerLayer
retainedRgbwsvBytes
retainedMaskBytes
retainedRasterBytes
boundedWindowRgbwsvBytes
boundedWindowMaskBytes
boundedWindowBytes
memoryBudgetConfigured
retainedBudgetExceeded
boundedBudgetExceeded
reason
```

场景最终 RGBWSV 每层只计一次，ownership/instance Mask 按可见或在途实例计数。无效输入通过异常
fail closed，不提供可被误读的 `valid=false` 半有效结果。

所有乘加必须检查 `uint64_t` 溢出。预算为 0 表示“未配置”，继续 Retained Dense，不自行写死设备
内存。只有显式声明 `boundedDenseStreamingAvailable` 时预算器才能选择该路径。

### 4.2 预算不是峰值承诺

该估算只覆盖主要栅格字节，不包含模型、纹理、allocator、JSON 和第三方 Writer。因此它用于路由
和诊断，不能替代独立进程 Peak Working Set。后续可添加 `reserveBytes`，但不能用魔法百分比冒充
设备合同。

## 5. Layer Stream 合同

### 5.1 Producer

目标内部合同：

```cpp
struct ProducedSceneLayer {
    int layerIndex;
    double zMm;
    RgbwsvProductionLayer output;
    ownership masks;
};

using OwnedLayerConsumer = function<LayerConsumeResult(ProducedSceneLayer&&)>;
```

调用同步；返回后 producer 不再访问已移动数据。首版不引入线程队列，先证明生命周期和输出等价。

### 5.2 Window

窗口最小为当前层；需要邻层/halo 的算法显式声明前后半径。请求超出窗口能力时在作业开始前
fail closed 或走 Retained Dense，不能运行到中途再回退。

### 5.3 Backpressure

同步阶段天然背压。后续有限异步队列必须同时限制层数和字节数，以字节预算为权威；不得只限制
“队列 3 层”却忽略 DPI/幅面变化。

## 6. Occupancy/Support 两阶段设计

### 6.1 Phase A：范围事实

```text
每个 XY 列：0..N 个有序 Z 区间或首末层范围
可选 2x2 边界覆盖：按已冻结 S3/S4 规则形成范围
```

`relief_heightfield` 可先使用单区间列；通用网格若一列有多个区间，不得退化为 `min/max` 填满
内部空洞。

### 6.2 Phase B：按层物化

当前层的 model/support/type/varnish mask 在复用缓冲中生成。铺底和上下投影优先使用列范围；必须
做连通域或形态学处理时，可使用固定 Tile halo 或显式两遍扫描。

### 6.3 必须保留的语义

```text
SupportType 优先级和统计
baseProjection 真实 layerIndex 范围
outerVarnish 与 support 清理次序
material closure exact/repair
Stage 15 white carrier
```

### 6.4 MF-03B1 pre-shape 支撑需求合同

`BoundedSupportDemandPlan` 只接受 `SingleIntervalHeightfield` 的已校验逐列事实，持有
lower/model-last/upper-boundary-last/unsupported-source 四组 compact 数据并禁止深拷贝。单层
materializer 接收独立的 model 与 upper-boundary Mask，写入调用方复用的 support/type buffer；
Upper 额外受 upper-boundary 抑制，其他三类只受 model 优先约束。该能力只覆盖 Bottom、Full、
Upper、Unsupported 的 pre-shape 语义，不包含岛检测、InternalVoid、shape、baseProjection、
varnish 或 material，也不接生产路由。

实现保持每层 O(pixelCount)、无动态分配；别名、非二值 Mask、非法范围和 General Mesh 均在写
输出前 fail closed。公共 `SupportTypePriority` 与 retained 路径共用，避免两套优先级漂移。

### 6.5 MF-03B2 顺序扫描合同

P1 scanner 只保留单层 outer-varnish scratch 和逐列 upper-boundary last；P2 scanner 只保留
previous model/boundary、preliminary support、膨胀/访问/component scratch、逐列 unsupported top
及逐源层 compact 事件。两者只接受从 0 开始的严格层序，未完整消费不得生成可用结果。

P2 的 previous support 由 MF-03B1 plan 仅重放 Bottom/Full/Upper，不能包含本遍此前发现的
Unsupported。这样保持 retained 实现“先生成全层 preliminary support，再顺序发现岛并向低层回写”
的精确时序，同时不保留完整 support volume。

MF-03B2 已按上述合同实现为非生产能力。P1/P2 构造期一次性分配 O(pixelCount + layerCount)，逐层
热路径不分配；P1/P2 输入在状态修改前完成层序、尺寸、二值、别名和 upper-boundary 包含 model
校验。完成结果为 move-only compact facts/events；未完整消费时 `Finish()` 失败。该实现未接
`run_slicer`、Production Service、Profile 或输出路径，Retained Dense 仍是生产路线。

### 6.6 MF-03B3 Shape/footprint 重放合同

`BoundedSupportShapeScanner` 使用 MF-03B1 final plan 顺序重建每层 support，随后执行 retained-equivalent
InternalVoid、现有 Shape optimizer 和 SupportType 同步。在 BaseProjection 与 varnish priority 之前，
它累计 O(XY) 最大 footprint，并为每层形成带固定域、grid/policy、model/upper、support/type 与 compact
report 的 SHA-256 replay digest。

组件像素只在当前层计算期间存在，交接报告只保留 aggregate、area/bbox、filtered、bridged 和 warning；
同步 sink 返回后才提交 caller-owned output 和 scanner 状态。结果不保存 layer x pixel Mask 栈。
该能力只由 Stage 16 测试引用，未连接生产路由；BaseProjection、outer/surface varnish priority、材料与
closure 的最终逐层重放仍属于 MF-03B4。

### 6.7 MF-03B4A Verified support final replay

`BoundedSupportFinalReplayScanner` 位于 support 模块，只消费 B1 final plan、B3 completed result 与
同层 model/upper/outer-varnish Mask。它通过 B3 canonical 实现先在内部 scratch 重放并验证同层
digest，再按 B3 footprint 应用 BaseProjection、outer-varnish priority 和 final support/connectivity
统计。输出由调用方持有；Result 不保存 Raster 层栈。

```text
support dependency direction:
BoundedSupportDemand -> BoundedSupportShapeScan -> BoundedSupportFinalReplay
materials/output/apps 不得成为 support 的依赖
```

`prepend_below_model` 的模型抬高发生在 Grid 建立前；B4A 只处理最终物理层 `0..N-1`，并保留前 N
层既有 type 覆盖为 ProjectionBase 的 retained 行为。outer-varnish 清除发生在 Base 后，cleared overlap
作为 B4B closure evidence 交接，但不计入 final support stats。任何 digest/sink 失败都发生在 caller
output 提交前并终止 scanner。

实现结果：B3 `ReplayIdentity` 已纳入 B1 final plan 的 canonical digest，并提供 verified Consume，
使 B4A 在 B3 report/output/state 可观察提交前完成逐层校验。B4A 使用当前层 scratch 复用完成 Base、
outer priority、type totals 与 connectivity；组件 area/bbox 通过仅在同步 sink 调用期间有效的 span
交接并按 retained 面积顺序排序。该实现及 10 组测试仅进入 support/CMake Stage 16 测试边界，
未连接生产路由。

### 6.8 MF-03B4B Material/closure final replay

B4B 位于 materials/pipeline core 边界，消费 B4A 同层 view 与冻结的 texture/material facts，按 retained
顺序执行 compose、Stage 15 white carrier、closure exact/repair/re-detect 和 repair 后 totals。它只向
同步 sink 交接 caller-owned RGBWSV/semantic evidence，不写 TIFF、preview 或 report 文件；MF-04 才把
完成层交给 StagedPackageLayerSink。B4B 不允许反向依赖 support 私有实现，也不允许 UI 读取临时结构。

## 7. Scene Layer Barrier

### 7.1 状态机

```text
Waiting(layer i)
  -> 每个 visible instance 提交 layer i 或 Empty(i)
  -> Validate identity/revision/grid/protocol
  -> Compose in frozen scene order
  -> Emit exactly one global layer i
  -> Release all instance layer i buffers
  -> Waiting(layer i + 1)
```

乱序提交首版拒绝。隐藏实例不占屏障。任何实例失败、取消、stale 或层序错误，整个 scene 作业
失败；不发布已有 TIFF staging。

### 7.2 单实例快速路径

单实例 full-grid 可直接移动 output；统计在同一校验扫描形成。该路径延续 16C-05，不再构造
Scene ownership 整栈。

## 8. Staged Package Layer Sink

Writer 首先获得不可变 metadata 和已验证的 Grid/协议，然后获取 lease、创建 staging。每收到一层：

```text
验证 layerIndex/z/dimension/byte count
写 TIFF 到 staging/layers
累计 layer entry、六通道统计和 preview evidence
释放层 buffer
```

最后写 manifest/report，运行严格 Reader，成功后原子发布。中途失败由现有 artifact recovery 清理；
已有正式 package 继续保留。

## 9. Telemetry 与数据同步

新增字段只进入诊断数据，不进入 `p0.rgbwsv.2`：

```text
memoryRoute
memoryRouteReason
memoryBudgetBytes
estimatedRetainedRasterBytes
estimatedBoundedWindowBytes
peakBufferedLayerCount
peakBufferedRasterBytes
occupancyMaterialization = retained_volume | bounded_window | sparse_candidate
```

Host 只能展示 Worker/Package 返回事实，不自行重算。无法测量的字段为 null，不用估算值冒充实测。

## 10. 自适应策略

```text
预算未配置                   -> Retained Dense
估算在预算内                 -> Retained Dense
超预算且 Bounded 已通过 Gate -> Bounded Dense Stream
超预算且无已通过路径         -> fail closed + 明确诊断
Sparse 未通过 Gate           -> 永不自动选择
```

后续可增加“小模型阈值”，但阈值必须由 Release 数据决定，不在设计文档硬编码。

## 11. 测试设计

### L1 合同

```text
整数溢出、无效尺寸、预算未配置、预算内、预算外
123 大 Grid 选择 bounded 候选
能力未实现时 fail closed
```

### L2 逐层语义

```text
S0/S3/S4
支撑 off/on、baseProjection、outer varnish、surface varnish
Stage 15 white carrier、material closure exact/repair
每层 RGBWSV/ownership/statistics SHA-256
```

### L3 Scene/Writer

```text
1/11/12/22 实例
层序、stale、overlap、cancel、Writer 失败、lease 冲突、恢复
manifest/report/preview/RIP strict
```

### L4 性能

固定 commit/build/compiler/Profile/asset/config hash；记录 wall、CPU、Peak Working Set、分页、阶段
计时和输出 hash。相同请求至少交替 A/B 各 3 次，不能拿不同层厚或压缩结果比较。

## 12. 实施顺序

```text
MF-00 文档、基线和状态同步
MF-01 RasterMemoryBudget 纯合同
MF-02 Owned Layer Producer/Sink，仍由测试消费
MF-03 Occupancy/Support 有界物化（B4A support final replay -> B4B material/closure replay）
MF-04 单实例流式 Package
MF-05 多实例 Layer Barrier
MF-06 Sparse Tile/Span 候选
MF-07 自适应生产接入和诊断
MF-08 Release/RIP/恢复/性能收口
```

每张卡只在上一张语义 Gate 通过后进入；MF-06 不与 MF-03..05 并行开发。
