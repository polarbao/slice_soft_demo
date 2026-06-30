# PRD_05_材料策略与白墨光油控制基础版

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_04A 之后  
> 所属模块：Slicer / MaterialPolicy  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

当前系统已经支持：

```text
RGBWSV uint8 TIFF
RGB 纹理采样
V 光油单材料
W 白墨单材料
S 支撑
texture fallback
support diagnostics
```

但当前材料输出仍偏向单通道或直接配置值：

```text
modelMaterial.materialChannel = RGB / W / V / auto
modelMaterial.rgb / whiteValue / varnishValue
texture.enabled
```

05 阶段目标是建立材料策略层，让 RGB / W / V 能按业务语义组合输出。

---

## 2. 产品目标

05 基础版支持以下材料策略：

```text
1. RGB texture only
2. RGB texture + W white underbase
3. RGB texture + V varnish top layers
4. RGB texture + W underbase + V top layers
5. V varnish only
6. W white only
```

支撑 S 通道继续由 support 系统生成，不属于 MaterialPolicy 的模型材料策略。

---

## 3. 核心业务语义

### 3.1 RGB

RGB 表示彩色模型材料或彩色纹理结果。

来源：

```text
texture sampled RGB
fallbackRgb
modelMaterial.rgb
```

### 3.2 W 白墨

W 可用于：

```text
white underbase
white solid model
white backing layer
```

05 只实现基础 underbase：

```text
在模型区域或纹理 RGB 区域写入 W = 0
```

### 3.3 V 光油

V 可用于：

```text
varnish solid model
varnish top layers
varnish protective layer
```

05 只实现基础：

```text
all_model
top_n_layers
```

### 3.4 S 支撑

S 继续由 support 系统控制。

MaterialPolicy 不得直接覆盖 S 支撑。

---

## 4. 推荐配置

新增：

```json
{
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
}
```

---

## 5. 策略模式

### 5.1 RGB texture only

```text
RGB 来自 texture
W = 255
V = 255
S 由 support 决定
```

### 5.2 white underbase

```text
模型区域 W = 0
RGB 仍按 texture 输出
```

注意：

```text
W underbase 与 RGB 可以在同一模型像素同时存在，因为它们是不同材料通道。
```

### 5.3 varnish top_n_layers

```text
仅在每个 XY column 的顶部 N 个模型层写 V = 0
```

用于基础表面光油。

### 5.4 varnish all_model

```text
模型所有占据层写 V = 0
```

用于单材料光油或厚光油基础验证。

---

## 6. 输出语义

继续遵守：

```text
0 = 打印
255 = 不打印
```

示例：RGB + W + V top layer 的模型像素可能为：

```text
R = sampled R
G = sampled G
B = sampled B
W = 0
S = 255
V = 0
```

支撑像素仍为：

```text
R/G/B/W/V = 255
S = 0
```

空白像素：

```text
R/G/B/W/S/V = 255
```

---

## 7. Report 需求

新增：

```text
reports/material_policy_report.json
```

字段：

```text
enabled
rgb.enabled / source
white.enabled / mode / printPixels
varnish.enabled / mode / printPixels
varnish.topLayers
conflictPolicy
warnings
```

`slice_report.json` 增加：

```text
rgbPrintPixels
whitePrintPixels
varnishPrintPixels
supportPrintPixels
materialPolicyApplied
```

---

## 8. Preview 需求

必须支持：

```text
model_rgb
white_w
varnish_v
support_s
composite_material_debug
```

`composite_material_debug` 可选。

---

## 9. 验收标准

1. RGB texture only 样例通过。
2. RGB + W underbase 样例通过。
3. RGB + V top_n_layers 样例通过。
4. RGB + W + V 样例通过。
5. V only / W only 回归不破坏。
6. S support 不被 MaterialPolicy 覆盖。
7. `material_policy_report.json` 输出。
8. `slice_report.json` 统计 W/V/RGB/S printPixels。
9. `run_regression.ps1` 通过。
10. RGBWSV 协议不变。

---

## 10. 非目标

05 不做：

```text
ICC
CMYK
RIP 半色调
真实墨量曲线
texture-driven varnish mask
texture-driven white mask
surface_shell
完整 color_shell_volume
3MF 多材料
OpenVDB
Qt UI
支撑形态修复
```

---

## 11. 结论

05 基础版的目标是建立 MaterialPolicy 层，使 RGB / W / V 能按策略组合输出，同时不破坏支撑 S 和 RGBWSV 协议。
