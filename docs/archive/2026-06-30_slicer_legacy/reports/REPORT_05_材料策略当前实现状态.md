# REPORT_05_材料策略当前实现状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 阶段范围：05 / 材料策略与白墨光油控制基础版  
> 生成时间：2026-06-08

---

## 1. 阶段结论

05 已完成 MaterialPolicy 基础实现：

```text
RGB texture only
RGB texture + W white underbase
RGB texture + V varnish top_n_layers
RGB texture + W underbase + V top_n_layers
V varnish only
W white only
```

05 未改变 RGBWSV 协议：

```text
schema = p0.rgbwsv.1
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF 通道
```

---

## 2. 配置结构

新增配置：

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

兼容规则：

```text
materialPolicy.enabled = false 时继续使用旧 modelMaterial / texture 逻辑。
materialPolicy.enabled = true 时只控制模型像素 RGB/W/V。
S 支撑仍由 support mask 生成，不由 MaterialPolicy 覆盖。
```

当前支持：

```text
rgb.source = texture_or_fallback / modelMaterial
white.mode = disabled / underbase / all_model
white.layers = all_model
varnish.mode = disabled / all_model / top_n_layers
conflictPolicy = model_material_over_support
```

---

## 3. 实现说明

05 在 `slicer_core` 中新增：

```text
MaterialPolicyConfig
RgbPolicyConfig
WhitePolicyConfig
VarnishPolicyConfig
MaterialPixel
ColumnLayerRange
MaterialPolicyReportData
```

核心输出逻辑：

```text
if model:
  write RGB/W/V from MaterialPolicy
  S = 255
else if support:
  S = 0
  RGB/W/V = 255
else:
  RGB/W/S/V = 255
```

`top_n_layers` 判定：

```text
layer >= upper_layer - topLayers + 1
```

`relief_heightfield` 路径复用 `ReliefColumnInfo.lower_layer / upper_layer`。  
`closed_mesh_scanline` 路径通过 model masks 扫描生成 column range fallback。

---

## 4. Reports

新增：

```text
reports/material_policy_report.json
```

字段：

```text
enabled
conflictPolicy
rgb.enabled / rgb.source / rgb.printPixels
white.enabled / white.mode / white.layers / white.value / white.printPixels
varnish.enabled / varnish.mode / varnish.topLayers / varnish.value / varnish.printPixels
warnings
```

manifest 已增加：

```json
"materialPolicy": "reports/material_policy_report.json"
```

`slice_report.json` 已增加：

```text
totals.materialPolicyApplied
totals.materialPolicy.rgbPrintPixels
totals.materialPolicy.whitePrintPixels
totals.materialPolicy.varnishPrintPixels
```

原有 `rgbPrintPixels / whitePrintPixels / varnishPrintPixels / supportPrintPixels` 继续保留。

---

## 5. 样例

新增小型策略 fixture：

```text
samples/models/textured/fixtures/policy_textured_small.obj
samples/models/textured/fixtures/policy_textured_small.mtl
```

该 fixture 使用：

```text
map_Kd ../textures/gradient.png
z range = 0.05..0.25 mm
support.enabled = true 时可生成 S 支撑
```

新增配置目录：

```text
samples/configs/material_policy/
```

包含：

```text
textured_rgb_only.json
textured_rgb_white_underbase.json
textured_rgb_varnish_top2.json
textured_rgb_white_varnish.json
varnish_only_all_model.json
white_only_all_model.json
```

---

## 6. 验证结果

已运行：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1
```

完整回归结果：

```text
Regression complete.
```

MaterialPolicy 样例统计：

| Package | RGB | W | V | Support | V Mode | TopLayers |
|---|---:|---:|---:|---:|---|---:|
| MaterialPolicyRgbOnly | 22560 | 0 | 0 | 5640 | disabled | 1 |
| MaterialPolicyRgbWhiteUnderbase | 22560 | 22560 | 0 | 5640 | disabled | 1 |
| MaterialPolicyRgbVarnishTop2 | 22560 | 0 | 2256 | 5640 | top_n_layers | 2 |
| MaterialPolicyRgbWhiteVarnish | 22560 | 22560 | 2256 | 5640 | top_n_layers | 2 |
| MaterialPolicyVarnishOnly | 0 | 0 | 22560 | 5640 | all_model | 1 |
| MaterialPolicyWhiteOnly | 0 | 22560 | 0 | 5640 | disabled | 1 |

`MaterialPolicyRgbWhiteVarnish` 中 V 只出现在顶部 2 层：

```text
layer 23: varnishPrintPixels = 1128
layer 24: varnishPrintPixels = 1128
```

---

## 7. 回归覆盖

`scripts/run_regression.ps1` 已覆盖：

```text
P0 ordinary
Support samples
TexturedReliefRgb
Texture fallback
MaterialPolicy six samples
Relief V/W/RGB
Bad package negative matrix
```

新增 MaterialPolicy 语义校验：

```text
RGB only: RGB > 0, W = 0, V = 0
RGB + W: RGB > 0, W > 0, V = 0
RGB + V: RGB > 0, W = 0, V > 0
RGB + W + V: RGB/W/V > 0
V only: RGB = 0, W = 0, V > 0
W only: RGB = 0, W > 0, V = 0
```

---

## 8. 当前未实现范围

05 按阶段约束未实现：

```text
ICC / 色彩管理
CMYK / RIP 半色调
真实墨量曲线
texture-driven varnish mask
texture-driven white mask
surface_shell
完整 color_shell_volume
3MF 多材料输入
OpenVDB / SDF
Qt UI
支撑形态修复
支撑小岛合并 / 剔除
```

---

## 9. 下一阶段建议

05 基础材料策略已完成，可以进入后续阶段，但不建议直接把 06 做成完整工业级 3MF / 多材料系统。

建议路线：

```text
1. 若业务优先是美甲生产效果：
   先做 05A，验证真实 textured relief 模型上的 RGB + W + V 参数组合。

2. 若业务优先是多材料文件输入：
   再进入 06，定义 3MF / 多材料输入边界，但保持输出协议仍为 p0.rgbwsv.1。

3. 若业务优先是第 68 层支撑割裂：
   不放入 05/06，另开 04B 或 08 支撑形态优化阶段。
```

当前建议：

```text
可以进入 05A 真实模型材料参数验证，或进入 06 多材料输入协议设计。
不建议在 06 直接展开 RIP 半色调、ICC、OpenVDB 或 Qt UI。
```
