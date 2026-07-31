# DEV_13G 支撑投影铺底与层间连续性设计

> 文档状态：DEV / IMPLEMENTED
> 版本：v1.2
> 日期：2026-07-30

## 1. 模块边界

```text
model/auto-orient：只负责姿态、frontUp 证据和 minZ=0；
support generation：生成 lower/internal/unsupported/upper 支撑；
support base projection：从最终普通支撑派生前 N 层铺底；
material composer：继续按既有优先级写 RGBWSV；
report/UI：解释配置、有效范围和支撑来源。
```

13G 不允许 support 模块自行判断或旋转模型；物理铺底只对已经完成自动定向的模型做确定性的
Z 向抬高。

## 2. 执行链路

```text
Load model
-> AutoOrient + FrontUp Gate + GroundToZ0
-> Resolve base-projection layer placement
-> prepend_below_model: lift model by N * layerThickness
-> Build model masks
-> Generate ordinary support masks
-> Apply support shape policy
-> Build max support footprint
-> Apply base projection to layerIndex [0, N)
-> Apply varnish/support priority
-> Recalculate support stats
-> Compose RGBWSV
-> Write report/manifest/preview
```

## 3. 数据结构

已在 `SupportConfig` 增加：

```cpp
struct SupportBaseProjectionConfig
{
    bool enabled{false};
    int layer_count{30};
    std::string layer_placement{"overlay_existing"};
    std::string source{"max_support_footprint"};
};
```

遵循当前结构体字段命名规则；公开接口补 Doxygen。

`SupportType` 已增加：

```text
ProjectionBase
```

优先级建议：

```text
InternalVoid > UnsupportedIsland > FullVerticalProjection
> UpperProjection > BottomProjection > ProjectionBase > None
```

这样铺底补充基础，但不覆盖更具体的支撑原因。

## 4. 核心算法

### 4.1 最大支撑投影

输入是完成 shape policy 后、应用 base projection 前的普通支撑：

```text
baseFootprint[p] = OR ordinarySupport[layer][p]
```

`overlay_existing` 历史兼容应用：

```text
effectiveLayerCount = min(configuredLayerCount, grid.layerCount)
for layer in [0, effectiveLayerCount):
    if modelMask[layer][p] == 0:
        set support[p] = true
        if no higher-priority support reason:
            type[p] = ProjectionBase
```

随后既有 OuterVarnish priority 可清除与光油壳层冲突的 S。

### 4.2 新增物理铺底层

生产 UI 使用 `prepend_below_model`：

```text
prependedLayerCount = configuredLayerCount
modelLiftMm = prependedLayerCount * layerThicknessMm
model.z += modelLiftMm
grid.layerCount = originalModelLayerCount + prependedLayerCount
base projection range = [0, prependedLayerCount)
```

模型抬高发生在 grid 和 model mask 建立前。因此新增层具有真实 Z 坐标，进入 TIFF、manifest、
preview、report 和 RIP Reader；不是只修改报告层数。旧配置缺少 `layerPlacement` 时仍走
`overlay_existing`，避免历史 fixture 的 TIFF 数量变化。

### 4.3 内存策略

只保留一张 `pixelCount` 大小的 base footprint，不建立第二套三维体：

```text
额外内存约 widthPx * heightPx bytes；
时间复杂度 O(layerCount * pixelCount)；
overlay_existing 不增加 TIFF 数量；
prepend_below_model 增加 N 张生产 TIFF，新增内存和输出耗时与 N 线性相关。
```

### 4.4 层号语义

配置层数是数量，不是最大索引：

```text
layerCount=30 -> layerIndex 0..29
human layer number -> 1..30
prepend_below_model -> 原模型 layerIndex 整体后移 30
```

所有 UI、report 和测试统一使用该定义。

## 5. 正反面 Gate

Reality 问题必须在 support base 之前关闭。建议 engine-neutral 甲片判定：

```text
先把长轴归一到 +Y；
在横向 X 的左右边带和中心带采样下包络；
若中心下包络显著低于两侧，判定 face-down；
绕长轴 Y 旋转 180 度并重新 ground 到 Z=0；
输出 frontOrientation、confidence、rotationDeg 和判定依据。
```

约束：

```text
只对满足甲片薄壳比例的候选应用；
立方体、近球体和无法判定模型保持原姿态并告警；
不根据文件名或目录特判；
显式 autoOrient=false 时不改变源姿态。
```

## 6. Report

`support_report` / `slice_report` 建议新增：

```json
{
  "baseProjection": {
    "configuredEnabled": true,
    "effectiveEnabled": true,
    "configuredLayerCount": 30,
    "effectiveLayerRange": [0, 29],
    "layerPlacement": "prepend_below_model",
    "addedLayerCount": 30,
    "modelLiftMm": 1.14,
    "source": "max_support_footprint",
    "footprintPixels": 12345,
    "printPixels": 234567
  }
}
```

每层新增：

```text
projectionBaseSupportPixels
```

方向报告新增或扩展：

```text
frontOrientation=positive_z | negative_z | indeterminate
frontOrientationAdjusted=true/false
frontOrientationEvidence.centerLowerEnvelopeMm
frontOrientationEvidence.sideLowerEnvelopeMm
```

## 7. Qt

在“切片设置 -> 支撑”加入：

```text
[x] 支撑投影铺底
铺底层数 [30] 层
```

控件修改写入 session Effective Config，并固定写入
`layerPlacement=prepend_below_model`。一键场景切片的 Legacy 生产入口消费该配置。Global
能力锁仍按其基线执行，未单独准入前不得声称支持新增物理铺底层。UI 不直接修改 support
mask。

## 8. 测试

### 单元测试

```text
空支撑 -> 空 base footprint；
不同层支撑 -> 取并集；
layerCount=0/1/30/>grid.layerCount；
模型像素优先；
具体 SupportType 优先于 ProjectionBase；
autoOrient=false 不翻转；
face-down synthetic nail 翻转；
face-up synthetic nail 保持。
```

### 集成测试

```text
历史 fixture 缺省 TIFF hash 不变；
overlay_existing fixture 保持原 TIFF 层数；
prepend_below_model fixture 总层数增加 30，0..29 层均为 projection base；
原模型首层后移到 layerIndex 30；
single material / texture profile 支撑 mask 一致；
RIP strict PASS。
```

### 真实模型

Reality 五模型每次最多切一个。先用只读方向检查验证五个模型，再选择 segment_105 做
Release Package 验证，最后才扩展到五模型矩阵。

## 9. 风险与回退

| 风险 | 缓解 |
|---|---|
| 正反面启发式误翻转通用模型 | 仅薄壳甲片生效；输出置信度；无法判断 fail-visible |
| 铺底材料量增加 | 默认 30 层可配置；报告 footprint/printPixels |
| 旧 fixture 变化 | 字段缺省兼容关闭；显式旧配置缺省 overlay_existing |
| 新增 TIFF 增加 I/O | 报告 addedLayerCount/modelLiftMm；层数由 UI 显式配置 |
| S 与 V 冲突 | 继续执行 Model > OuterVarnishShell > Support > Empty |
| 把二维洞误当三维腔体 | report 明确 `layer_enclosed_2d`，跨层策略单独 Gate |

回退只需关闭 `support.baseProjection.enabled`，不影响普通 lower/internal support 和协议。
