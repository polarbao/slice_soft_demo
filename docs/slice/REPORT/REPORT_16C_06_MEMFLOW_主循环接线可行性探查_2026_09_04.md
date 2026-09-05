# REPORT_16C_06_MEMFLOW 主循环接线可行性探查（2026-09-04）

> 文档状态：**MF-03X2a 已接线（单模型 CLI 峰值 22~34 GB -> 1.14 GiB）／双模型另有更大根因，见 §5.6**
> 版本：v1.11 ｜ 日期：2026-09-05
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

## 5.5 MF-03X2a 实施与验证

### 5.5.1 三个整栈的有界替代

| 整栈容器 | 有界替代 | 依据 |
|---|---|---|
| `model_masks` | `MaterializeReliefModelLayer(spans, L, buf)` | relief legacy 采样按列写闭区间 `[startLayer, endLayer]`，故整栈是列区间的**纯函数** |
| `support_masks` | `MaterializeBottomProjectionSupportLayer(...)` | 谓词 `L < support_source_layers[col] && model[L][col] == 0` 只依赖本层 mask 与按列标量 |
| `support_type_maps` | 同上（`SupportType::BottomProjection`） | 同上；类型仲裁沿用 `set_support_pixel` 的优先级判据 |
| 内部空腔补写 | `AddInternalVoidSupportForLayer(...)`（原实现下沉后复用） | 该函数逐层独立：只读本层 model mask、只改本层 support |
| 支撑统计 | `AccumulateSupportLayerStats(...)` | 从 `CalculateSupportGenerationStats` 原样抽出的逐层主体；retained 由包装函数逐层调用，累加顺序不变 |

按列归约无需改造：`relief_heightfield` 模式下 `support_source_layers` 与
`column_ranges` 分别由 `compute_relief_lower_layers` / `compute_relief_column_ranges`
从 `relief_columns` 求出，**本来就不读 `model_masks` 整栈**。

采样阶段也一并省掉分配：`sample_relief_heightfield_masks` 新增
`materializeModelMaskStack` 参数，为 false 时不 resize、不填充，只写列区间。
这一步是关键 —— 10.37 GB 的分配发生在采样内，事后 `clear()` 救不回峰值。

### 5.5.2 准入判定与守卫

`EvaluateBoundedReliefSupportPath(config)` 只看配置，故可在采样前求出。
任何一项不满足即退回 retained，并记下 `reason`：

```text
slicing_mode_not_relief_heightfield          非 relief 模式
geometry_sampling_strategy_not_legacy_...    supersample / layer_slab 候选另走 BuildLayerOccupancy
support_placement_not_lower                  显式 placement 非 lower
support_mode_without_bottom_projection       模式不含 bottom projection
support_mode_includes_unsupported            含悬空岛发现（需 B2）
support_shape_enabled                        形状优化整栈进出
support_base_projection_enabled              base projection 整栈进出
outer_varnish_enabled                        会带出两个整栈光油容器
```

外光油这一项是**保守判据**：实际用到的两个开关都严格更窄 ——
`ComputeOuterVarnishDiscretization` 要求 `enabled && thickness_mm > 0`，
`ResolveUpperSupportBoundaryInfo` 要求 `enabled && thickness_mm > 0` 且
`upper.outside == "outer_varnish_shell"`。故 `enabled` 为假时两者必假，无缺口。

### 5.5.3 一次真实漏项：内部空腔

首次接线后 gubao04 逐字节一致，但 **r01 的 20 个层各差 1 个字节**。逐字段比报告
定位到：retained 把 79,086 个支撑像素标为 `InternalVoid`，而我的有界路径全标成
`bottom_projection`，且少了 1 个像素。

根因是我枚举 `generate_support_masks` 的分支时**只看了四个 `placement_policy`
条件分支，漏掉其后一个无条件的逐层循环** —— `AddInternalVoidSupportForLayer`，
而 `internal_void.enabled` **默认为 true**。

补救后重列该函数的全部六个顶层块并逐条对照，才确认无其他遗漏：

```text
1  !config.support.enabled 提前返回        -> supportEnabled 形参
2  placement_policy.lower_enabled          -> MaterializeBottomProjectionSupportLayer
3  ..full_vertical_projection_enabled      -> 准入排除
4  ..upper_enabled                         -> 准入排除
5  ..unsupported_only_enabled              -> 准入排除
6  内部空腔逐层循环（无条件，默认开启）    -> 复用原实现，逐层调用
7  layers_with_islands 累加                -> 无岛时恒为 0
```

**教训：按「条件分支」枚举会漏掉无条件语句。** 该函数的分支清单应按顶层语句
逐条核对，而非只找 `if`。

### 5.5.4 验证结果

四判据逐字节 + 报告全等（`package_report.json` 仅差 configPath / packageDir，
即我用的不同输入输出路径，非漂移）：

```text
默认路径 r01    94 层  3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
                       与长期基线一致
gubao04         129 层 8315b63c42e3f6a90faef6aa693f4d9f3443e95af1245268b86d8cf812a2aee1
```

峰值内存（同一 build，接线前后同配置对照）：

四判据逐字节全等（`x2a_tm25` / `x2a_yz` 同时与主仓 P1 基线相同，
即 memflow @ X2a 与 `product/packaged-slicer` 对这两项资产输出等价）：

```text
默认路径 r01     94 层  3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
gubao04         129 层  8315b63c42e3f6a90faef6aa693f4d9f3443e95af1245268b86d8cf812a2aee1
tm2-5           124 层  f0d8e429afd3e385f6a6742945a8c6fe…
yz 内嵌         474 层  6bfc4959681111d23ae1ea4e460681f6…
```

单元测试 `stage16c06_bounded_relief_support_plan_unit_tests` PASS，
含准入判定十项拒绝理由、闭区间边界、半开支撑区间、缓冲复用与尺寸 fail-closed，
以及对 retained 参考实现（整栈物化 + `lower_enabled` 双层循环）的
40 组随机夹具暴力等价比对。

### 5.5.5 解除用户阻塞的实测

场景：`model/obj/reality/finger_suoguo/a-2/0.2.obj` @ **10um**，
栅格 1500 x 5197 = 7,795,500 列 x 1,429 层 = 111.4 亿 pixel-layer。

```text
retained   峰值 22~34 GB（压在物理内存 31.6 GB 线上，大量换页）
           进入层处理前耗时 277,200 ms
           实跑到 450/1429 层用 593,671 ms 后被中止，从未跑完
bounded    峰值 peakWorkingSetBytes = 1,225,912,320  ->  1.14 GiB
           进入层处理前耗时 642 ms
           totalMs = 1,135,091  ->  18.9 分钟，完整跑完 1,429 层
```

| 指标 | retained | bounded | 变化 |
|---|---|---|---|
| 峰值内存 | 22~34 GB | **1.14 GiB** | **约 -95%（20~30 倍）** |
| 层处理前序幕 | 277,200 ms | 642 ms | **-99.8%（432 倍）** |
| 总耗时 | 从未跑完（用户实测约 20 分钟） | 1,135,091 ms（18.9 分钟） | 略优 |

**零漂移（真实资产、真实层厚）：** retained 那次被中止的运行留下了前 457 层输出，
与 bounded 的同层输出**457/457 层逐字节一致**。

### 5.5.6 对时间与内存的诚实界定

**内存是本次的实质改变，时间只是略优。** 原因是本改动把内部空腔洪泛与支撑统计
从「序幕一次性全层」搬到了「主循环逐层」—— 总工作量不变，只是不再需要整栈驻留。
时间的收益来自两处：省掉的 277 s 序幕，以及换页消失。同窗口逐层速率
（第 225~375 层）retained 878 ms/层 vs bounded 913 ms/层，基本持平。

若还要压时间，下一步应优化逐层热循环本身，而非继续搬动：
`AddInternalVoidSupportForLayer` 每层重新分配 `externalEmpty`（7.8 MB）与
`stack.reserve(pixelCount)`（31 MB），1,429 层累计约 5.6 万 MB 的分配/释放；
三个物化循环与统计循环合计每层约 3,100 万次 `.at()` 边界检查。
两者都可在不改语义的前提下收敛（缓冲提为调用方持有、热循环改索引访问）。
**但这属独立的性能任务，不应与本次的语义等价改动混在一起。**

---

## 5.6 双模型场景的根因【不是】三个 mask 栈

X2a 完成后回头验证用户的第二个诉求（`0.2.obj + 0.3.obj` 一起切片内存不足），
读多模型路径的代码得到一个必须纠正的结论：**该失败与三个 mask 栈无关。**

### 5.6.1 场景路径把全部实例的全部层都留在内存里

`MultiModelProductionService.cpp:813` 起：

```cpp
std::vector<SceneInstanceRaster> rasters;
rasters.reserve(scene.instances.size());
for (const SceneModelInstance& item : scene.instances) { ... rasters.push_back(...); }
...
composeRequest.instances = std::move(rasters);   // 全部切完才合成
```

而单个实例的 raster 持有**全部层**（`SceneRasterTypes.h:107`）：

```cpp
struct SceneInstanceRaster { ...; std::vector<SceneInstanceRasterLayer> layers; };
struct SceneInstanceRasterLayer {
    RgbwsvProductionLayer output;                  // channels = w*h*6
    std::vector<std::uint8_t> modelownership;      // w*h
    std::vector<std::uint8_t> modelvarnishownership;
    std::vector<std::uint8_t> outervarnishownership;
    std::vector<std::uint8_t> supportownership;
};
```

`LegacySceneLayerAdapter.cpp:230` 的 `ownedlayercallback` 对**每一层**
`push_back` 且不释放任何一层。四张归属 mask 必然填充 —— 设置了
`ownedlayercallback` 就使 `collectMaterialClosureSemantic` 为真。

### 5.6.2 量级对比

```text
每列每层字节数
  三个 mask 栈（X2a 已解决）   3 B      model + support + support_type
  场景实例栅格（未解决）      10 B      6 通道 + 4 张归属 mask，【每实例】

用户 10um 场景（1418 x 5197 = 7,368,146 列 x 1,429 层）
  三个 mask 栈                31.11 GiB   -> 已降至约 21 MB（三个单层缓冲）
  场景实例栅格               103.7 GiB /实例   -> 未动
```

**故场景路径比本次修好的部分还大一个量级，且按实例线性叠加。**
双模型即 207 GB —— 这才是用户「内存不够、切片失败」的直接原因。

### 5.6.3 这正是 MF-02 合同要解决而 sink 没做到的事

`ownedlayercallback` 就是 MF-02「Owned Layer Producer/Sink」合同的落点，
合同的用意是让 sink **消费即释放**。`LegacySceneLayerAdapter` 却把每层都囤起来，
使合同的收益归零。

合成本身**不需要**全部层同时在内存：合成第 L 层只要各实例的第 L 层。
障碍在于实例是**按实例顺序**切的（instance-major），而合成需要 layer-major。
消除该错位有两条路：

```text
A  MF-05 Layer Barrier   各实例按 global layer 交错推进，同层到齐即合成并释放
                         峰值 O(实例数 x 列数)，与层数无关
B  逐实例落盘再流式合成   实例仍顺序切，但每层写入 staging 后释放，
                         合成阶段按层读回。峰值同样与层数无关，代价是 I/O
```

A 是决策文原定方向且能同时解掉 MF-04 的 staging 语义；B 改动小但引入磁盘往返。
**建议 A**，并按 X2a 的做法先只覆盖用户档、以 fail-safe 准入分流。

### 5.6.4 对上游判断的修正

§2 曾结论「`slicer_cli` 的 TIFF 输出早已逐层流式，故 MF-04 的内存收益不成立」。
该结论对 **CLI 单模型路径**成立，但**不适用于场景路径** —— 后者经
`LegacySceneLayerAdapter` 全量囤积，MF-04/MF-05 在这条路径上的内存收益是实打实的。
故 §5.1 表中「MF-04 降级」只应理解为「对 CLI 路径降级」。

---

## 5.7 切片耗时优化（用户 2026-09-05 提出）

X2a 解决了内存但**时间只是略优**（18.9 分钟）。用户随即要求处理耗时。

### 5.7.1 先测再改：热点分布

在层循环内插临时子计时器，用同一模型 @0.1mm（143 层，同样 7,369,346 列）取分布：

| 段 | 总 ms | 每层 ms | 占比 |
|---|---|---|---|
| `compose_layer` | 56,454 | 394.8 | 55.9% |
| `AddInternalVoidSupportForLayer` | 32,810 | 229.4 | 32.5% |
| model / support 物化 | 3,929 | 27.5 | 3.9% |
| 支撑统计 | 2,519 | 17.6 | 2.5% |
| **layerCompute 合计** | **101,014** | **706.4** | 100% |

### 5.7.2 根因：为稀疏模型做满幅面扫描

`relief_report` 实测：**186,103 / 7,369,346 列有模型（2.53%）**，
其余 **97.47% 的列在任何一层都既无模型也无支撑**。而每层的每一遍都扫满幅面。

包围盒剔除在此**无效** —— 模型 bbox（60 x 220 mm）就是整块幅面，
它只是在幅面内很稀疏。必须按【列集合】而非包围盒剪枝。

### 5.7.3 精确等价的剪枝判据

`compose_layer` 的分支链是

```cpp
if (model_mask[i]) { … } else if (outer_varnish_mask[i]) { … } else if (support_mask[i]) { … }
// 【没有末尾 else】
```

故三者皆零的列不写任何字节，保持预填的 `background.value`。
只要活动列表覆盖三者的并集，跳过其余列就是**精确等价**，不是近似。

但活动列表**不能**简单取「有模型的列」：`AddInternalVoidSupportForLayer` 会给
面内被模型围住的空腔写支撑，而环形件孔心那类列在**所有层**都没有模型。
—— 这正是首版只取有模型列时 r01 再次漂移的原因。

`BuildBoundedActiveColumns` 的判据：无模型的列若能经由其他无模型列连到幅面边界，
则它在任何一层都是外部空白（那些列在该层同样为空，洪泛必然经它们抵达），可安全排除；
其余全部保留。用 4 邻接求连通是保守方向 —— 少判外部只会让活动表偏大，不会漏列。

### 5.7.4 三处改动与效果

```text
1  compose_layer 增 active_columns 参数，只遍历活动列
2  AddInternalVoidSupportForLayer 增 active_columns：表外列直接标为外部，
   洪泛种子改取「表内、本层为空、紧邻表外」的列，分量扫描也只走表内
3  compose 输出缓冲、内部空腔 externalEmpty / visited 均改为跨层复用，
   每层只重置表内列 —— 原实现每层新建两个 7.37 MB 缓冲，实测【这才是】
   该函数的主要开销：只做剪枝而不复用缓冲，143 ms/层仅降到 143 ms/层
```

第 3 点是本轮最反直觉的一处：剪枝把 BFS 的工作量砍掉 97%，耗时却几乎没动，
直到把每层的整幅面分配去掉才真正下来。**按幅面计的固定开销与按占用计的
工作量是两笔账**，只算后者会得出错误结论。

```text
a-2/0.2.obj @10um（1,429 层，单模型 CLI）
  totalMs          1,135,091 -> 639,903   (18.9 分钟 -> 10.7 分钟，-43.6%)
  layerComputeMs   1,081,463 ->  594,981   (-45.0%)
  peakWorkingSet   1.14 GiB  ->    1.18 GiB（基本不变，本轮只动耗时）
```

三判据逐字节全等（r01 / gubao04 / a2@0.1mm 自身前后对照）。

> **测量口径的教训。** 上面只引 10um 那次长跑，因为 143 层的 0.1mm 对照链**不可信**：
> 同一二进制连续三次跑出 45,402 / 49,747 / 66,539 ms，**波动 47%**（机器有并发争用）。
> 我曾据单次运行判定「物化剪枝造成 15% 回退」，那个结论是噪声。
> 短跑必须多次取最小值；1,429 层的长跑因层数多而自平均，且进程中 366~390 ms/层
> 持续稳定，故可单次引用。

### 5.7.5 仍未处理的按幅面开销

同类问题还剩三处，都是「每层新建整幅面缓冲」：

```text
compose_layer 的输出缓冲     每层新建 w*h*6 = 44.2 MB，是当前最大单项
model / support 单层物化     每层 std::fill 整幅面后只写活动列
analyze_support_connectivity 每层新建 w*h 的 visited（已下沉为独立 TU 备改）
```

三者合计仍占每层约 300 ms。改法与第 3 点相同：缓冲跨层复用、只重置活动列。
**未做的原因是本轮先交付已验证的部分**，不是判断它们不值得做。

---

## 5.8 修正 §5.6：场景路径是【三份】整栈，且合成侧受校验契约约束

§5.6 只算了每实例栅格，**低估了**。实施 MF-05 前逐段读代码，完整的驻留链是：

```text
1  N 份 每实例 SceneInstanceRaster      10 B/列/层  x N
   （6 通道 + 4 张归属 mask，LegacySceneLayerAdapter 逐层 push_back 不释放）

2  1 份 SceneLayerComposeResult.layers   6 B/列/层  （global 幅面）
   MultiModelProductionService.cpp:975
     composeRequest.instances = std::move(rasters);
     composition = ComposeAdmittedSceneRastersValidated(std::move(composeRequest));
   两者在合成期间【同时驻留】

用户 0.2+0.3 @10um
  每实例 103.7 GiB x 2 = 207.4 GiB
  合成结果            ≈  62.2 GiB
  合计               ≈ 269.6 GiB   （物理内存 31.6 GB）
```

### 5.8.1 「Consuming」不等于边合成边释放

`ComposeSceneLayersConsuming` 实为 `ComposeSingleInstanceConsuming` 的转发，
仅覆盖**单实例快路径**；多实例仍走 `ComposeSceneLayersBorrowed`，
借用而不释放。故多实例场景下第 1 份不会随合成推进而缩小。

### 5.8.2 合成侧流式化受【类型不变量】阻挡

```cpp
// SceneLayerComposer.cpp:1427  ValidatedSceneLayerComposeResult 构造
m_validated(
    m_result.available && m_result.status == "ready_for_writer"
    && !m_result.error.has_value() && m_result.grid.IsValid()
    && m_result.layers.size() == static_cast<std::size_t>(m_result.grid.layercount)
    && m_result.layerstatistics.size() == m_result.layers.size())
```

「**全部层同时在场**」是写进该类型的不变量，不是实现细节 —— 它存在的目的正是
让下游 report/package 阶段复用合成器的闭合证据，而不必重扫每个 RGBWSV 字节
（见 `SceneLayerComposer.h` 的类注释）。

**故合成侧流式化必须先把该证据契约由「一次性全量」改为「逐层累积」**，
否则要么破坏不变量、要么让下游退回全量重扫（那会把省下的内存换成一次全量扫描）。

### 5.8.3 对 MF-05 范围与工期的修正

```text
原估   实例侧加层屏障即可，峰值 -> 约 132 MB
修正   必须三处一起流式，且第 3 处要改类型不变量：
       a 实例侧   ownedlayercallback 存单层槽 + 层屏障      （步骤 1 已完成）
       b 合成侧   逐层合成、合成即写出、不累积 layers
       c 证据侧   ValidatedSceneLayerComposeResult 的闭合证据改为逐层累积，
                  使「已验证」不再等价于「全部层在内存里」
```

**这是一次触及既有架构不变量的改动，不是接线量级。** 分步与验收见任务清单
MF-05；在其落地前，用户可用「两个模型分两次作业」规避（单模型路径已达
1.18 GiB / 10.7 分钟）。

### 5.8.4 为什么不先做半截

只修 a（实例侧）后峰值仍有约 62.2 GiB（合成结果），**依旧超物理内存**，
用户的双模型场景不会因此可用。故 a/b/c 必须一起交付才有意义，
不宜为了看得见进度而先合入半截。

---

## 5.9 单模型还剩多少空间（2026-09-05 用户提问）

### 5.9.1 耗时：已集中到单一去处

插子计时器实测（`a-2/0.2.obj` @0.05mm，286 层，7,369,346 列）。
注意口径：机器噪声只影响绝对值，**同一次运行内各段的占比不受影响**，
故一次运行足以定性。

| 段 | 每层 ms | 占比 |
|---|---|---|
| model / support 物化 | 0.97 | 0.3% |
| 内部空腔 | 3.34 | 1.1% |
| 支撑统计（含连通性） | 10.9 | 3.7% |
| **compose 及其后** | **278** | **94.8%** |

MF-03X3 之前内部空腔是 229 ms/层，现在 3.34 —— 稀疏剪枝加缓冲复用把前三项
基本清零了。**剩余耗时集中在对 44.2 MB 输出缓冲的整幅面往返**：

```text
assign 填背景         写 44.2 MB   -> 已解决（改为只重置活动列）
通道统计扫描          读 44.2 MB   -> 未解决，见 5.9.3
TIFF 写出             读 44.2 MB   -> 不可省，那就是输出本身
```

### 5.9.2 内存：最大一块是 relief_columns

按 7,369,346 列拆解 1.18 GiB 的构成：

```text
relief_columns        64 B/列   449.8 MB   <- 最大单项，已归还
column_ranges         12 B/列    84.3 MB
support_source_layers  4 B/列    28.1 MB
compose 输出缓冲       6 B/列    42.2 MB
六个单层 mask/scratch  1 B/列 x6  42.0 MB
```

`relief_columns` 的全部消费者都在层循环之前跑完，层循环只用 12 B/列的
`boundedReliefSpans`，故用完即归还。实测 a2@0.1mm 峰值 1.14 GiB -> 0.86 GiB。

### 5.9.3 MF-03X4 后的 10um 长跑准数

```text
a-2/0.2.obj @10um，1,429 层，单模型 CLI
                     X2a 前        X3 后        X4 后
  totalMs          1,135,091      639,903      566,649
                    18.9 分钟     10.7 分钟     9.44 分钟
  layerComputeMs   1,081,463      594,981      516,877
  peakWorkingSet   22~34 GB       1.18 GiB     0.84 GiB
```

相对本专项介入前：**耗时 -50.0 百分比，峰值 -97 百分比以上**。
长跑 1,429 层自平均，可单次引用（口径见 §5.7.4）。

### 5.9.4 结论：耗时仍有一项，内存已接近底

```text
耗时  还有一项已量化未做：update_layer_channel_stats 每层整幅面读 44.2 MB。
      空列的贡献是常数（列数 x emptyValue），可在 compose 内按活动列累积
      后解析补齐，省掉一次整幅面往返。
      未做的原因：它改错会让通道统计【悄悄偏差】而非报错，需单独一轮配合
      零漂移验，不适合与其他改动混在一起。

内存  剩余项都是 O(列数) 且各自有明确用途，没有再拿掉一大块的余地。
      要继续降只能减小工作幅面（例如按模型实际占用裁剪栅格），
      那会改变输出坐标系，属于另一类改动，不在本专项范围。
```

---

## 5.10 内存与耗时优化的阶段结论（2026-09-05 用户提问）

### 5.10.1 单模型 CLI 路径：已完成

`a-2/0.2.obj` @10um，1,429 层，栅格 1418 x 5197 = 7,369,346 列：

| 指标 | 专项介入前 | 现在 | 变化 |
|---|---|---|---|
| 耗时 | 18.9 分钟 | **9.44 分钟** | **-50.0%** |
| 峰值内存 | 22~34 GB | **0.84 GiB** | **-97% 以上** |
| 序幕（采样+支撑生成） | 277,200 ms | 642 ms | -99.8% |

这条路径**判定为已完成**：内存已不构成约束，耗时也已减半，
且剩余耗时集中在一处、收益有限（见 5.10.3）。

### 5.10.2 场景路径（UI 多实例）：进行中

| 指标 | 专项介入前 | 现在 | 目标 |
|---|---|---|---|
| 单实例斜率 | 73.7 MB/层 | **44.2 MB/层** | 与层数无关 |
| 双实例 15 层峰值 | 3.84 GB | **2.67 GB** | 百 MB 级 |
| 用户双模型 @10um 外推 | 270 GB | 约 130 GB | 百 MB 级 |

实例侧已流式（不再持有整栈），**合成结果仍累积**（6 B/列/层），
故峰值尚未与层数脱钩。写入侧流式（MF-05 步骤 4 第二步）落地后才到目标。

### 5.10.3 还剩什么，以及为什么不做

```text
耗时  剩余集中在 compose 及其后（占 layerCompute 的 94.8%），
      而那是对 44.2 MB 输出缓冲的整幅面往返：
        assign 填背景   写 44.2 MB   已消除（改为只重置活动列）
        通道统计扫描    读 44.2 MB   可省，未做
        TIFF 写出       读 44.2 MB   不可省，那就是输出本身
      即最多再省三分之一里的一份。通道统计的空列贡献是常数
      （列数 x emptyValue），可在 compose 内按活动列累积后解析补齐。
      未做的原因：改错会让通道统计【悄悄偏差】而非报错，需单独一轮验。

内存  单模型已接近底。拆解 0.84 GiB 后，剩余项都是 O(列数) 且各有用途
      （列区间、支撑起始层、六个单层 mask、输出缓冲）。
      再降只能缩小工作幅面（按模型实际占用裁剪栅格），
      那会改变输出坐标系，属另一类改动、不在本专项范围。
```

### 5.10.4 一句话结论

**单模型的内存与耗时优化已完成**（耗时减半、内存降两个数量级），
剩余项要么收益有限、要么属另一类改动。
**场景路径的内存优化尚未完成** —— 实例侧已解决，合成与写入侧还差一步，
那一步也是解除用户「双模型内存不足」的最后一环。

---
## 6. 风险与未决

1. **B2 时序等价是最大风险。** 悬空岛的「先全层 preliminary、再顺序发现并回写」时序
   必须逐字节等价，否则支撑连通性会变。B2 已有 retained oracle，但**接线时的调用顺序**
   仍需逐层 digest 比对。
2. **接线范围大。** 七个容器分布在主循环各处，`model_masks` 在 `slicer.cpp` 有 5 种下标形式。
   ~~建议按容器逐个替换~~ —— **见 §5.3 修订**：只有第一个容器逐层独立、可独立替换；
   剩余六个通过 `generate_support_masks` 耦合成一个簇，须整体改走有界路径。
3. **`slicer.cpp` 与 G2 的真实关系（v1.4 更正）。**

   `0d2a1bf` 的提交信息写「同步下沉使门禁 PASS、未新增豁免登记」，**表述有误导**：
   `src/slicer_core/slicer.cpp` 早在 `642d29e` 就已登记进 G2 豁免清单，
   故门禁在下沉与不下沉两种情况下都会 PASS —— 下沉并非门禁所迫。

   ```json
   { "path": "src/slicer_core/slicer.cpp", "rules": ["G2"],
     "reason": "用户 2026-09-01 授权放宽：MATOPQ 方案 A 需在 transfer 与 MATVOL
                两处适配调用透传退化面阈值；MO-04 将在此文件内接入不透明度到
                光油通道的判据。",
     "expiresWhen": "版本重构时统一清理 slicer.cpp（现 5645 行）" }
   ```

   下沉本身仍然正确（删除无调用者的死代码、提高内聚），只是**理由不成立**。

   更要紧的是：**那条豁免的 reason 只覆盖 MATOPQ 方案 A 与 MO-04，不含 MEMFLOW。**
   靠它为本专项的行数增长背书，等于悄悄借用别的专项的债额度。故本专项的处置是
   **不依赖该豁免**：每次接线都同步下沉，使 `slicer.cpp` 实测净减 ——
   MF-03X1 后 5,988 行，MF-03X2a 后 5,981 行（较 MF-03X1 再减 7 行，
   接线本身的 +137 行由内部空腔函数簇下沉全额抵掉）。
4. **多实例场景未覆盖。** 用户的双模型场景（`0.2+0.3`）需 MF-05 的 Barrier；
   本报告只覆盖单实例。

---

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-05 | v1.11 | 新增 5.10：应用户提问给出内存与耗时优化的阶段结论。单模型 CLI 路径判定为已完成（耗时 18.9 -> 9.44 分钟、峰值 22~34 GB -> 0.84 GiB）；场景路径进行中（单实例斜率 73.7 -> 44.2 MB/层，实例侧已流式而合成结果仍累积，尚未与层数脱钩）。并逐项说明剩余空间与不做的理由。 | 补记 MF-03X4 后的 10um 长跑准数：totalMs 639,903 -> 566,649（10.7 -> 9.44 分钟），peakWorkingSet 1.18 -> 0.84 GiB。相对本专项介入前累计：耗时 18.9 -> 9.44 分钟（-50.0 百分比），峰值 22~34 GB -> 0.84 GiB。 | 新增 §5.9，回答用户「单模型是否还有优化空间」。耗时侧插子计时器实测确认已集中到单一去处：compose 及其后占 94.8%，前三项在 MF-03X3 后基本清零（内部空腔 229 -> 3.34 ms/层）；剩余是对 44.2 MB 输出缓冲的整幅面往返，其中 assign 已改为只重置活动列，TIFF 读不可省，仅剩通道统计一次整幅面读可省。内存侧拆解 1.18 GiB 构成，确认 relief_columns（64 B/列，449.8 MB）是最大单项且其消费者都在层循环之前，用完即归还，实测 1.14 -> 0.86 GiB。结论：耗时还剩一项已量化未做（且标注了它改错会悄悄偏差而非报错的风险），内存已接近底，再降需改工作幅面、属另一类改动。 | 新增 §5.8，**修正 §5.6 的低估**。实施 MF-05 前逐段读代码，确认场景路径是三份整栈而非一份：N 份每实例栅格（10 B/列/层）+ 一份合成结果（6 B/列/层），两者在合成期间同时驻留，用户双模型 10um 合计约 269.6 GiB。另确认 `ComposeSceneLayersConsuming` 只覆盖单实例快路径，多实例仍走 Borrowed 不释放。最关键的修正：合成侧流式化受**类型不变量**阻挡 —— `ValidatedSceneLayerComposeResult` 的构造要求 `layers.size() == grid.layercount`，「全部层同时在场」是该类型存在的证据契约（让下游免于重扫每字节），故必须先把它改为逐层累积。据此把 MF-05 范围由「实例侧加屏障」修正为「实例侧 + 合成侧 + 证据侧三处一起」，并说明为何不宜先合入半截（只修实例侧仍有 62.2 GiB，依旧超物理内存）。 |
| 2026-09-05 | v1.7 | 新增 §5.7：应用户要求处理切片耗时。先插子计时器测出热点（compose 55.9%、内部空腔 32.5%），再据 `relief_report` 实测确认根因是「为占 2.53% 列的稀疏模型做满幅面扫描」，且包围盒剔除无效（bbox 就是整幅面）。给出精确等价的列集合剪枝判据（compose 的 else 链无末尾 else；活动表须含面内被围空腔列，故按「无模型且能连到边界」排除而非按「有模型」保留）。三处改动使 a-2@0.1mm 的 layerComputeMs 由 101,014 降至 46,726（-53.7%），三判据逐字节全等。记录一处反直觉发现：只做剪枝而不复用缓冲，内部空腔耗时几乎不降 —— 按幅面计的固定开销与按占用计的工作量是两笔账。并明确列出仍未处理的三处按幅面开销及未做原因。 |
| 2026-09-05 | v1.6 | 新增 §5.6：回头验证用户第二个诉求（双模型内存不足）时发现**该失败与三个 mask 栈无关**。场景路径 `MultiModelProductionService` 把全部实例的全部层留到最后一起合成，单实例 raster 每层持有 6 通道 + 4 张归属 mask = 每列每层 10 B，用户场景下 **103.7 GiB/实例**，比 X2a 修好的 31.11 GiB 还大一个量级且按实例线性叠加。根因是 `LegacySceneLayerAdapter` 的 `ownedlayercallback` 逐层 `push_back` 不释放，使 MF-02「消费即释放」合同收益归零。合成本身只需各实例的同一层，障碍是 instance-major 切片与 layer-major 合成的错位，建议走 MF-05 Barrier。同时修正 §2：「MF-04 内存收益不成立」只对 CLI 单模型路径成立，对场景路径不成立。 |
| 2026-09-04 | v1.5 | 新增 §5.5：MF-03X2a 实施与验证。三个整栈（`model_masks`/`support_masks`/`support_type_maps`）改按列区间推导 + 按层物化，采样阶段一并跳过 10.37 GB 分配。四判据逐字节全等（tm2-5/yz 同时与主仓 P1 基线相同）、单元测试含对 retained 参考实现的 40 组暴力等价比对。**解除用户阻塞实测：`a-2/0.2.obj` @10um 峰值由 22~34 GB 降至 1.14 GiB（约 -95%），序幕由 277,200 ms 降至 642 ms，完整跑完 1,429 层用 18.9 分钟；retained 那次中止运行留下的前 457 层与 bounded 逐字节 457/457 一致。** 记录一次真实漏项（`AddInternalVoidSupportForLayer` 无条件逐层块被漏，`internal_void.enabled` 默认 true，导致 r01 二十层各差一字节）及其教训：按条件分支枚举会漏掉无条件语句。并诚实界定收益：内存是实质改变，时间仅略优，进一步压时间需另立性能任务。 |
| 2026-09-04 | v1.4 | **更正 §6 第 3 条。** `0d2a1bf` 提交信息称「同步下沉使门禁 PASS、未新增豁免」有误导：`slicer.cpp` 早在 `642d29e` 已登记 G2 豁免，门禁两种情况都会 PASS，下沉并非门禁所迫（下沉本身仍正确，只是理由不成立）。更要紧的是该豁免 reason 只覆盖 MATOPQ 方案 A 与 MO-04、不含 MEMFLOW，靠它背书等于借用别的专项的债额度。故本专项处置为不依赖该豁免：每次接线同步下沉使 slicer.cpp 实测净减（X1 后 5,988 行、X2a 后 5,981 行）。 |
| 2026-09-04 | v1.3 | 新增 §5.4：按配置收缩范围。逐项核对用户 `a2_probe.json` 的实际激活项，确认 `mode = bottom_projection` 下三个耦合难点全部不激活 —— 岛发现（`unsupported_only_enabled` false）、形状优化（`shape_enabled` 默认 false）、两种光油（未配置）。故该档**不含任何无界随机访问**，剩余向下遍历只有按列的 bottom-projection，是主循环已在算的 `column_ranges` 的纯函数。两点结论：§6 第 1 条「B2 时序等价是最大风险」在本档不适用（不需要 B2/B3，只需已 COMPLETE 的 B1 + A）；解除阻塞的改动远小于全模式接线。据此把 MF-03X2 拆为 X2a（本档，关键路径）与 X2b（全模式）。 |
| 2026-09-04 | v1.2 | **修正 §3 占用表。** 首版把七个容器都按已分配计入得 72.62 GB；复核分配条件后确认四个是条件分配（`surface_varnish.enabled` 默认 false；`outerVarnishMasks` 与 `upperBoundaryMasks` 无光油时返回 `{}`），在用户实际阻塞配置下为 0。该配置真实合计 **31.11 GB**，对上物理内存 31.6 GB，比 72.62 GB 更能解释实测峰值 22~34 GB 与换页。同时确认 `SupportType` 是 `uint8_t`（非 4 字节）、`support_masks`/`support_type_maps` 的 resize 在 `support.enabled` 检查之前故无条件分配。据此新增 §3.1：MF-03X1 对 10um 阻塞场景收益为零，解除阻塞的全部收益在 MF-03X2。 |
| 2026-09-04 | v1.1 | 首容器（`outer_surface_masks` / `inner_surface_masks`）接线完成并零漂移通过，提交 `0d2a1bf`。据此修订 §6 第 2 条：「按容器逐个替换」只对第一个容器成立——它逐层独立；剩余六个通过 `generate_support_masks`（整栈进整栈出）耦合成一个簇，且 `model_masks`/`support_masks`/`support_type_maps` 各有恰好一处 `.at(target_layer)` 且同处一个回写循环，必须同时转换。风险 3（G2 门禁）解除：同步下沉使 slicer.cpp 净减 137 行，未新增豁免。新增 §5.3 与共同前置 `geometry/SliceGridSpec.h` 说明。 |
| 2026-09-04 | v1.0 | 首版。推翻两个认知：`slicer_cli` 的 TIFF 输出早已逐层流式（故 MF-04 的内存收益不成立），真正瓶颈为七个 mask 整栈驻留 72.62 GB。量化跨层访问模式并确认 `target_layer` 的「当前层之下全部层」随机访问是最难有界化的一环，同时确认 MF-03B2 已用 compact 事件在设计层解决。提出 MF-03X「主循环有界接线」并列出六项替代能力均已 COMPLETE，MF-04 范围重定义为原子发布语义。 |
