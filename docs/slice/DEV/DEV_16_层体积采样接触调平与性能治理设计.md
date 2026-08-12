# DEV_16 层体积采样、接触调平与性能治理设计

> 阶段：Stage 16
> 状态：ACTIVE / 16A-03 IMPLEMENTED
> 版本：v0.2
> 日期：2026-08-06
> 上游：`PRD_16` / `DOC_DECISION_16`

## 1. 设计原则

```text
策略显式化，不把新语义藏在既有 bool 中；
先 wrapper/provider，再迁移，最后才考虑删除历史物化路径；
采样、姿态、材料、支撑、输出和报告职责分离；
性能优化不得改变已选语义；
候选默认关闭，每一步都能回退 Legacy；
先内部核心合同，再决定是否扩展 Stage 14 公开 API/SPI。
```

## 2. 建议模块边界

### 2.1 采样策略

建议在 `src/slicer_core/geometry` 或 `src/slicer_core/raster` 边界建立 STL-only 策略 DTO：

```cpp
enum class LayerOccupancyMode {
    LegacyCenterSample,
    LayerSlabCoverage
};

enum class XyCoverageMode {
    PixelCenter,
    Supersample2x2
};

struct GeometryOccupancyPolicy {
    LayerOccupancyMode layerMode;
    XyCoverageMode xyMode;
    unsigned minimumCoveredSubsamples;
};
```

名称仅为设计草案，16A-02 必须先核对 Stage 14 后配置/Facade DTO 风格再冻结。

策略不应依赖 Qt、JSON、TIFF 或材料类型。

### 2.2 Occupancy Provider

建议在既有 `z_min/z_max` 列范围外建立 provider 包装：

```text
Geometry/Importer
  -> oriented mesh
  -> ColumnIntersectionField / BoundaryCoverageField
  -> LayerOccupancyProvider
  -> model occupancy mask/range
  -> support/material/scene compose
  -> RGBWSV writer
```

Provider 首版允许内部继续复用既有数组，不要在语义候选未收口时同时重写整个存储形式。

### 2.3 接触调平策略

姿态能力应属于 `model/geometry` 边界，不依赖材料、支撑或 Writer。建议将候选分为：

```text
ContactLevelingAnalyzer：只读测量和候选角；
ContactLevelingPolicy：off / diagnostic / apply_candidate；
ContactLevelingReport：输入姿态、边界带、接触面积、角度、约束和回退原因。
```

`apply_candidate` 只能在独立 Gate 后实现；首个代码任务只允许 diagnostic。

### 2.4 Telemetry

Telemetry 由 pipeline/report 组装，不允许 report writer 决定优化策略。Stage 14 后若 Facade/Worker 已有稳定 job telemetry，Stage 16 应复用它，不再叠加第二套不兼容计时字段。

## 3. 层体积算法合同

对层 `i`：

```text
slabLow  = i * h
slabHigh = (i + 1) * h
occupied = columnMaxZ > slabLow && columnMinZ < slabHigh
```

边界规则：

```text
使用半开区间；
几何区间必须与 layer slab 有正测度重叠；仅在 slabLow 处结束的区间不占据该层；
零厚度区间不产生占据；
容差来自统一几何容差合同，不按模型缩放；
相邻层的边界不重复、不遗漏；
最后一个空层是否写出由统一 layer-count 合同决定，不由 TIFF writer 自行修补。
```

### 3.1 16A-03 边界澄清

`columnMaxZ > slabLow` 是半开区间正测度相交的必要条件。早期 v0.1 草案中的
`>=` 会把恰好终止在下一层下边界的列重复计入该层，与 16A-01 已冻结的
`intervalHigh > slabLow && intervalLow < slabHigh` fixture 合同冲突。16A-03 因此采用
严格大于，并仅使用固定 `1e-9 mm` 的层边界吸附容差处理浮点表示误差；该容差不随模型缩放。

对多区间列或强倒扣网格，单一 `z_min/z_max` 可能过度填充。首版 Stage 16 只允许在既有 `relief_heightfield` 准入范围内使用新候选；推广到通用网格需新的 heightfield admission。

## 4. XY 超采样设计

### 4.1 首版

```text
内部/外部明确像素：快速通过；
边界像素：2x2 固定子样本；
阈值候选：>=2/4 和 >=1/4；
子样本位置、遍历顺序和平局处理固定；
不使用随机 jitter。
```

### 4.2 不选全局 4x4 的原因

```text
当前 2x2 已能大幅接近参考结果；
4x4 无条件采样使热路径理论采样数放大 16 倍；
Stage 16 还需同时完成 22 实例和内存预算收口；
除非 2x2 矩阵证明尺寸/拓扑 Gate 不能通过，不得先行进入 4x4。
```

### 4.3 薄特征保护

不在首版同时实现“50% 覆盖 + 连通域孤儿保活 + 最小线宽”多规则融合。若 S3 在合成薄壁 fixture 中丢失整个有效连通分量，先与 S4 作候选分层，再决定是否建立独立 `ThinFeaturePolicy`。

## 5. 接触姿态设计

### 5.1 准入

只对满足甲片形态判定且当前已完成 +Z/+Y 归一的模型生成候选。输入必须通过严格网格准入；`warn_and_attempt` 或已阻断模型不参与自动调平。

### 5.2 目标函数

候选不以“两个最低顶点高度相等”为唯一目标。建议按以下优先级评分：

```text
1. 保持正面 +Z、尖端 +Y 和准入；
2. 最大化前 1..2 个物理 layer slab 内的接触面积；
3. 减小两侧边界带下包络差；
4. 最小化绝对调平角；
5. 不增加超过冻结预算的高度、占地和支撑。
```

### 5.3 搜索

首版只绕甲片长轴作有界一维搜索。角度范围和步长不在本文档硬编码，由16B-01 矩阵基线决定。粗搜索后可在最优区间内做确定性精化，但不允许无界迭代。

## 6. 性能整并设计

### 6.1 基线先行

Stage 14 后重新采集：

```text
单模型：12B 三个历史模型 + Reality segment_101/105；
场景：1/11/12/22 实例；
策略：S0/S2/S3/S4，P0 和 diagnostic posture candidate；
路径：core-only / compose / write / preview / RIP strict；
运行：cold/warm，固定预热和重复次数；
身份：commit/build/compiler/Profile/asset/config hash。
```

### 6.2 优化顺序

```text
telemetry 缺口
-> 支撑重复扫描
-> bottom projection range/provider
-> compose 扫描/buffer
-> occupancy provider 分块与流式化
-> 缓存与平移复用
-> preview/I/O 解耦
-> 有限并行
```

不允许跳过单线程数据结构优化直接引入全局并行。

### 6.3 超采样性能控制

```text
边界自适应，避免内部像素重复 4 次判断；
按 tile/layer 处理，不常驻完整 2x 或 4x 三维体；
中间缓冲可复用，但不跨不同 strategy/config hash 复用；
每一个加速点先证明输出逐层/通道不变。
```

## 7. 报告与差异合同

建议新增独立 Stage 16 工程报告，不修改 `p0.rgbwsv.2`：

```text
geometrySampling:
  strategyId
  layerInterval
  xyCoverageMode
  subsampleThreshold
  boundaryPixels
  occupiedPixels

contactLeveling:
  mode
  candidateAngleDeg
  appliedAngleDeg
  firstSlabContactPixels
  secondSlabContactPixels
  sideEnvelopeDeltaMm
  rejectionReason

performance:
  timingsMs
  peakWorkingSetBytes
  cacheHits/cacheMisses
  producerCount/reuseCount
```

候选对比必须产出：

```text
per-layer R/G/B/W/S/V diff；
false-positive / false-negative occupancy；
connected component change；
first/last non-empty layer；
total model/support/union；
dimension bias 与物理单位；
RIP strict 结果。
```

## 8. 测试设计

### 8.1 合成 fixture

| Fixture | 目的 |
|---|---|
| 平底方块 | 锁定平面不应因新采样漂移 |
| 上升斜楔 | 区分中心、顶面和层体积 |
| 下降斜楔 | 验证上表面收缩不丢层 |
| 圆弧接触边 | 验证首层渐宽 |
| 亚像素细线/薄片 | 区分 S3 和 S4 |
| 多区间列 | 证明 relief-only admission 和 fail-closed |
| 对称/非对称甲片 | 验证姿态候选不过度调平 |

### 8.2 真实模型

```text
Reality segment_101..105；
13E 标准甲片矩阵；
12B 性能历史三模型；
Stage 15 按需补白 Profile 正向模型；
13B 1/11/12/22 实例场景。
```

### 8.3 验证层级

```text
L1 策略/数学/配置单测；
L2 mask/layer/channel diff 和 golden；
L3 Quick CI/full regression；
L4 Package + RIP strict；
L5 Qt/Facade/Worker 能力和取消；
L6 必要时的实物打样，不由软件验证伪造。
```

## 9. 回退

```text
配置缺省始终回到 LegacyCenterSample；
候选 mask 可在写包前与 Legacy 作 diff，超 Gate 则阻断而不静默降级；
姿态 apply 未授权前只输出 diagnostic report；
性能优化每张卡单独前后对比，不绑定大批重写；
公开 Facade/SPI 未冻结前只在内部 Effective Config 启用候选。
```

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-12 | v0.2 | 完成 16A-03 实施并澄清半开 layer slab 的正测度相交合同：上界必须严格大于 slabLow，零厚度区间不占据；候选仍只准入 `relief_heightfield`。 |
| 2026-08-06 | v0.1 | 首版设计草案。 |
