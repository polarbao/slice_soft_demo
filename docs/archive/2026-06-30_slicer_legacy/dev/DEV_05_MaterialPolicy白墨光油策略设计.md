# DEV_05_MaterialPolicy白墨光油策略设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 适用阶段：PRD_05  
> 所属模块：Slicer / MaterialPolicy  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

在现有 `modelMaterial` 与 `texture` 基础上新增 `materialPolicy`，用于统一控制 RGB / W / V 模型材料输出。

MaterialPolicy 不负责 S 支撑。

支撑仍由 support masks 与 support generator 控制。

---

## 2. 当前基础

当前配置已有：

```text
modelMaterial.materialChannel
modelMaterial.applyMode
modelMaterial.rgb
modelMaterial.whiteValue
modelMaterial.varnishValue
texture.enabled
texture.applyMode
texture.fallbackRgb
support.*
```

05 应保持旧配置可用，并让 `materialPolicy.enabled = true` 时进入新策略路径。

---

## 3. 配置结构建议

```cpp
struct RgbPolicyConfig {
    bool enabled{true};
    std::string source{"texture_or_fallback"};
};

struct WhitePolicyConfig {
    bool enabled{false};
    std::string mode{"disabled"}; // disabled / underbase / all_model
    std::uint8_t value{0};
    std::string layers{"all_model"};
};

struct VarnishPolicyConfig {
    bool enabled{false};
    std::string mode{"disabled"}; // disabled / all_model / top_n_layers
    std::uint8_t value{0};
    int top_layers{1};
};

struct MaterialPolicyConfig {
    bool enabled{false};
    RgbPolicyConfig rgb;
    WhitePolicyConfig white;
    VarnishPolicyConfig varnish;
    std::string conflict_policy{"model_material_over_support"};
};
```

---

## 4. 数据输入

MaterialPolicy 输入：

```text
model_masks
support_masks
optional texture_color_masks / ReliefColorColumnInfo
optional column lower/upper layer info
config.materialPolicy
config.modelMaterial fallback
```

输出：

```text
per-layer RGB/W/V channel decisions
material_policy_report
```

---

## 5. Top N Layers 判定

`varnish.top_n_layers` 需要知道每个 XY column 的顶部模型层。

可复用或新增：

```cpp
struct ColumnLayerRange {
    bool has_model;
    int lower_layer;
    int upper_layer;
};
```

逻辑：

```text
if layer >= upper_layer - topLayers + 1:
    V = varnish.value
else:
    V = 255
```

---

## 6. Compose 逻辑

建议将当前模型像素写入逻辑拆成：

```cpp
MaterialPixel compose_model_material_pixel(
    const SliceConfig& config,
    const MaterialContext& ctx);
```

输出：

```cpp
struct MaterialPixel {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t w{255};
    std::uint8_t v{255};
};
```

最终 layer 写入：

```text
if model:
    write RGB/W/V from MaterialPixel, S=255
else if support:
    write S=0, RGB/W/V=255
else:
    all 255
```

保持：

```text
Model > Support > Empty
```

---

## 7. 兼容模式

如果：

```text
materialPolicy.enabled = false
```

继续使用当前：

```text
modelMaterial.materialChannel
texture.enabled
```

逻辑。

这样不会破坏 P0 / 04 / 04A 样例。

---

## 8. Report

新增：

```text
material_policy_report.json
```

建议 schema：

```json
{
  "enabled": true,
  "rgb": {
    "enabled": true,
    "source": "texture_or_fallback",
    "printPixels": 0
  },
  "white": {
    "enabled": true,
    "mode": "underbase",
    "printPixels": 0
  },
  "varnish": {
    "enabled": true,
    "mode": "top_n_layers",
    "topLayers": 2,
    "printPixels": 0
  },
  "warnings": []
}
```

---

## 9. 样例配置

新增：

```text
samples/configs/material_policy/
  textured_rgb_only.json
  textured_rgb_white_underbase.json
  textured_rgb_varnish_top2.json
  textured_rgb_white_varnish.json
  varnish_only_all_model.json
  white_only_all_model.json
```

---

## 10. 回归要求

`run_regression.ps1` 增加 material policy 正向用例，但不要让所有 heavy texture 样例进入默认快速回归。

必须保证已有：

```text
P0
Relief
Support
Texture
Fallback
Bad package
```

继续通过。

---

## 11. 不做内容

DEV_05 不做：

```text
ICC / CMYK
RIP 半色调
OpenVDB
3MF
Qt UI
texture-driven varnish mask
texture-driven white mask
support morphology
```

---

## 12. 结论

DEV_05 的重点是把 RGB/W/V 材料输出从“分散配置”提升为“策略层”，但仍保持实现范围可控。
