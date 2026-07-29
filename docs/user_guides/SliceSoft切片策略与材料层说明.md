# SliceSoft 切片策略与材料层说明

> 日期：2026-07-04
> 适用程序：`slicer_cli`、`slicer_debug_ui`
> 适用阶段：Stage 11B 后的当前实现
> 文档定位：面向使用者和调试者解释当前切片策略、材料通道、白墨/光油/支撑、层厚、填充和 UI 配置项。

---

## 1. 当前固定输出协议

当前生产输出仍固定为 RGBWSV 六通道 TIFF package：

```text
manifest schema = p0.rgbwsv.2
channelOrder    = R G B W S V
bitDepth        = 8
polarity        = black_is_print
printValue      = 0
emptyValue      = 255
storageMode     = stripped 或 tiled
```

通道含义：

| 通道 | 名称 | 当前用途 | 打印判断 |
|---|---|---|---|
| R | 红色 | RGB 彩色/模型实体 | `< 255` 表示打印 |
| G | 绿色 | RGB 彩色/模型实体 | `< 255` 表示打印 |
| B | 蓝色 | RGB 彩色/模型实体 | `< 255` 表示打印 |
| W | 白墨 | 白墨底层或白墨材料 | `< 255` 表示打印 |
| S | 支撑 | 支撑材料 | `< 255` 表示打印 |
| V | 光油 | 光油/透明涂层 | `< 255` 表示打印 |

重要解释：

```text
(255,255,255,255,255,255) = 完全空白
(0,0,0,255,255,255)       = RGB 黑色模型打印
(255,255,255,0,255,255)   = 白墨打印
(255,255,255,255,0,255)   = 支撑打印
(255,255,255,255,255,0)   = 光油打印
```

Photoshop 或普通图像软件只看 RGB 前三通道时，无法区分：

```text
RGB 白色但 S=0 的像素：实际是支撑；
RGB 白色但 W=0 的像素：实际是白墨；
RGB 白色且全通道 255：才是空白。
```

因此调试时应优先使用 UI 的生产 RGB 预览和六通道像素探针，而不是只看 Photoshop 的 RGB 视图。

---

## 2. 切片层厚与材料层厚

### 2.1 几何层厚

每一层的几何厚度来自：

```json
"output": {
  "dpiX": 635,
  "dpiY": 600,
  "layerThicknessMm": 0.038
}
```

含义：

```text
layerIndex = 0      => zMm = 0.019 附近的底部第一层
layerIndex = N      => zMm 约为 (N + 0.5) * layerThicknessMm
总层数               => 由模型高度 / layerThicknessMm 推导
```

当前产品默认使用 X/Y `635/600 dpi`，并采用
`layerThicknessMm=0.038mm`，也就是每层 38 微米。测试 fixture
可以显式使用其他数值，且不会被编译过程改写。

### 2.2 RGB/W/S/V 是否有独立厚度

当前软件没有为 RGB、白墨、支撑、光油分别设置独立物理层厚。所有材料通道都落在同一个 `layerIndex` 的 TIFF 六通道像素中。

可以理解为：

```text
几何 Z 分层：由 layerThicknessMm 决定；
材料是否打印：由该层的 R/G/B/W/S/V 通道值决定；
材料“覆盖多少层”：由策略决定，例如 varnish.topLayers。
```

### 2.3 光油顶部层数换算

若配置：

```json
"materialPolicy": {
  "varnish": {
    "enabled": true,
    "mode": "top_n_layers",
    "topLayers": 2
  }
}
```

且：

```json
"output": {
  "layerThicknessMm": 0.01
}
```

则光油覆盖厚度近似为：

```text
光油覆盖层数 = 2 层
几何厚度     = 2 * 0.01mm = 0.02mm
```

这只是当前切片输出层面的几何解释，不等于真实喷墨固化后的最终膜厚；真实膜厚还取决于喷头、墨量、RIP、固化和材料工艺。

---

## 3. 模型摆放与切片方向

当前 UI 一键切片默认启用自动摆放：

```json
"autoOrient": {
  "enabled": true,
  "maxHeightMm": 6.0,
  "strategy": "minimize_height_by_right_angle_rotation"
}
```

目标：

```text
将指甲类模型尽量“趴放”；
高度控制在 6mm 以内；
减少用户在无 3D 视图时因模型竖放导致切片异常。
```

切片索引约定：

```text
layerIndex 小：靠近底部；
layerIndex 大：靠近顶部；
UI 预览已按切片坐标显示，避免 Qt 图像坐标导致上下颠倒。
```

如果视觉上仍像“从上往下切”，优先检查：

```text
1. 当前看的是否是 texture_rgb preview，而不是 production_rgb；
2. 是否选择了旧输出包；
3. preview 是否跨层合成；
4. 纹理策略是否把顶面纹理投影到了实体体积。
```

---

## 4. RGB 彩色策略

### 4.1 基础模型材料

基础模型材料由 `modelMaterial` 控制：

```json
"modelMaterial": {
  "materialChannel": "RGB",
  "applyMode": "solid_volume",
  "rgb": [0, 0, 0],
  "whiteValue": 255,
  "varnishValue": 255
}
```

当前含义：

| 字段 | 含义 |
|---|---|
| `materialChannel=RGB` | 模型实体默认写入 RGB 通道 |
| `applyMode=solid_volume` | 模型体素/高度场内的实体区域都视为模型材料 |
| `rgb=[0,0,0]` | 默认模型填充为 RGB 黑色打印 |
| `whiteValue=255` | 默认不写白墨 |
| `varnishValue=255` | 默认不写光油 |

### 4.2 纹理策略

纹理由 `texture` 控制：

```json
"texture": {
  "enabled": true,
  "applyMode": "top_surface_band",
  "topSurfaceLayers": 50,
  "sampler": "bilinear",
  "uvAddressMode": "clamp",
  "flipV": true,
  "fallbackRgb": [0, 0, 0],
  "missingTexturePolicy": "warn_and_fallback",
  "nonSurfaceRgbPolicy": "model_material"
}
```

当前支持的主要策略：

| 策略 | UI 中文 | 含义 | 适用场景 |
|---|---|---|---|
| `top_surface_band` | 顶面纹理带 | 只在每列模型顶部若干层写贴图颜色 | 指甲贴图、避免顶面花纹贯穿实体 |
| `solid_volume_from_top_surface` | 顶面纹理投影到实体 | 将顶面采样到的纹理颜色沿列投影到实体体积 | 早期兼容策略，可能导致低层出现顶面花纹 |
| `solid_volume` | 实体填充 | 整个实体使用纹理/颜色 | 简化模型或调试 |
| `surface_shell_from_sdf` | SDF 表面壳层 | OpenVDB candidate 的表面壳层纹理路径 | 实验路径，非默认生产 |
| `disabled` | 禁用纹理 | 不采样贴图 | 单色/纯材料模型 |

### 4.3 非表面 RGB 策略

`top_surface_band` 只给顶部表面带写贴图颜色。表面带之外的模型实体区域由：

```json
"texture": {
  "nonSurfaceRgbPolicy": "model_material"
}
```

决定。

| 策略 | UI 中文 | 行为 |
|---|---|---|
| `model_material` | 使用模型材料 | 使用 `modelMaterial.rgb` 写入模型实体 RGB |
| `empty` | 视为空白 | RGB 写 255，不打印 RGB |
| `fallback_rgb` | 使用备用 RGB | 使用 `texture.fallbackRgb` 写 RGB |
| `material_policy` | 交给材料策略 | 当前基础版等价于 `model_material`，保留后续工艺插入点 |

解释一个常见现象：

```text
如果 modelMaterial.rgb=[0,0,0] 且 nonSurfaceRgbPolicy=model_material，
那么 TIFF RGB 中非表面纹理区域会是黑色打印。
这不是空白，也不是错误，而是模型实体填充。
```

如果产品希望非表面实体不打印 RGB，应使用：

```json
"texture": {
  "nonSurfaceRgbPolicy": "empty"
}
```

### 4.4 模型内部填充与纹理的互斥边界

`modelFill` 用于填充颜色表层之间的模型实体区域。选择白墨时写 W 通道，选择光油时写 V 通道：

```json
"modelFill": {
  "enabled": true,
  "material": "white",
  "scope": "below_texture_surface",
  "value": 0,
  "emptyAllowedInProduction": false
}
```

生产彩色甲片应配合：

```json
"texture": {
  "enabled": true,
  "applyMode": "top_surface_band",
  "topSurfaceLayers": 1
}
```

不允许把 `below_texture_surface` 与 `solid_volume_from_top_surface` 组合使用。后者把每一层模型像素都标记为纹理区域，模型内部填充因没有剩余区域而得到 `modelFillPixels=0`、`whitePrintPixels=0`。Qt 一键切片会在生成会话生效配置时把该冲突组合纠正为 1 层 `top_surface_band`，CLI 配置仍应显式写对。

UI 中的“叠加白墨底层”属于 `materialPolicy.white`，它不是 `modelFill.material=white` 的替代项。前者是可选的全模型白墨叠加策略，后者才是颜色层之间的模型内部填充。

---

## 5. 多材料选择与材料角色映射

多材料输入主要用于 OBJ/MTL 和 3MF。

### 5.1 材料角色

`materialRoleMapping` 可把输入材料映射到 RGB/W/V/S 等角色：

```json
"materialRoleMapping": {
  "enabled": true,
  "mode": "rules_then_default",
  "defaultRole": "rgb",
  "allowInputSupportMaterial": false,
  "rules": [
    {
      "matchNameContains": "white",
      "role": "white"
    }
  ]
}
```

当前角色：

| role | UI 中文 | 输出通道 |
|---|---|---|
| `rgb` | RGB 彩色 | R/G/B |
| `white` | 白墨 | W |
| `varnish` | 光油 | V |
| `support` | 支撑 | S |
| `support_candidate` | 支撑候选 | 当前不直接写生产 S |
| `ignore` | 忽略 | 不写材料 |

默认策略：

```text
没有匹配规则的材料进入 defaultRole；
默认 defaultRole=rgb；
allowInputSupportMaterial=false 时，不建议让输入模型直接决定生产支撑。
```

### 5.2 OBJ/MTL

OBJ 多材料依赖：

```text
OBJ face 的 usemtl；
MTL 材料名；
MTL diffuse / map_Kd；
规则中的 matchNameContains。
```

典型用法：

```text
带贴图的甲面材料 => rgb；
命名包含 white 的材料 => white；
命名包含 varnish 或 gloss 的材料 => varnish；
支撑通常不要由 OBJ 输入材料直接决定，而由支撑策略生成。
```

### 5.3 3MF

3MF 当前支持基础：

```text
BaseMaterial；
ColorGroup；
Texture2DGroup；
component transform；
stored / deflate package。
```

但 3MF 的复杂材质、生产支撑树、RIP 半色调不在当前范围内。

---

## 6. MaterialPolicy：白墨、光油和 RGB 的生产策略

`materialPolicy` 是当前更接近生产策略的配置段：

```json
"materialPolicy": {
  "enabled": true,
  "rgb": {
    "enabled": true,
    "source": "texture_or_fallback"
  },
  "white": {
    "enabled": true,
    "mode": "underbase",
    "value": 0,
    "layers": "all_model"
  },
  "varnish": {
    "enabled": true,
    "mode": "top_n_layers",
    "value": 0,
    "topLayers": 2
  },
  "conflictPolicy": "model_material_over_support"
}
```

### 6.1 RGB 来源

| source | UI 中文 | 行为 |
|---|---|---|
| `texture_or_fallback` | 纹理或备用色 | 优先用贴图，缺失时用 fallbackRgb |
| `modelMaterial` | 模型材料 | 使用 `modelMaterial.rgb` |

### 6.2 白墨策略

| mode | UI 中文 | 行为 |
|---|---|---|
| `disabled` | 禁用 | W 通道保持 255 |
| `underbase` | 白墨底层 | 模型区域写 W 值，当前用于 RGB 下方白墨底 |
| `all_model` | 覆盖整个模型 | 模型区域写 W 值 |

当前限制：

```text
materialPolicy.white.layers 当前只支持 all_model；
white.value=0 表示满量打印；
white.value=255 表示不打印。
```

### 6.3 光油策略

| mode | UI 中文 | 行为 |
|---|---|---|
| `disabled` | 禁用 | V 通道保持 255 |
| `all_model` | 覆盖整个模型 | 模型区域所有层写 V |
| `top_n_layers` | 顶部 N 层 | 每个 XY 列的顶部 N 个模型层写 V |

`top_n_layers` 是当前最常用的指甲光油策略：

```text
每个 XY 列先计算该列模型的 lowerLayer / upperLayer；
upperLayer 往下 topLayers 层写光油；
不同高度的异形/浮雕区域会各自按本列顶部计算。
```

---

## 7. MaterialProcessProfile：工艺验收 Profile

`materialProcessProfile` 当前主要用于报告和验收，不是独立的几何切片引擎。

示例：

```json
"materialProcessProfile": {
  "enabled": true,
  "name": "nail_rgb_white_varnish_top2",
  "target": "uv_relief_nail",
  "validation": {
    "requireRgbPixels": true,
    "requireWhitePixels": true,
    "requireVarnishPixels": true,
    "requireSupportPixels": true
  }
}
```

它用于回答：

```text
这个输出包是否有 RGB？
是否有白墨？
是否有光油？
是否有支撑？
光油 activeLayerIndices 是否符合 topLayers？
是否存在不期望的重叠？
```

对应报告：

```text
reports/material_process_report.json
```

---

## 8. 支撑策略

支撑由 `support` 配置段生成：

```json
"support": {
  "enabled": true,
  "mode": "bottom_projection",
  "value": 0,
  "minOverlapRatio": 0.2,
  "minIslandAreaPx": 16,
  "connectivity": 8,
  "xyDilationPx": 0
}
```

支撑写入：

```text
模型像素优先；
如果当前位置不是模型，且 support_mask=1，则写 S=value；
默认 value=0，表示支撑满量打印；
其他通道保持 255。
```

### 8.1 支撑模式

| mode | UI 中文 | 行为 |
|---|---|---|
| `none` | 不生成支撑 | 不写 S 通道 |
| `bottom_projection` | 底面投影支撑 | 每个 XY 列从构建底板到该列第一层模型之间填 S |
| `unsupported_only` | 仅悬空支撑 | 检测悬空孤岛后向下投影支撑 |
| `bottom_projection_plus_unsupported` | 底面 + 悬空支撑 | 同时启用底面投影和悬空孤岛支撑 |
| `full_vertical_projection` | 全高度垂直填充 | 每个 XY 列从底部到该列最后模型层下方都可填 S |
| `island_filter` | 孤岛过滤 | 历史/诊断概念，当前不作为普通 UI 生产选项展示 |

注意：

```text
当前 core validator 接受 bottom_projection、unsupported_only、bottom_projection_plus_unsupported、full_vertical_projection；
island_filter 不是当前完整生产 mode，新配置不应选择它。
```

### 8.2 中间填充与“填充材料”

当前没有单独的“填充材料”通道。

常见的“填充”分两类：

```text
模型实体填充：写入 RGB 通道，来自 modelMaterial、texture 或 materialPolicy；
支撑填充：写入 S 通道，来自 support 生成策略。
```

对于甲片模型：

```text
两侧真实模型区域：通常写 RGB/W/V；
中间空腔或底面下方需要承托区域：通常写 S；
如果中间区域在 Photoshop RGB 看起来白色，不能直接判断为空白，应看 S 通道或 UI 六通道探针。
```

六通道判断：

```text
(255,255,255,255,0,255) = 支撑填充；
(0,0,0,255,255,255)     = RGB 模型实体填充；
(255,255,255,255,255,255) = 真空白。
```

---

## 9. relief_heightfield 与浮雕/甲片模型

当前指甲/浮雕类模型主要走：

```json
"slicingMode": "relief_heightfield"
```

关键字段：

```json
"relief": {
  "fillMode": "intersection_range",
  "baseZMm": 0.0
}
```

含义：

```text
按 XY 列统计模型覆盖的 lowerLayer / upperLayer；
实体范围内生成模型 mask；
top_surface_band / top_n_layers 都基于每列 lower/upper 层判断；
对异形浮雕，光油和纹理顶部带会随局部高度变化。
```

这就是为什么浮雕表面光油不是简单全局最后几层，而是每个 XY 列按自身顶部范围计算。

---

## 10. Preview 与生产 TIFF 的区别

当前输出包含两类图像：

```text
layers/layer_000123.tiff      生产数据，六通道 RGBWSV；
preview/*.png 或 *.ppm        调试预览图，不等于生产协议。
```

UI 中常见预览：

| UI 视图 | 数据来源 | 用途 |
|---|---|---|
| 生产 RGB | `layers/*.tiff` 的 R/G/B | 检查真实生产 RGB |
| 纹理 RGB | `preview/texture_rgb_*` | 检查贴图表面颜色 |
| 支撑伪彩 | S 通道或 support preview | 用绿色显示 S 打印区域 |
| 白墨伪彩 | W 通道或 white preview | 用配置色显示 W 打印区域 |
| 光油伪彩 | V 通道或 varnish preview | 用灰色显示 V 打印区域 |
| 叠加预览 | RGB + W/S/V 伪彩 | 观察材料空间关系 |

因此：

```text
texture_rgb 里的白色不一定是空白；
Photoshop RGB 里的白色也不一定是空白；
真实判断必须看 RGBWSV 六通道。
```

---

## 11. 当前 UI 中文选项映射

为了减少用户直接面对英文协议值，UI 当前采用“中文显示 + 英文协议值保存”的方式。

示例：

| UI 中文 | 保存到 JSON |
|---|---|
| 顶面纹理带 | `top_surface_band` |
| 顶面纹理投影到实体 | `solid_volume_from_top_surface` |
| 使用模型材料 | `model_material` |
| 视为空白 | `empty` |
| 白墨底层 | `underbase` |
| 顶部 N 层 | `top_n_layers` |
| 底面投影支撑 | `bottom_projection` |
| 全高度垂直填充 | `full_vertical_projection` |
| RGB 彩色 | `rgb` |
| 白墨 | `white` |
| 光油 | `varnish` |
| 支撑 | `support` |

这样做的原因：

```text
用户界面更容易理解；
配置文件仍保持稳定协议；
CLI、测试脚本、历史样例不需要迁移。
```

---

## 12. 推荐配置组合

### 12.1 OBJ 彩色纹理甲片

```text
texture.enabled = true
texture.applyMode = top_surface_band
texture.nonSurfaceRgbPolicy = model_material 或 empty
materialPolicy.enabled = false 或按工艺开启
support.enabled = true
support.mode = bottom_projection 或 full_vertical_projection
```

如果希望非表面实体不打印黑色 RGB：

```text
texture.nonSurfaceRgbPolicy = empty
```

### 12.2 OBJ 彩色 + 白墨 + 顶部光油

```text
materialPolicy.enabled = true
rgb.source = texture_or_fallback
white.enabled = true
white.mode = underbase
white.value = 0
varnish.enabled = true
varnish.mode = top_n_layers
varnish.topLayers = 2 或 3
varnish.value = 0
support.enabled = true
```

### 12.3 单材料光油浮雕

```text
texture.enabled = false
modelMaterial.materialChannel = V
modelMaterial.varnishValue = 0
support.enabled = true
```

### 12.4 支撑诊断

```text
support.mode = bottom_projection
support.connectivity = 8
support.minIslandAreaPx = 16
preview.channels 包含 support
```

---

## 13. 当前限制和后续改进方向

当前限制：

```text
1. 没有独立“填充材料”通道；
2. RGB/W/S/V 没有独立真实物理膜厚模型；
3. MaterialProcessProfile 更偏报告验收，不是完整工艺求解器；
4. OpenVDB candidate 仍不满足 legacy 替代 gate；
5. island_filter 在 UI 中可见，但 core validator 当前尚未作为完整生产 mode 接收。
```

建议后续：

```text
1. 将 materialPolicy.white.layers 从 all_model 扩展到可配置层范围；
2. 将光油策略扩展为表面法线/曲率/区域 mask 控制；
3. 增加显式 fillMaterialPolicy，区分 RGB 实体填充、支撑填充和未来填充材料；
4. 将支撑孤岛过滤继续沉淀为 unsupported_only 的参数，而不是暴露成单独生产 mode；
5. 将生产 RGB、纹理 RGB、W/S/V 伪彩图例固化到 UI。
```
