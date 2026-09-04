# REPORT_16C_06_MEMFLOW 主循环接线可行性探查（2026-09-04）

> 文档状态：**探查完成 / 首容器已接线 / 修正 §3 占用表：七容器中四个是条件分配**
> 版本：v1.3 ｜ 日期：2026-09-04
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

> **v1.2 修正。** 本表首版把七个容器都按「已分配」计入，得到 72.62 GB。
> 复核分配条件后发现**其中四个是条件分配**，在用户的实际阻塞配置里为 0。
> 下表已按条件重列，并给出该配置的真实合计。

| 容器 | 类型 | 分配条件 | 满配占用 | **用户 10um 场景** |
|---|---|---|---|---|
| `model_masks` | `vector<vector<uint8_t>>` | 无条件 | 10.37 GB | **10.37 GB** |
| `support_masks` | 同上 | 无条件[^1] | 10.37 GB | **10.37 GB** |
| `support_type_maps` | `vector<vector<SupportType>>`[^2] | 无条件[^1] | 10.37 GB | **10.37 GB** |
| `outer_surface_masks` | `vector<vector<uint8_t>>` | `surface_varnish.enabled`（默认 **false**） | 10.37 GB | 0 |
| `inner_surface_masks` | 同上 | 同上 | 10.37 GB | 0 |
| `outerVarnishMasks` | 同上 | `ComputeOuterVarnishDiscretization().enabled`，否则返回 `{}` | 10.37 GB | 0 |
| `upperBoundaryMasks` | 同上 | `boundaryInfo.includes_outer_varnish_shell`，否则返回 `{}` | 10.37 GB | 0 |
| **合计** | | | **72.62 GB** | **31.11 GB** |

[^1]: `generate_support_masks`（`slicer.cpp:2031`）的两次 `resize` 位于
      `if (!config.support.enabled) return result;` **之前**，故即使关闭支撑也照样分配。
[^2]: `enum class SupportType : std::uint8_t` —— 1 字节，故与 mask 同量级，非 4 倍。

场景：`a-2/0.2.obj` @10um，栅格 1500 x 5197 = 7,795,500 列 x 1,429 层，
配置见 `a2_probe.json`：开支撑（`bottom_projection`），**未配置 `surfaceVarnish`
或 `outerVarnish`**。

**31.11 GB 对上物理内存 31.6 GB** —— 这比首版的 72.62 GB 更能解释实测：
峰值 22~34 GB 恰好压在物理内存线上，故大量换页，耗时 20~31 分钟。
首版把差距归因于「容器并非全部同时存活」，真实原因更简单：**四个根本没分配。**

**这三个容器在三层有界窗口下只需 67 MB。** 差别纯粹在于「乘不乘 1429」，
这正是决策文「峰值从 O(w*h*layers) 降到 O(w*h*window)」的字面含义。

### 3.1 对 MF-03X1 收益的诚实界定

首容器（表面光油）接线**对用户的 10um 阻塞场景收益为零** —— 该配置下这两个容器
本来就是空的。它的价值在于：开启光油的工艺（多图层透明→光油等预设）不再有这个
天花板。但那些预设目前只用于 tm2-5 / gubao04 这类小幅面模型，尚未与大幅面叠加。

**推论：解除用户阻塞的全部收益都在 MF-03X2（支撑耦合簇）。** 见 §5.3 与 §5.4。

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
| `outer_surface_masks` / `inner_surface_masks` | `MaterializeSurfaceVarnishLayer`（本次新增，逐层物化） | **已接线 `0d2a1bf`** |
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

## 5.3 首容器接线结果与【逐容器替换建议的修订】

`outer_surface_masks` / `inner_surface_masks` 已于 `0d2a1bf` 完成接线，
两项零漂移逐字节通过（默认路径 94 层哈希与长期基线 `3cbfdec…` 一致；
gubao04 六材质 129 层 `8315b63c…` 与接线前一致）。

**但由此发现 §6 第 2 条「按容器逐个替换」只对第一个容器成立。**

选中的第一个容器之所以可独立替换，是因为它**逐层独立**——每层只读本层 model
mask，层间无依赖。剩余六个容器不具备该性质：它们全部通过同一个函数耦合。

```cpp
// slicer.cpp:2031 —— 整栈进、整栈出
SupportGenerationResult generate_support_masks(
    ..., const std::vector<std::vector<std::uint8_t>>& model_masks,
         const std::vector<std::vector<std::uint8_t>>& upper_boundary_masks, ...);
//   返回 result.support_masks / result.support_type_maps，均按 layer_count 预分配
```

`outer_varnish_masks` 亦然：它被 `BuildUpperSupportBoundaryMasks` 与
`ApplyOuterVarnishSupportPriority` 两个整栈函数消费，其产物
`upper_support_boundary_masks` 再整栈喂给 `generate_support_masks`。

### 5.3.1 剩余容器的下标形式实测

| 容器 | 引用处数 | `.at(layer_index)` | `.at(layer_index - 1)` | **`.at(target_layer)`** |
|---|---|---|---|---|
| `model_masks` | 41 | 16 | 1 | **1** |
| `support_masks` | 19 | 7 | 1 | **1** |
| `support_type_maps` | 12 | 6 | 0 | **1** |

三者各有**恰好一处** `.at(target_layer)`，且都位于同一个悬空岛向下回写循环内
（§4.1）。这意味着它们**必须同时**转换——只换其中之一，那一处回写就会失去
其余两者的整栈视图。

### 5.3.2 修订后的剩余范围

```text
原建议   七个容器逐个替换、每次一个并验证零漂移
修订     第一个容器（表面光油）已按此完成 —— 逐层独立，无耦合
         剩余六个是【一个耦合簇】，替换单元不是「容器」而是
         「把 generate_support_masks 整体改走 B1/B2/B3/B4A 的有界路径」
```

故剩余工作的粒度远大于「再换一个容器」，应作为独立任务卡估算，
不宜按首容器的工作量线性外推。

### 5.3.3 已就位的共同前置

本次为满足 G2 而下沉的 `geometry/SliceGridSpec.h`（`GridSpec` + `mask_index`）
是**所有** mask 构建函数的共同参数类型。它已提为共享头，后续任何一个 mask 构建
函数下沉都不再需要先解决这两个符号的共享问题。

---

## 5.4 按配置收缩范围：用户阻塞档不含任何无界随机访问

§5.3 说剩余六个容器是一个耦合簇、须整体改走有界路径。这对**全模式**成立，
但用户的阻塞配置只用到其中一小部分。逐项核对 `a2_probe.json` 的实际激活项：

| 耦合难点 | 守卫条件 | 用户配置（`mode = bottom_projection`） |
|---|---|---|
| `.at(target_layer)` 悬空岛向下回写 | `placement_policy.unsupported_only_enabled` | **false** |
| 支撑形状优化（整栈进出） | `support_shape_policy.enabled` ← `config.h:312` `shape_enabled{false}` | **false** |
| `outerVarnishMasks` / `upperBoundaryMasks` | 光油离散化 / `includes_outer_varnish_shell` | **false** |

`support_mode_includes_unsupported()` 只对 `unsupported_only` 与
`bottom_projection_plus_unsupported` 返回 true，故纯 `bottom_projection`
**不进入岛发现分支** —— §4.1 那个「当前层之下全部层」的随机访问不执行。

该配置下剩余的向下遍历只有 bottom-projection 自己的
`for (layer_index in [0, lower_layer))`：每列填到该列最低模型层，
是 `support_source_layers` / `column_ranges` 的纯函数，
**而主循环已经在算这两个归约。**

### 5.4.1 由此得到的两点结论

**一、§6 第 1 条的最大风险在这一档不适用。** B2 时序等价之所以是最大风险，
是因为悬空岛「先全层 preliminary、再顺序发现并回写」的时序必须逐字节等价。
该分支在此档不执行，故不需要 B2/B3。所需能力只有 MF-03B1（Range-derived
Support Demand）与 MF-03A（LayerOccupancyProvider），两者均已 COMPLETE。

**二、解除阻塞的改动远小于「全模式接线」。** 据此把 MF-03X2 拆为
MF-03X2a（本档，解除阻塞关键路径）与 MF-03X2b（岛发现 / 形状 / 光油全模式）。

**必要守卫：** 非 `bottom_projection` 配置必须仍走 retained 路径，
按 mode fail-safe 分流；不得把只在本档验证过的有界路径应用到其他档。

---

## 6. 风险与未决

1. **B2 时序等价是最大风险。** 悬空岛的「先全层 preliminary、再顺序发现并回写」时序
   必须逐字节等价，否则支撑连通性会变。B2 已有 retained oracle，但**接线时的调用顺序**
   仍需逐层 digest 比对。
2. **接线范围大。** 七个容器分布在主循环各处，`model_masks` 在 `slicer.cpp` 有 5 种下标形式。
   ~~建议按容器逐个替换~~ —— **见 §5.3 修订**：只有第一个容器逐层独立、可独立替换；
   剩余六个通过 `generate_support_masks` 耦合成一个簇，须整体改走有界路径。
3. ~~**`slicer.cpp` 属 G2 只减不增名单。**~~ **已解除。** 首容器接线净增 127 行，
   同步下沉表面光油几何簇后净减 137 行，`ValidateSourceSizeGuard --base-ref HEAD`
   判定 PASS，**未新增任何豁免登记**（AGENTS.md 已记 12 处门禁 ERROR，不再增加）。
4. **多实例场景未覆盖。** 用户的双模型场景（`0.2+0.3`）需 MF-05 的 Barrier；
   本报告只覆盖单实例。

---

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-04 | v1.3 | 新增 §5.4：按配置收缩范围。逐项核对用户 `a2_probe.json` 的实际激活项，确认 `mode = bottom_projection` 下三个耦合难点全部不激活 —— 岛发现（`unsupported_only_enabled` false）、形状优化（`shape_enabled` 默认 false）、两种光油（未配置）。故该档**不含任何无界随机访问**，剩余向下遍历只有按列的 bottom-projection，是主循环已在算的 `column_ranges` 的纯函数。两点结论：§6 第 1 条「B2 时序等价是最大风险」在本档不适用（不需要 B2/B3，只需已 COMPLETE 的 B1 + A）；解除阻塞的改动远小于全模式接线。据此把 MF-03X2 拆为 X2a（本档，关键路径）与 X2b（全模式）。 |
| 2026-09-04 | v1.2 | **修正 §3 占用表。** 首版把七个容器都按已分配计入得 72.62 GB；复核分配条件后确认四个是条件分配（`surface_varnish.enabled` 默认 false；`outerVarnishMasks` 与 `upperBoundaryMasks` 无光油时返回 `{}`），在用户实际阻塞配置下为 0。该配置真实合计 **31.11 GB**，对上物理内存 31.6 GB，比 72.62 GB 更能解释实测峰值 22~34 GB 与换页。同时确认 `SupportType` 是 `uint8_t`（非 4 字节）、`support_masks`/`support_type_maps` 的 resize 在 `support.enabled` 检查之前故无条件分配。据此新增 §3.1：MF-03X1 对 10um 阻塞场景收益为零，解除阻塞的全部收益在 MF-03X2。 |
| 2026-09-04 | v1.1 | 首容器（`outer_surface_masks` / `inner_surface_masks`）接线完成并零漂移通过，提交 `0d2a1bf`。据此修订 §6 第 2 条：「按容器逐个替换」只对第一个容器成立——它逐层独立；剩余六个通过 `generate_support_masks`（整栈进整栈出）耦合成一个簇，且 `model_masks`/`support_masks`/`support_type_maps` 各有恰好一处 `.at(target_layer)` 且同处一个回写循环，必须同时转换。风险 3（G2 门禁）解除：同步下沉使 slicer.cpp 净减 137 行，未新增豁免。新增 §5.3 与共同前置 `geometry/SliceGridSpec.h` 说明。 |
| 2026-09-04 | v1.0 | 首版。推翻两个认知：`slicer_cli` 的 TIFF 输出早已逐层流式（故 MF-04 的内存收益不成立），真正瓶颈为七个 mask 整栈驻留 72.62 GB。量化跨层访问模式并确认 `target_layer` 的「当前层之下全部层」随机访问是最难有界化的一环，同时确认 MF-03B2 已用 compact 事件在设计层解决。提出 MF-03X「主循环有界接线」并列出六项替代能力均已 COMPLETE，MF-04 范围重定义为原子发布语义。 |
