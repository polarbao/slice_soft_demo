# ANALYSIS-04：缩裹整版多实例的根因与实现面实测

> 专项：MONOWRAP ｜ 日期：2026-09-20 ｜ 触发：用户在 Release 构建里排 12 件缩裹模型切片失败
> 上游：[ANALYSIS-01](ANALYSIS-01-现状与改动面.md)、[TASKS-01](TASKS-01-任务清单.md)
> 下游：[TASKS-03](TASKS-03-缩裹整版多实例.md)

## 1. 现象

Release 构建（SliceSoft 0.2.510-dev），工艺配置选「缩裹材料」、预设选「单材料缩裹」，
版面上 12 件可见模型，点「开始切片」后 1.21 s 失败：

```
错误码：PM-SLICER-LAYOUT-0022
错误说明：slice.rgbwsvt requires exactly one visible scene instance
```

## 2. 根因：RGBWSVT 这条能力**没有多实例合成器**

### 2.1 两道护栏

| 位置 | 错误码 | 时机 |
| --- | --- | --- |
| `apps/slicer_worker/slice/WorkerSliceRequestMaterializer.cpp:405` | `PM-SLICER-LAYOUT-0022` | Worker 物化请求时，执行前 |
| `src/slicer_core/engine/ProductionSliceFacadeFactory.cpp:287` | `PM-SLICER-LAYOUT-0023` | 核心生产入口，执行中 |

两道都是 fail-closed 的纵深防御，用户命中的是第一道。

### 2.2 架构上为什么只能一个

`RunTransferProductionEntry`（`ProductionSliceFacadeFactory.cpp:246`）的实现是：

1. 解码场景，遍历实例，**挑出那个唯一可见实例**（多于一个即 `LAYOUT-0023`）；
2. 用它的 `modelid` 找到模型源文件，塞进 `options.inputoverride`；
3. 把它的实例变换塞进 `options.instanceoverride`；
4. 调**单模型**的 `run_slicer(profilePath, options)`；
5. `result.engine_version = "legacy-rgbwsvt-scene-v1"`。

也就是说 **RGBWSVT 是一个套了场景外壳的单模型切片器**，它根本没有走六通道那套
「逐实例出栅格 → 逐层跨实例合成 → 写整版包」的管线。

对照六通道的 `RunExistingProductionEntry`（同文件 `:172`）：它构造
`MultiModelProductionRequest` 后直接调 `RunMultiModelProductionService`，
由 `MultiModelProductionService` 逐实例产栅格、`SceneLayerComposer` 逐层合成。
两个入口的**结构差异就是这次要补的东西**。

### 2.3 这是立项时的既定行为，不是回归

`docs/slice/DOC/DOC_PREP_MATVOL_T_T_06_SceneWorkerHost双协议透传准备.md:85`
的 T-06B 验收条件原文：

> 03 单实例成功；**多实例**/08/09/取消均 **fail closed** 且无残包

`docs/slice/DOC/DOC_DESIGN_XPAD_X原点输出画幅补白.md:19`：

> RGBWSVT 采用独立单实例 run_slicer 路由，在最终七通道输出处补白，
> **不经过六通道 Scene 合成器**；T 原有单可见实例准入限制不变。

所以护栏是被测试钉死的设计，MONOWRAP 没有引入它、也没有改动它。

### 2.4 但 MONOWRAP 的验证确有盲区

TASKS-01 的 MONOWRAP-09 记的「10/10 通过」是**逐个模型跑 CLI** 得到的，
从未跑过一版多件。整模缩裹的识别逻辑本身没问题，但
「这条工艺能不能用于真实排版作业」这个问题当时没有被问到。
本文即为补上该盲区的产物。

## 3. 为什么「单材料光油 / 白墨」可以整版切

工艺里的 `output.packageProtocol` 决定走哪条能力
（`apps/slicer_ui_host_sim/HostSliceProtocolRoute.cpp:65-82`）：

| 工艺 | `packageProtocol` | 能力 | 多实例 |
| --- | --- | --- | --- |
| 单材料光油 / 白墨 | `p0.rgbwsv.2` | `slice.rgbwsv` | ✅ 走 `SceneLayerComposer` |
| **单材料缩裹** | `p0.rgbwsvt.1` | `slice.rgbwsvt` | ❌ 单实例专用路由 |

光油是六通道里的 V、白墨是 W，两者都在合成器覆盖范围内；
缩裹是第七个通道 T，那条路上没有合成器。
**这条差异与 MONOWRAP 的整模识别无关**，换成按 mtl 颜色匹配的旧版缩裹工艺同样撞墙。

## 4. 实现面实测：不是重写

### 4.1 已经存在、可直接复用的

| 件 | 位置 |
| --- | --- |
| 七通道层类型 `RgbwsvtProductionLayer` | `src/slicer_core/output/rgbwsvt/` |
| 通道数常量 `kRgbwsvtChannelCount` | `output/rgbwsvt/RgbwsvtProtocol.h` |
| 单层七通道合成 `ComposeRgbwsvtLayer` | `materials/transfer/LegacyTransferChannelSession.cpp:62` |
| 七通道包读写与预览 | `PackageQueryFacadePackage/Preview.cpp`、`LayerPreviewWriter.cpp` |

也就是说**七通道的「一层怎么长」早就有了**，缺的只是「N 个实例的层怎么拼成整版一层」。

### 4.2 合成器的通道绑定程度（实测）

`SceneLayerComposer.cpp` 共 1467 行。通道相关的写法分两类：

**(a) 通用循环——占绝大多数。** 全文用的是 `for (channel < kChannelCount)` 与
`pixelIndex * kChannelCount` 这类按通道数迭代的写法（`:373/379/424/425/685/703/711/1151/1173/1231`），
**没有把 R/G/B/W/S/V 逐个点名**。通道数来自单一 constexpr：

```cpp
// SceneLayerComposer.cpp:21
constexpr std::size_t kChannelCount{kSceneChannelCount};
// SceneSourcePixelClosure.h:13
inline constexpr std::size_t kSceneChannelCount{6U};
```

**(b) 具名通道的语义例外——少数且集中。** `WriteOwnedPixel`（`:693-736`）
与 `SceneLayerComposerCancellation.h:44-76` 对 `kSupportChannel` / `kVarnishChannel`
有专门规则：支撑像素只在 `Support` 归属下写、外圈光油只在 `OuterVarnish` 归属下写、
模型归属下光油还要再查 `modelvarnishownership`。

结论：**合成逻辑本身是通道无关的，硬编码集中在「通道数」与「三类归属的具名通道」两处**。

### 4.3 绑死在 6 的具体清单

| 位置 | 形态 |
| --- | --- |
| `pipeline/SceneSourcePixelClosure.h:13` | `kSceneChannelCount{6U}`，并参与 `SceneSourcePixelIndex` 的下标运算 |
| `output/rgbwsv/RgbwsvPackage.h:33` | `std::array<std::string, 6> channelOrder` |
| `output/rgbwsv/RgbwsvPackage.h:47-48` | `std::array<std::uint64_t, 6> printPixels / emptyPixels` |
| `pipeline/SceneLayerComposer.cpp:81-82` | channelOrder 比较函数签名收 `const std::array<std::string, 6>&` |
| `pipeline/SceneRasterTypes.h:125` | 请求里的 `RgbwsvProtocol protocol` |
| `pipeline/SceneRasterTypes.h:140` | `layersink` 回调的 `RgbwsvProductionLayer&&` |

## 5. 唯一的新设计决策：**T 通道跨实例重叠归谁**

这是本次**唯一无法从现有代码推导**的问题，其余都是接线。

现有三类归属（`SceneRasterOwnership`）：`Model` / `OuterVarnish` / `Support`。
两个实例在同一像素相撞时由 `ResolveCrossInstancePixel`（`:738`）裁决。
T 通道需要明确：

- **甲选（推荐）**：T 跟随 `Model` 归属——缩裹像素属于哪个模型，就归那个模型的实例。
  两件甲片的缩裹区若重叠，按与 RGB 相同的跨实例规则裁决，不额外开一类归属。
  理由：缩裹是**模型自身的一部分**（整模模式下 T 就等于模型掩膜），
  语义上贴近 RGB 而非外圈光油；且不引入新归属类，改动面最小。
- **乙选**：为 T 新开一类 `TransferWrap` 归属，仿照 `OuterVarnish` 允许缩裹跨出模型轮廓。
  只有在「缩裹要包住模型外一圈」这种工艺需求出现时才有必要。

> 实务上甲片排版留有间隙、缩裹区不重叠，两案产出一致；差异只在重叠时。
> 故建议先落甲选，并**在合成器里对 T 重叠加计数**，真有重叠时报告里看得见。

## 6. 影响面与风险

| 风险 | 说明 | 缓解 |
| --- | --- | --- |
| 既有单实例缩裹产出改变 | 换管线后即便结果应当相同，也可能有一字节差 | 单实例场景必须**字节级比对**改动前后 |
| 六通道产出被连累 | 通道数泛化触碰的是六通道共用代码 | 11 例字节级基线必须全绿，这是硬门槛 |
| 内存 | 整版 N 实例七通道比单实例六通道占用更大 | `layersink`/`layerprovider` 逐层出入口已存在，沿用即可 |
| 契约 | 报告与 schema 的通道枚举要跟随 | 同版本放宽的做法见 [DECISION-01](DECISION-01-开工门四问的裁定与依据.md) |

## 6b. 设计修正：**模板化合成器**，不动共享六通道类型

§4.3 列出「绑死在 6」的清单后，第一反应是把 `kSceneChannelCount` 与那几个
`std::array<..., 6>` 泛化掉。**实测影响面后否掉了这个方向**：

| 符号 | 使用文件数 |
| --- | --- |
| `RgbwsvProtocol` | 17 |
| `RgbwsvProductionLayer` | 27 |
| `channelOrder` | 60 |
| 直接硬写 `std::array<std::string, 6>` / `<std::uint64_t, 6>` | 24 处 |

把共享类型的数组改成变长，等于在**六通道生产路径**上动刀，而六通道正是
11 例字节级基线守着的东西——为了加 T 去动它，风险与收益完全不成比例。

**改为：把合成器模板化，按「通道数 + 协议类型 + 层类型」实例化两份。**

- 六通道实例化继续用 `RgbwsvProtocol` / `RgbwsvProductionLayer`（`array<,6>` 原样不动），
  编译期常量不变 ⇒ **生成的代码与现在等价，字节级基线最有把握不被碰**；
- 七通道实例化直接用**已经存在**的 `RgbwsvtProtocol` / `RgbwsvtProductionLayer`（`array<,7>`）；
- 改动面收敛到合成器自身 + 新增的七通道调用路径，共享类型**零改动**。

实测合成器里真正用到 `kChannelCount` 的只有 **5 个函数**
（`ComputeLayerSizes` / `ValidateInstance` / `ValidateLayer` /
`ComposeSceneLayersWithInstances` / `WriteOwnedPixel`），
外加 `SameChannelOrder` 的 `array<std::string, 6>` 签名要跟着模板化。
1467 行里需要动的是这几处，不是全文。

### T 的归属规则不需要写任何代码

通道下标是 S=4、V=5、T=6（`kTransferChannelOffset{6U}`）。
`WriteOwnedPixel` 在 `Model` 归属下的逻辑是「除支撑外逐通道照抄，光油还要再查
`modelvarnishownership`」——通道 6 既不是 `kSupportChannel` 也不是 `kVarnishChannel`，
**天然落进通用分支，即「跟随 Model 归属」**，正是本专项裁定的甲选语义。

也就是说甲选不仅是语义上更贴切的选择，**实现代价恰好为零**；
乙选才需要新开归属类并改这个函数。MW3-03 因此缩小为「补重叠计数」一项。

## 7. 工作量评估

| 档 | 内容 | 估计 |
| --- | --- | --- |
| 通道数泛化 | `kSceneChannelCount` 与三处 `array<,6>` 改为随协议走 | 中 |
| T 归属语义 | `WriteOwnedPixel` + 取消路径补 T 分支 | 小 |
| 生产入口改道 | `RunTransferProductionEntry` 仿 `RunExistingProductionEntry` | 中 |
| 护栏放开 | 两处 fail-closed 改为允许多实例 | 小（但按门禁规则须留授权痕迹） |
| 包写入 / 报告 / 契约 | 跟随七通道 | 中 |
| 验证 | 单实例字节级不变 + 整版新用例 + 全量档 | 大 |

**整体量级：与 MONOWRAP 主体相当或略大，显著小于「重写合成器」。**
关键是 §4.2 的实测结论——合成算法不必重新设计，只需泛化与接线。
