# REPORT_16C_06_MEMFLOW 主循环接线可行性探查（2026-09-04）

> 文档状态：**探查完成 / 修订 MF-04 范围判断**
> 版本：v1.0 ｜ 日期：2026-09-04
> 定位：为解除真实生产阻塞（10um 层厚 111.4 亿 pixel-layer）而探查有界能力接入主循环的可行性。
> 上游：`REPORT_16C_06_MEMFLOW_替代基线资产与调查结论_2026_09_03.md`（阻塞场景实测）
> 授权：用户 2026-09-04 授予本专项最高决策权

---

## 1. 结论摘要

**本次探查推翻两个此前的认知，并据此修订实施范围。**

| # | 此前认知 | 探查结论 |
|---|---|---|
| 1 | 「Writer 累积全部层后逐层写 TIFF」是内存瓶颈 | **错。`slicer_cli` 路径早已逐层写 TIFF**，层数据写完即出作用域 |
| 2 | MF-04（单实例流式 Package）是解除阻塞的关键一步 | **收益远小于预期**。真正的瓶颈是七个 mask 整栈驻留 |

**修订后的判断：缺的不是新能力，而是接线。** 七个 mask 的有界替代品（MF-03A/B1/B2/B3/B4A）
全部 COMPLETE 且各有 retained oracle 验证，但**没有一处接进 `slicer.cpp` 主循环**。

---

## 2. TIFF 输出已是流式（推翻认知 1）

`slicer.cpp:5364` 位于层循环【内部】：

```cpp
if (options.write_tiff_layers) {
    ...
    WriteRgbwsvProductionLayerTiff(
        package_dir / relative_path, productionStorage,
        RgbwsvProductionLayerView{grid.width_px, grid.height_px, layer});
}
```

`layer` 是本层局部变量，写完即释放。`RgbwsvProductionLayerView` 只持 `std::span`，不拷贝。

决策文称「`RgbwsvPackageWriter` 接收完整 `vector<RgbwsvProductionLayer>` 后逐层写 TIFF」——
该描述对 `WriteRgbwsvProductionPackage`（整包发布入口）成立，但**`slicer_cli` 的生产路径
走的是 `WriteRgbwsvProductionLayerTiff` 逐层入口**，不经过整包累积。

**故 MF-04「把完成层交给 sink 而不累积」对 CLI 路径的收益不成立**——它已经不累积了。
MF-04 的价值应重新界定为「原子发布与 staging 语义」，而非内存收益。

---

## 3. 真正的瓶颈：七个 mask 整栈驻留

`slicer.cpp` 中按层数增长的容器：

| 容器 | 声明位置 | 类型 | 10um 场景占用 |
|---|---|---|---|
| `model_masks` | 241 | `vector<vector<uint8_t>>` | 10.37 GB |
| `support_masks` | 269 | 同上 | 10.37 GB |
| `support_type_maps` | 270 | `vector<vector<SupportType>>` | 10.37 GB |
| `outer_surface_masks` | 294 | `vector<vector<uint8_t>>` | 10.37 GB |
| `inner_surface_masks` | 295 | 同上 | 10.37 GB |
| `outerVarnishMasks` | 889（返回值） | 同上 | 10.37 GB |
| `upperBoundaryMasks` | 946（返回值） | 同上 | 10.37 GB |
| **合计** | | | **72.62 GB** |

场景：`a-2/0.2.obj` @10um，栅格 1500 x 5197 = 7,795,500 列 x 1,429 层。

实测峰值 22~34 GB 低于 72.62 GB，因这些容器并非全部同时存活，且系统在换页——
这与「耗时 20~31 分钟」互为印证。

**同样这七个 mask 在三层有界窗口下只需 156 MB。** 差别纯粹在于「乘不乘 1429」，
这正是决策文「峰值从 O(w*h*layers) 降到 O(w*h*window)」的字面含义。

---

## 4. 跨层访问模式（决定窗口半径）

| 访问形式 | 出现的容器 | 所需窗口 |
|---|---|---|
| `.at(layer_index)` | 全部七个 | 当前层 |
| `.at(layer_index - 1)` | `model_masks`、`support_masks` | 半径 1 |
| **`.at(target_layer)`** | `model_masks`、`support_masks`、`support_type_maps` | **当前层之下【全部】层** |

### 4.1 `target_layer` 是最难有界化的一环

`slicer.cpp:2280`：

```cpp
for (int target_layer{0}; target_layer < layer_index; ++target_layer) {
    for (const int pixel : island.pixels) {
        if (model_masks.at(target_layer).at(index) == 0) { ... }
```

悬空岛发现后**向其下方的所有层回写支撑**。第 1400 层发现的岛要回写第 0..1399 层，
**访问范围不是固定窗口**，任何固定半径都表达不了。

### 4.2 MF-03B2 已解决该问题

`BoundedSupportDiscovery` 的方案是**不保留 mask 栈，只产出逐层 compact 事件**：

```cpp
struct BoundedUnsupportedLayerEvent {
    int layerIndex; int islandCount; int islandPixels;
    int unsupportedPixels; int filteredIslandCount; int filteredIslandPixels;
};
```

把「向下回写」转化为「记录事件、由 B3 重放阶段按事件重建」。设计文原文要求 B2 的
previous support「仅重放 Bottom/Full/Upper，不含本遍此前发现的 Unsupported」，
以此精确保持 retained 的「先全层 preliminary support、再顺序发现岛并向低层回写」时序，
同时不保留完整 support volume。

**故该障碍在设计层已解决，B2/B3 已 COMPLETE 并通过 retained oracle 验证。**

---

## 5. 修订后的实施路径

```text
原定   MF-03B4B -> MF-04 -> MF-07
修订   MF-03B4B（接口已接线）
       -> MF-03X 主循环接线（新增，本报告提出）
       -> MF-07 自适应路由
       MF-04 降级：范围重定义为原子发布/staging 语义，不再作为内存收益的关键步
```

### 5.1 新增 MF-03X「主循环有界接线」的范围

把七个 mask 的整栈容器替换为已建成的有界物化：

| 整栈容器 | 有界替代 | 状态 |
|---|---|---|
| `model_masks` | `LayerOccupancyProvider`（MF-03A） | COMPLETE |
| `support_masks` / `support_type_maps` | `BoundedSupportDemand`（B1）+ `FinalReplay`（B4A） | COMPLETE |
| 悬空岛向下回写 | `BoundedSupportDiscovery`（B2）compact 事件 | COMPLETE |
| 形状/footprint | `BoundedSupportShapeScan`（B3） | COMPLETE |
| `outer_surface_masks` / `inner_surface_masks` | B1 的 upper-boundary 抑制 + P1 scanner scratch | COMPLETE |
| material/closure | `RetainedMaterialLayerComposer`（B4B） | 接口已接线 |

**所有替代能力均已存在。MF-03X 是接线工作，不是新能力开发。**

### 5.2 验收判据（沿用既有手段）

```text
零漂移   默认路径 94 层拼接哈希 3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
         gubao04 六材质 129 层 TIFF 逐字节 + 逐材质 owner
         tm2-5 / yz 全通道
内存     a-2/0.2.obj @10um 的 peakWorkingSetBytes 由 22~34 GB 降至百 MB 级
耗时     同场景 totalMs 由 20~31 分钟显著下降（换页消失）
资产     model/stl/suoguo-baseline/ 八个 STL 的既有基线（2,658~3,316 MB）不得上升
```

---

## 6. 风险与未决

1. **B2 时序等价是最大风险。** 悬空岛的「先全层 preliminary、再顺序发现并回写」时序
   必须逐字节等价，否则支撑连通性会变。B2 已有 retained oracle，但**接线时的调用顺序**
   仍需逐层 digest 比对。
2. **接线范围大。** 七个容器分布在主循环各处，`model_masks` 在 `slicer.cpp` 有 5 种下标形式。
   建议按容器逐个替换、每次一个容器并验证零漂移，而非一次全换。
3. **`slicer.cpp` 属 G2 只减不增名单。** 接线会净增行数，需按现状登记豁免或在接线中同步下沉。
4. **多实例场景未覆盖。** 用户的双模型场景（`0.2+0.3`）需 MF-05 的 Barrier；
   本报告只覆盖单实例。

---

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-04 | v1.0 | 首版。推翻两个认知：`slicer_cli` 的 TIFF 输出早已逐层流式（故 MF-04 的内存收益不成立），真正瓶颈为七个 mask 整栈驻留 72.62 GB。量化跨层访问模式并确认 `target_layer` 的「当前层之下全部层」随机访问是最难有界化的一环，同时确认 MF-03B2 已用 compact 事件在设计层解决。提出 MF-03X「主循环有界接线」并列出六项替代能力均已 COMPLETE，MF-04 范围重定义为原子发布语义。 |
