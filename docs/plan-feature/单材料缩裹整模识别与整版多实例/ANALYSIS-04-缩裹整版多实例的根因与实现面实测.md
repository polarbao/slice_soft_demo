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

### 为什么不能只把通道数改成运行时变量

一个看着更省事的想法是：给请求加一个 `channelCount` 字段，
把那 5 个函数里的 `kChannelCount` 换成它，共享类型一动不动。
**这条路走不通，卡在输出类型上。**

实测 `SceneInstanceRasterLayer::output` 的类型是 `RgbwsvProductionLayer`，其中：

| 成员 | 类型 | 能否容纳七通道 |
| --- | --- | --- |
| `channels` | `std::vector<std::uint8_t>` | ✅ **本来就是变长的**，装 7×像素 字节没问题 |
| `channelOrder` | `std::array<std::string, 6>` | ❌ 装不下第七个通道名 |

也就是说**像素字节早就与通道数无关了**，卡住的只有描述性元数据
（`channelOrder`，以及统计结构里的 `std::array<std::uint64_t, 6>`）。
运行时 `channelCount` 能让合成循环跑对，但产出的层**无法自述它是七通道的**，
包写入器拿到手也不知道第七个平面叫什么。

把这几个数组改成变长 ⇒ 回到 §6b 开头那张表的 27 文件 / 24 处硬写。
而七通道的对应类型 `RgbwsvtProductionLayer`（`array<, 7>`，通道名含 `"T"`）
**已经存在**。所以正确解法是让合成器按层类型模板化、两份实例化各用各的类型，
而不是把一个类型拉宽去同时伺候两种协议。

## 6c. 再次修正（最终方案）：**T 作为后置一遍叠加，六通道合成器一行不动**

继续往下读 `run_slicer` 时发现：**单模型路径产七通道层的方式，本身就是「后置叠加」**，
不是把合成逻辑做成七通道的。`slicer.cpp:1103-1114`：

```cpp
std::optional<RgbwsvtProductionLayer> transferLayer;
if (transferSession.has_value())
{
    transferLayer = ComposeLegacyTransferChannelLayer(
        transferSession.value(),
        RgbwsvProductionLayer{ ... .channels = layer },   // ← 先出完整六通道层
        current_model_mask);
    transferCanvas.Apply(transferLayer.value());
}
```

而 `ComposeRgbwsvtLayer`（`output/rgbwsvt/RgbwsvtProtocol.cpp:42`）的签名是：

```cpp
RgbwsvtProductionLayer ComposeRgbwsvtLayer(
    const RgbwsvProductionLayer& rgbwsvLayer,   // 六通道层
    std::span<const std::uint8_t> modelMask,
    std::span<const std::uint8_t> transferMask,
    std::uint8_t transferValue);
```

语义是：缩裹像素**丢弃全部六通道只写 T**，非缩裹像素六通道原样透传、T 留空。

### 于是场景级的做法可以完全照搬，升一层而已

| 步 | 做什么 | 代价 |
| --- | --- | --- |
| 1 | 让**现有六通道合成器原样**跑完 N 个实例 → 整版 RGBWSV 层 + 整版模型掩膜 | **零改动** |
| 2 | 把各实例的**缩裹掩膜**按各自偏移合成到整版掩膜 | 新增，但用的是合成器已有的摆放数学 |
| 3 | 把 1 和 2 交给**现有的 `ComposeRgbwsvtLayer`** → 整版七通道层 | **零改动** |

**六通道合成器不动、`ComposeRgbwsvtLayer` 不动**，新代码只有第 2 步的掩膜合成。
这不是新发明的机制，而是本仓在单模型层面**已经在用**的模式，只是搬到整版上。

### 三版方案的演进（每一版都是被实测推翻的）

| 版本 | 想法 | 被什么推翻 |
| --- | --- | --- |
| v1 | 把通道数与几个 `array<,6>` 泛化 | 影响 27 文件 / 24 处硬写，直接威胁字节基线 |
| v2 | 把合成器按层类型模板化，实例化两份 | 可行但要动 ~20 个函数，仍在六通道热路径上动刀 |
| **v3** | **T 后置叠加，六通道合成器零改动** | — `run_slicer` 本来就这么做的 |

> 教训与 §3.5 同一条：**先读既有实现怎么做，再设计**。
> v1 和 v2 都是在没读 `slicer.cpp` 的 T 叠加之前拍的。

### 这一版仍需解决的两件事

1. **步 2 的掩膜合成要复用合成器的归属裁决**，不能自己再算一遍摆放与重叠，
   否则两处逻辑会漂移。**落法见下方 §6d——比「导出归属再外部合成」更干净。**
2. ~~**V/T 重叠**：整版下多了一种新重叠——**跨实例**的 V/T 相撞，需确认走的是同一条拒绝路径。~~
   **已核查，不是问题——现有合成器已经挡住了。** 见下。

### 跨实例重叠：甲选让 T 白捡了既有的全部保护

`ResolveCrossInstancePixel`（`SceneLayerComposer.cpp:760-788`）对跨实例相撞的处理：

| 相撞形态 | 现有行为 |
| --- | --- |
| `Model` × `Model` | **拒绝**：`InstanceOverlap`，"different instances claim the same model pixel" |
| `Model` × 其它材质归属 | **拒绝**：`MaterialConflict`，"model ownership conflicts with another instance material" |

T 跟随 `Model` 归属（甲选），于是：

- **跨实例 T × T** ＝ Model × Model ⇒ 已经是 `InstanceOverlap`，fail-closed；
- **跨实例 T × V** ＝ Model × OuterVarnish ⇒ 已经是 `MaterialConflict`，fail-closed。

两种新重叠**都已经被现有代码挡住**，无需新写拒绝逻辑，也无需把
`slicer.cpp:1124` 的 `E_MATOPQ_VARNISH_TRANSFER_OVERLAP`（那条是**模型内**
V/T 相撞，K3 表决未实施）搬到场景层。

> 这是甲选的第三项白捡收益：归属规则零代码、跨实例保护零代码。
> 乙选（新开 `TransferWrap` 归属）反而要把上面两条规则**逐条重写一遍**，
> 且每条都得重新论证——因为新归属类不在现有的 `Model` 判断里。

**对 MW3-03 的影响**：原定的「T 重叠计数」失去意义——合成器根本不允许
两个实例的模型像素重叠，重叠即整单拒绝，不存在"悄悄重叠"的情形。
MW3-03 因此改为**验证**这两条拒绝确实在整版缩裹下触发（反例用例），
而不是新增计数。

> 对用户版面的实际约束：12 件甲片在同一层的 XY 投影**不得相交**。
> 截图上各件之间留有明显间隙，满足。但这条约束要写进使用说明——
> 它不是缩裹独有的，六通道整版一直如此。

### T 的归属规则不需要写任何代码

通道下标是 S=4、V=5、T=6（`kTransferChannelOffset{6U}`）。
`WriteOwnedPixel` 在 `Model` 归属下的逻辑是「除支撑外逐通道照抄，光油还要再查
`modelvarnishownership`」——通道 6 既不是 `kSupportChannel` 也不是 `kVarnishChannel`，
**天然落进通用分支，即「跟随 Model 归属」**，正是本专项裁定的甲选语义。

也就是说甲选不仅是语义上更贴切的选择，**实现代价恰好为零**；
乙选才需要新开归属类并改这个函数。MW3-03 因此缩小为「补重叠计数」一项。

## 6d. 掩膜合成的接法：在合成器内顺带合成，而不是导出归属外部再算

两种接法比较过，取后者：

| 接法 | 做法 | 问题 |
| --- | --- | --- |
| 甲：导出归属 | 给请求加 `ownershipsink`，逐层交出 `ownership` / `ownerindices`，外部据此合成 T 掩膜 | 外部拿到的是**整版**坐标，要写回各实例的 T 掩膜得做**整版→实例局部**的反向映射，而偏移量在 `InstancePlacement` 里、是合成器的内部状态。等于把摆放数学在外面再实现一遍——**正是要避免的漂移** |
| **乙：顺带合成** | 给 `SceneInstanceRasterLayer` 加一个可选的 `transfermask`，合成器写像素时**顺手**把它写进整版 T 掩膜 | 需要动 `WriteOwnedPixel` 的签名 |

**取乙**，关键实测依据：`WriteOwnedPixel` 全文**只有一个调用点**
（`SceneLayerComposer.cpp:794`，在 `ResolveCrossInstancePixel` 内）。
首次认领与跨实例改写都走它，因为 `Empty` 是归属序数最小的一档，
第一次认领同样落在 `sourceOwnership > currentOwnership` 那条分支上。

于是「整版 T 掩膜」的合成就是在那一个地方加一行：
`Model` 归属下把 `source.transfermask[sourcePixel]` 写到
`globalTransferMask[destinationPixel]`，**摆放、偏移、跨实例裁决全部原样复用**，
一行反向映射都不用写。

**对字节基线的影响**：加的是一个**并行缓冲区**的写入，六通道字节一个都不动；
`transfermask` 为空（既有六通道调用方全都如此）时整条跳过。
byte-identical 由构造保证，仍由 MW3-09 实测确认。

> 甲案的 `ownershipsink` 补丁已写好又废弃。留这条记录是因为
> **「先导出中间状态、再在外面重算」是个反复出现的诱惑**——
> 它看着更解耦，实际是把被复用方的内部不变式复制了一份出去。

## 6e. 实施中才暴露的两件事（盘点时都漏了）

§2.1 那张表只列了两道护栏。实际落地时又撞出**第三道**，还改了**一条既有不变量**。
两件都不是「多一个开关」，而是原方案的盲区，记在这里。

### 第三道护栏：`E_MATVOL_T_PROTOCOL_INVALID`

`ValidateLegacyTransferChannelRunBoundary` 要求 T 通道的运行要么
**只算不写**（`directComputeOnly`）、要么**写 TIFF 与清单的候选包**。
而 `directComputeOnly` 的判据里带一条 `!usesAdapter`。

整版路径每个实例的 `run_slicer` 恰恰是**经适配器、只算不写**——
`LegacySceneLayerAdapter` 把 `write_tiff_layers` / `write_preview_files` /
`write_reports` 三个开关全置 `false`，包由整版合成器统一写。于是被这条挡下。

那条当初是对的：T 那时只有单模型一条路，经适配器就意味着有人要拿它的层去
另作他用，而那时**没有任何东西能保证那份层会被正确地叠上 T**。现在有了。

处置沿用本仓既有做法——加一个显式准入标志
（`SliceRunOptions::transfer_plate_compute_only`），而**不是**把 `!usesAdapter` 删掉：
删掉会连带放行其它未经考虑的适配器用法。标志只是准入、不是豁免，
三个写盘开关仍须全关，哪天有人给它开了写盘，这里照样挡下。

### 一条既有不变量被扩展：像素闭合

闭合校验要求 `Model` 归属的像素**在六通道里至少印一个**，否则
「模型像素凭空空白」。整模缩裹的六通道层是**全空的**——内容全在 T 里，
于是逐实例与整版两处闭合都报错。

改法是让判据认第七通道：

> 这条不变量要问的是「这个模型像素**有没有真的印出东西**」，
> 而不是「有没有印在这六个通道里」。缩裹像素印了，只是印在该函数
> 看不见的通道上。

豁免是**有据的**而非放宽：掩膜只在启用 T 的运行里非空，且它是模型掩膜的子集；
六通道运行下恒为空、该分支永不触发，原保证一字未改。

### 为什么这两件事只能靠跑出来

它们都不在「谁调用谁」的静态关系里——前者是一个布尔表达式里的一项，
后者是一条写在别处的语义要求。读代码读到的是结构，跑起来撞到的才是约束。
本专项从 §3.5 到这里，**每一次方向更正都来自一次真实失败**。

## 6f. 实机暴露的两件事（2026-09-20 夜）

护栏摘掉、用户实跑后，暴露了一个功能缺陷和一个性能问题。两件都记在这里，
因为它们各自指向一类**系统性**的疏漏，而不只是两个待修项。

### 一、漏填 `grid_px`：在比真实调用方低一层的地方验证

实机报 `PM-SLICER-CONTRACT-0060`「incomplete package evidence」。
真因是 `RunTransferPlateProductionEntry` 照抄六通道入口时漏了
`result.grid_px`，而 Worker 的产出证据检查
（`WorkerSliceExecutor.cpp:443-450`）要求它两维都 > 0。

**为什么 MW3-10 没抓到**：它直接调 `RunMultiModelProductionService`，
绕过了生产门面与 Worker 这两层。包写得完全正确，只是返回给调用方的
**摘要少一个字段**——低的那层全绿，真实边界仍然红。

> 这和 MONOWRAP-09「逐个模型跑 CLI、从未跑整版」是**同一种漏法**，
> 本专项一天内栽了两次：
> **在比真实调用方低一层的地方验证，低的那层全绿说明不了任何事。**

改法不是补个断言了事，而是把 MW3-10 整体**提到门面层**
（`CreateProductionSliceFacade()->Run()`，即 Worker 用的同一道边界），
并把 Worker 的证据判据抄一份断言在测试里。
证伪：撤掉 `grid_px` 回填后该用例立刻变红，且报错直指 `grid_px`。

### 顺带补上的覆盖缺口：X/Y 补白

用户的实际配置里「补齐至 X=0 / Y=0」**都开着**，而 MW3-10 没开——
那条路径从未被验过。补上后通过，且确认补白确实生效：
关闭时画幅 974×96，开启时 1004×126（3mm 偏移 + 2mm 边距在 150dpi 下各加 30px）。

补白在合成**之前**扩展全局网格，故整版掩膜与合成层天然落在同一张已补白的
画布上——原先这是推理，现在是实测。

### 二、耗时：一半是白做功，一半是结构性的

实测同一版 12 件、三次取最小：

| | 单材料光油 | 整模缩裹 | 差 |
| --- | --- | --- | --- |
| 修复前 | 18.0 s | **24.5 s** | +36% |
| 修复后 | 17.6 s | **21.0 s** | +20% |

**白做功**：整版路径下每个实例的 `run_slicer` 仍在逐层构建一份完整的
七通道层（`pixelCount × 7` 字节），`transferCanvas` 还给它补白再搬一次——
**而这份层没有任何消费者**：整版路径 `write_tiff_layers` 恒为 false，
只取它的 `transferMask`。

改法是把 `ComposeLegacyTransferChannelLayer` 拆成两步，整版只调
`MaterializeLegacyTransferChannelMask`。单实例路径一行未动——它确实要
那份七通道层去写 TIFF。省下约 14% 总耗时。

**剩下的 20% 是结构性的**，不是缺陷：七通道比六通道多 17% 的 TIFF 字节，
外加整版装配那一遍全画幅扫描。光油那条路根本没有这两项。
要再压只能动装配本身（例如就地改写而非新建层），收益有限、风险不小，
当前不做。

## 7. 工作量评估

> 下表已按 §6c 的 v3 方案更新；v1/v2 的估计见该节的演进表。

| 档 | 内容 | 估计 |
| --- | --- | --- |
| 六通道合成器 | **零改动**（只增加「把整版模型掩膜与归属暴露出来」的出口） | 小 |
| 整版 T 掩膜合成 | 各实例 T 掩膜按偏移并入整版，复用合成器的归属裁决 | 中 |
| 七通道装配 | 调**现有** `ComposeRgbwsvtLayer`，零改动 | 极小 |
| V/T 跨实例重叠 | 确认走既有 fail-closed 路径 + 补计数 | 小 |
| 生产入口改道 | `RunTransferProductionEntry` 仿 `RunExistingProductionEntry` | 中 |
| 护栏放开 | 两处 fail-closed 改为允许多实例 | 小（但按门禁规则须留授权痕迹） |
| 包写入 / 报告 / 契约 | 七通道整版包 | 中 |
| 验证 | 单实例字节级不变 + 整版新用例 + 全量档 | 大 |

**整体量级：明显小于 MONOWRAP 主体。** 真正的新代码只有「整版 T 掩膜合成」一件，
其余是接线与验证。关键是 §6c 的实测结论——**本仓在单模型层面已经在用后置叠加**，
整版只是把同一模式升一层，六通道热路径与字节基线因此不被触碰。

**验证仍是最大的一档**，且不可压缩：`ComposeRgbwsvtLayer` 虽零改动，
但它这次收到的是整版层而非单模型层，边界条件（跨实例掩膜越界、V/T 相撞）全是新的。
