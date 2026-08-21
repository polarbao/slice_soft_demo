# DOC_PREP_16C-06-MEMFLOW MF-02/03 开发准备补充

> 状态：**MF-02/03A/03B1/03B2/03B3/03B4A IMPLEMENTED / MF-03B4B PREPARED**
> 日期：2026-08-21
> 上游：`DOC_DECISION_16C_06_MEMFLOW_有界逐层流式内存根治.md`、
> `DEV_16C_06_MEMFLOW_有界逐层流式切片设计.md`

## 1. 准备裁决

MF-02 可以实施。完整 MF-03 尚不能一次接入生产，先拆为：

```text
MF-03A  Occupancy Range + caller-owned 单层 Materializer；旧完整结果保留为独立 Retained 对照
MF-03B  Support/varnish/material 多遍需求扫描和最终逐层重放
```

MF-03A 不改变 `run_slicer` 的生产数据流。MF-03B 在组合语义 Gate 建立前保持 PENDING。

## 2. MF-02 冻结合同

```text
owned callback 与旧 const callback 互斥，作业开始前 fail closed；
owned callback 仅在 producer 对 layer/semantic 最后一次读取后调用；
owned 模式禁止 producer 同时写 TIFF、preview、report；
结果为 Accepted / Cancelled / Failed，并携带稳定 layerIndex/detail；
同步调用形成背压；回调返回后 producer 永不访问 moved payload；
Cancelled/Failed/异常均终止生产，不再发下一层；Adapter 清除已收集部分层；
旧 const callback 的调用位置、写出能力和行为保持不变；
Global 路由不得静默忽略 owned callback。
```

MF-02 只消除 `run_slicer -> LegacySceneLayerAdapter` 的 RGBWSV 和 ownership 交接复制；Adapter
仍返回完整 Retained Raster，因此不宣称峰值内存根治完成。

## 3. MF-03A 冻结合同

`LayerOccupancyRanges` 保存经过现有边界规则裁剪后的层区间：

```text
primary interval / output column
4 independent subsample intervals / output column（仅 S3/S4）
layerCount、columnCount、policy、inputKind
exact first/last occupied layer derived from the selected per-layer threshold
```

S3/S4 必须逐层计算 1/4 或 2/4 阈值，不能把四个 subsample 合并为单一 `min/max`。旧
`BuildLayerOccupancy()` 暂时保留为独立 Retained 对照，避免在 MF-03B Gate 前改变生产实现和增加
重复扫描；新 materializer 的每层输出必须与该对照逐字节相等。错误层号、错误 buffer 大小和不支持
的 General Mesh 候选继续 fail closed。

实现约束补充：compact DTO 构造后不可变，完整结构/summary 校验只在 Build 时执行一次；逐层
Materialize 热路径只做 O(1) 层号/尺寸检查并融合必要的 per-column 判断，不得每层先额外扫描全部
ranges。S3/S4 的 summary 由四区间固定事件点推导，不执行 columnCount x layerCount 的构建扫描。

## 4. MF-03B 多遍需求

完整有界支撑不能简化为普通前向窗口。后续生产接线必须严格遵循下列冻结顺序：

```text
P0 Preflight
   固定 config/grid/policy/build identity；仅 SingleIntervalHeightfield 可进入 bounded 候选；
   GeneralMesh 保持 Retained 或 fail closed；在写任何 TIFF 前完成能力和预算选路。

P1 Geometry / outer-boundary scan
   逐层物化 model 和 outer varnish；以 model OR outer varnish 更新每列
   upperBoundaryLastLayer；surface varnish 不进入本遍。

P2 Unsupported discovery
   按 i=1..N-1 顺序检测 current model island；previous base 只含
   model[i-1] OR range-derived preliminarySupport[i-1]；不得包含 unsupported、
   internalVoid、shape、baseProjection 或 varnish；把岛回写需求压缩为
   unsupportedTopExclusive[pixel]，诊断归属源层 i。

P3 Shape / footprint scan
   逐层重建 range support + unsupported，添加 InternalVoid，再执行 Shape 和 type 同步；
   从 Shape 后结果累计全层 support footprint；只保留丢弃 component pixels 后的 compact report
   和逐层 replay digest，不保留 Mask 栈。

P4 Final replay
   重建 model/outer+surface varnish/support，重放 InternalVoid/Shape 并校验 digest；
   再应用 baseProjection，保留 prepend_below_model 对前 N 层既有 SupportType 的覆盖语义；
   随后按 outer-varnish priority 清 support/type；之后才累计 support/connectivity、compose、
   Stage 15 carrier、closure、channel/semantic totals、TIFF/preview/callback。
```

Texture/Material 报告只能在 P4 累计，扫描遍不得重复计数。S3/S4 四个 subsample 区间不得合并为
`min/max`；General Mesh 不得单区间化；不得借本专项顺手修正既有统计口径。

### 4.1 MF-03B1 可执行边界

MF-03B1 只实现 `Range-derived Support Demand` 纯合同和 caller-owned pre-shape 单层物化：

```text
输入事实：lowerSourceLayer、modelLastLayer、upperBoundaryLastLayer、
          unsupportedTopExclusiveLayer（由 fixture/未来 P2 提供）
当前层输入：modelOccupancyMask；upperBoundaryOccupancyMask（model OR 可选 outer varnish）
输出类型：BottomProjection / FullVerticalProjection / UpperProjection / UnsupportedIsland
优先级：Unsupported > Full > Upper > Bottom；模型像素始终优先，不生成支撑
范围：Bottom [0, lower)、Full [0, last)、Upper (upper, layerCount)、
      Unsupported [0, sourceLayer)，sourceLayer 为 1..layerCount-1 且位于模型范围内
```

Upper 除模型优先外还必须跳过 `upperBoundaryOccupancyMask`，而其他三类不得因 outer varnish
提前被抑制。合同必须保存不可变事实，逐层热路径只做 O(1) 层号/尺寸校验和 O(pixelCount) 物化；输出 buffer
由调用方持有并在每次调用时完整覆盖。General Mesh、非法 input kind、向量尺寸不一致、越界/半空
范围、半空模型摘要、未包含模型的 upper boundary、模型范围外的 unsupported source 均 fail closed。
Plan 为 move-only，避免大 Grid 四组逐列事实被意外深拷贝。InternalVoid、Shape、ProjectionBase、
Varnish 和岛检测明确不属于 B1。

### 4.2 MF-03B2 可执行边界

MF-03B2 只实现 P1/P2 的顺序 scanner，不接 retained producer 或 Production Service：

```text
P1 输入：严格递增的 layerIndex、当前 model Mask、outer varnish 离散化与 upper-boundary 策略
P1 输出：调用方复用的 outerVarnishMask / upperBoundaryMask；完成后得到逐列 upperBoundaryLastLayer
P2 输入：严格递增的 model/upperBoundary Mask、B1 preliminary plan、岛检测策略
P2 previous base：model[i-1] OR Bottom/Full/Upper preliminarySupport[i-1]，再做现有 8 邻域膨胀
P2 输出：逐列 unsupportedTopExclusiveLayer、逐源层 island/filter 事件摘要及既有总计口径
```

Outer varnish 只取同层 model 的外部空域与物理椭圆膨胀交集；upper boundary 在策略启用时为
`model OR outerVarnish`，否则为 model。P2 连通性仅允许 4/8，接受条件严格保持
`overlapRatio < minOverlapRatio`，过滤条件严格保持 `area < minIslandAreaPx`；接受岛在逐列事实中
记录 `max(existing, sourceLayer)`，源层只允许 1..layerCount-1。事件统计归属检测源层，不从最终
支撑 Mask 反推。

两个 scanner 均构造期分配固定 O(pixelCount + layerCount) scratch/result，逐层调用不得分配；
输入先完整二值/尺寸/别名校验，再修改输出或状态。调用方可在层间取消并丢弃 scanner；只有消费
全部层后 `Finish()` 才返回结果，提前 Finish、乱序/重复/跳层、General Mesh、非法策略或 buffer
均 fail closed。MF-03B2 明确不包含 InternalVoid、Shape、footprint、baseProjection、varnish priority、
material/closure/report 重放；这些仍属于 B3/B4。

### 4.3 MF-03B3 可执行边界

MF-03B3 只实现 P3 的非生产顺序 scanner，不接 retained producer 或 Production Service：

```text
逐层顺序：B1 final demand 物化 -> InternalVoid -> Shape -> SupportType 同步
          -> post-shape footprint OR -> canonical replay digest -> compact report sink
禁止步骤：BaseProjection、outer/surface varnish priority、最终 support 统计、
          material/closure、TIFF/preview/report 文件写出
```

InternalVoid 只根据当前 model Mask 判断封闭空域，不读取 support：先按配置 4/8 连通从边框洪泛外部
空域，再扫描封闭分量；`area < minAreaPx` 跳过，等于阈值接受。接受像素按
`SupportType::InternalVoid` 最高优先级写入，即既有 support 的 type 也升级。

Shape 的 retained 顺序固定为 pre component analysis、严格 `< minComponentAreaPx` 过滤、方形 dilation、
simple closing、水平 bridge、垂直 bridge、model priority、max-added rollback、post analysis。新增 Shape
像素 type 为 `BottomProjection`，删除像素 type 清为 `None`，既有像素 type 保持。rollback 只撤销
Shape 新增像素，不恢复过滤删除；既有 gap/warning 报告保持 retained 行为。现 retained 在 model priority
清理前计算 added/removed 和 max-added 上限，B3 必须原样保留，不借机修正统计口径。

footprint 是所有层 InternalVoid+Shape 后、BaseProjection/varnish priority 前 support Mask 的 OR。
replay digest 使用 SHA-256 raw 32-byte，固定版本域、小端 layer/grid/policy 元数据，随后覆盖 model、
upper-boundary、最终 support、显式 uint8 SupportType 和 compact layer report 的 canonical bytes。Shape
禁用时仍生成 InternalVoid 后 mask/type 的 digest。P4 必须在相同点复算并逐层比较，失败不得继续写出。

报告以同步 sink 逐层交接，只保存 area/bbox、aggregate、filtered、bridged 和 warning；所有
`SupportComponentInfo::pixels` 在交接前清空。Scanner Result 仅保存 footprint、footprintPixels、逐层
固定长度 digest 和 totals，不保存逐层 Mask 或完整 layer report。sink 抛异常时 scanner 终止失败。

只接受 `SingleIntervalHeightfield`、严格 `0..N-1` 层序和兼容的 final B1 plan。尺寸、二值、别名、
connectivity、负值/非 finite policy、plan identity 必须在状态修改前 fail closed；校验错误允许同层修正
重试，未消费全部层不得 Finish。层间取消由调用方销毁 scanner 表达，不返回半结果。Mask scratch
构造期一次分配并复用；动态 report 事件允许有界分配，但不得出现 layer x pixel Mask 栈。

### 4.4 MF-03B4A 支撑最终重放合同

MF-03B4A 只实现非生产 P4 的支撑侧最终化，禁止 material、RGBWSV、TIFF、preview、report 文件、
callback、Profile、SPI、Worker 或 Production Service 接线：

```text
输入：B1 final plan、B3 completed result、严格递增的 model/upperBoundary/outerVarnish Mask
内部：使用 B3 相同 canonical 实现重放 pre-base support/type，并在提交任何 caller output 前
      逐层比较 B3 digest
顺序：verified B3 replay -> BaseProjection -> outer-varnish priority
      -> final support/type/connectivity statistics -> synchronous compact sink
输出：caller-owned final support/type/clearedOuterOverlap Mask；Result 只保留 O(1) totals 和
      BaseProjection summary，不保留 layer x pixel Mask 栈
```

`BoundedSupportReplayIdentity` 冻结 digest version、width/height/layerCount/inputKind、connectivity、
InternalVoid/Shape policy fingerprint。B3 Result 携带该 identity；B4A 构造期必须验证 result、request 与
plan identity 全等。B3 scanner 的 verified 模式在每层 report sink 与 caller output 提交前比较 expected
digest；mismatch 使 scanner 永久失败，禁止同层输出、后续 Consume 或 Finish。

BaseProjection 直接复用 B3 的 post-shape 全层 footprint。`effectiveLayerCount =
min(config.layerCount, layerCount)`；只处理物理 `layerIndex [0, effectiveLayerCount)`。模型已在 Grid
建立前按 `prepend_below_model` preparation 抬高，P4 不再改变物理层号。每个 footprint 像素仍遵守
model priority 和已有 support 保留：空 support 才新增。`overlay_existing` 只把新增像素标为
`ProjectionBase`；`prepend_below_model` 必须把有效前 N 层全部既有和新增 support type 覆盖为
`ProjectionBase`，与 retained 当前行为一致。

Base 之后才应用 outer-varnish priority：重叠 support 清零且 type 设为 `None`，同时写入 caller-owned
`clearedOuterOverlapMask`。被清像素不进入 final support/type/connectivity totals，但 B4B closure 必须把
该 Mask 恢复进 `supportRequiredMask` / expected occupied domain。surface varnish 位于模型像素内部，
不清 support，不属于 B4A。

错误层序、尺寸、非二值、任意输入/输出别名、GeneralMesh、非法 config、identity/digest 不匹配均
fail closed。可校验输入错误不推进状态且允许同层修正重试；digest 或 sink 异常发生在内部 scratch 上，
不提交 caller output，并永久终止 scanner。层间取消由销毁 scanner 表达，不返回半结果；未完整消费
不得 Finish。所有 mask/type scratch 在构造期一次分配并复用。

### 4.5 MF-03B4B 材料与闭合最终重放合同

MF-03B4B 是独立的非生产材料卡，依赖 B4A COMPLETE，当前只冻结合同、不随 B4A 一并实现：

```text
输入：B4A 同层 final support/type/clearedOuterOverlap，model、outer/surface varnish，
      texture/material-role/column facts identity，closure config
顺序：compose material -> Stage 15 white carrier -> populate empty mask
      -> closure exact analysis -> optional repair -> re-detect remaining gaps
      -> repaired final channel/semantic/material totals -> synchronous layer sink
输出：caller-owned RGBWSV 与 semantic evidence；Result 仅保留 compact totals
```

Texture/Material/Stage 15 计数只能在 B4B 最终遍累计一次。closure 的 `supportRequiredMask` 必须是
final support OR B4A cleared overlap；outer/surface varnish ownership 保持 retained 口径。repair 后必须
重新检测 remaining gaps，channel/semantic totals 只能读取 repair 后最终 RGBWSV。B4B 不复用
experimental `MaterialChannelComposer`，不得把 `slicer.cpp` 匿名 DTO 暴露给 UI；实施前先提取 retained
等价的 core DTO/helper，并以独立 oracle 校验。

B4B 同样采用严格层序、caller-owned buffer、同步 sink、无半结果和 fail-closed 状态机；sink 返回前
不得发布层，取消/错误不得留下可消费 RGBWSV。它不写文件、不接生产，MF-04 才负责 staging Writer。

## 5. 验证矩阵

### MF-02

```text
旧 const 与 owned 的逐层 RGBWSV/semantic 等价；
owned payload 二次 move 后地址不变；
同步背压、严格层序、Accepted 全消费；
Cancelled / Failed / throw 在目标层终止且错误稳定；
双 callback、owned + 任一 write flag 在输出目录创建前拒绝；
Adapter 成功等价、取消/失败不返回部分层。
```

### MF-03A

```text
S0 / S3 / S4 全层 mask 与兼容 wrapper 零差异；
S3/S4 非连续 subsample 阈值不被 min/max 填平；
caller-owned buffer 被复用且尺寸/层号错误 fail closed；
General Mesh 原有允许/拒绝边界不变。
```

### MF-03B（后续）

```text
SupportType 优先级、unsupported/baseProjection 层范围、varnish 清理次序；
Stage 15 white carrier、material closure exact/repair；
真实模型连通分量和支撑连续层不减少；
Retained/Bounded 逐层 RGBWSV/ownership/statistics hash 全等。
```

### MF-03B4A

```text
disabled/overlay/prepend/clamp、model priority 与 prepend type 覆盖逐层全等；
digest mismatch 在 sink/caller output 前失败，identity/policy/plan 不匹配构造期拒绝；
Base 后 outer-varnish 清理、cleared overlap evidence、final support/type totals 全等；
4/8 connectivity 的 component count/largest/small/tiny 与 retained oracle 全等；
层序、尺寸、二值、别名、sink failure、提前/重复 Finish 和取消边界；
caller buffer/scratch 地址复用，mask-only hot path 无 layer x pixel 保留；
Release MF-03B4A、B3/B2/B1/03A/02、outer varnish、support shape 回归。
```

### MF-03B4B（实施前 Gate）

```text
support off/on、Base、outer/surface varnish、Stage 15 white_underbase；
closure disabled/diagnostic/exact/repair_then_report，repair 后重检；
texture/materialPolicy/materialRoleMapping/modelFill 组合；
逐层 RGBWSV、semantic ownership、channel/support/material totals 与独立 retained oracle 全等；
不写 TIFF/preview/report 文件，不接生产；错误/取消/sink/缓冲复用无半结果。
```

### MF-03B1

```text
Bottom/Full/Upper/Unsupported 精确开闭边界；
模型优先与 Unsupported > Full > Upper > Bottom 冲突优先级；
upperBoundary mask 只抑制 Upper，不提前抑制 Bottom/Full/Unsupported；
禁用模式、空列、错误层号、错误 buffer/向量尺寸和非法范围 fail closed；
固定 257 columns x 32 layers 与独立 brute-force reference 全层逐字节相等；
重复物化确定性、调用方 buffer 地址不变；
General Mesh 和 InternalVoid/Shape/Base/Varnish 非能力边界显式拒绝或不在 DTO 中暴露；
Release MF-03B1 定向测试和 MF-03A 回归。
```

### MF-03B2

```text
P1 outer-varnish/upper-boundary 每层 Mask 与 retained 公式逐字节相等，upper last 全等；
P2 对完整 retained preliminary volume + unsupported loop 的 demand 与源层事件逐项相等；
覆盖 4/8 连通、0/N 膨胀、阈值等于边界、accepted/filtered/supported component；
证明此前 unsupported 不进入下一层 previous base；
乱序、提前 Finish、错误尺寸/别名/非二值输入可修正后重试且不产生半结果；
固定输出/内部 scratch 地址复用，逐层 hot path 无分配；
Release MF-03B2、MF-03B1、MF-03A、MF-02 与既有 support shape 回归。
```

## 6. 本轮开工门

```text
[x] 用户允许准备后继续开发并允许一次完成多个任务
[x] MF-02 共存、调用时机、写出、错误和清理条款已冻结
[x] MF-03 已拆为可独立验证的 MF-03A 与高风险 MF-03B
[x] 既存 scene adapter 平移断言失败已在修改前复现并单列
[x] RIPFLOW 并行修改已识别，禁止覆盖
[x] P0..P4 顺序、统计重放、baseProjection 与 outer-varnish 次序已冻结
[x] MF-03B1 DTO、开闭边界、错误边界和独立对照矩阵已冻结
[x] MF-03B2 P1/P2 状态机、previous-base 排除域、事件口径和 retained oracle 已冻结
[x] MF-03B3 InternalVoid/Shape/type/footprint/digest/report sink 顺序和非生产 Gate 已冻结
[x] MF-03B4 已拆为支撑归属 B4A 与材料归属 B4B，依赖和完成口径已冻结
[x] B4A identity、同层 digest checkpoint、Base 物理层映射、varnish priority 和统计归属已冻结
[x] B4A DTO/ownership、错误/取消/sink/半结果禁止和 retained oracle 矩阵已冻结
[x] B4B compose/Stage 15/closure/repair/totals 顺序与非生产边界已冻结
```

实施结果：MF-02、MF-03A、MF-03B1 与 MF-03B2 已按上述 Gate 完成并通过 Release 定向验证。MF-03B1
提供 move-only compact plan、双 Mask caller-owned 单层物化、完整字节区间别名拒绝和 257 x 32
retained oracle 全层零差异；核心源与测试均通过 `/W4 /WX`。MF-03B 的 P0..P4、统计重放顺序已
冻结。MF-03B2 已完成非生产实现与 Release 定向 Gate；完整 Shape/Base/Varnish/Material 组合 diff
fixture 和生产接线仍未达到 Gate。MF-03B3 已按冻结合同完成非生产实现和 Release 定向 Gate；
MF-03B4A/B 的准备 Gate 已完成；B4A 已实现 plan-bound verified replay、Base/outer-varnish 最终化、
compact connectivity sink 与 fail-closed 生命周期，并通过 10/10 独立用例及 Release 组合回归。
B4B 的依赖现已满足，但仍需作为下一张独立卡执行；本轮未开工 B4B。MF-04 和生产接线仍未授权，
B1..B4A 均未接生产路径。
