# DEV_12A_彩色纹理材料填充支撑光油策略设计

> 文档版本：v0.1
> 文档状态：DEV / Stage 12A
> 生成日期：2026-07-05
> 前置文档：PRD_12A_彩色纹理材料填充支撑光油策略.md

---

## 1. 技术目标

12A 技术实现要把当前隐式组合的材料行为整理为显式策略：

```text
当前隐式组合：
texture.applyMode
texture.nonSurfaceRgbPolicy
modelMaterial
materialPolicy
support.mode
varnish.top_layers

目标显式策略：
TextureSurfacePolicy
ModelFillPolicy
SupportPlacementPolicy
InternalVoidSupportPolicy
OuterVarnishShellPolicy
LayerSemanticReport
```

---

## 2. 当前代码基线

### 2.1 配置基线

当前 `src/slicer_core/config.h` 已有基础字段：

```text
TextureConfig.apply_mode
TextureConfig.top_surface_layers
TextureConfig.non_surface_rgb_policy
ModelMaterialConfig.rgb
WhitePolicyConfig.enabled/mode/value/layers
VarnishPolicyConfig.enabled/mode/value/top_layers
SupportConfig.enabled/mode/unsupported_projection/xy_dilation_px
PreviewConfig.support_pseudo_color / white_pseudo_color / varnish_pseudo_color
```

### 2.2 切片基线

当前 `src/slicer_core/slicer.cpp` 已有：

```text
support.mode = bottom_projection
support.mode = unsupported_only
support.mode = bottom_projection_plus_unsupported
support.mode = full_vertical_projection
ShouldApplyTextureToLayer
write_non_surface_texture_pixel
compose_material_policy_pixel
Model > Support > Empty
```

### 2.3 已存在但未生产化的边界

当前已存在设计入口：

```text
VarnishGeometryPolicy::AdditiveGrow
TextureApplicationPolicy::OuterSurfaceShell
```

但 legacy production path 尚未实现外侧光油壳层和真正外表面壳层纹理。

---

## 3. 设计原则

```text
1. 不修改 RGBWSV 协议；
2. 不把 support 混入 model real data；
3. 不用 UI 颜色判断生产语义；
4. legacy production path 先完成语义收敛；
5. OpenVDB 仅在 12B/后续作为候选引擎接入同一语义；
6. 配置新增字段必须有默认兼容策略，避免破坏已有 fixture；
7. report 必须解释每类像素来源。
```

---

## 4. 建议配置模型

### 4.1 ModelFillConfig

新增建议：

```json
{
  "modelFill": {
    "enabled": true,
    "material": "white",
    "scope": "below_texture_surface",
    "value": 0,
    "legacyRgbFallback": false
  }
}
```

字段说明：

```text
enabled：是否启用模型填充；
material：white | varnish | rgb | none | profile_default；
scope：solid_volume | below_texture_surface | all_model；
value：写入通道的 8-bit 打印值，默认 0；
legacyRgbFallback：兼容旧配置的 RGB 黑色填充。
```

兼容策略：

```text
1. 老配置没有 modelFill 时，保持 texture.nonSurfaceRgbPolicy 的旧行为；
2. 新 UI 生产 Profile 默认写 modelFill；
3. regression fixture 可以继续显式使用 legacyRgbFallback。
```

### 4.2 SupportPlacementConfig

建议扩展：

```json
{
  "support": {
    "enabled": true,
    "placement": "lower",
    "mode": "bottom_projection",
    "internalVoid": {
      "enabled": false,
      "minAreaPx": 16,
      "fillRule": "enclosed_by_model_envelope"
    },
    "upper": {
      "enabled": false,
      "reason": "disabled_by_default"
    }
  }
}
```

映射关系：

```text
placement=lower => bottom_projection 或 unsupported_projection；
placement=upper => 后续新增 upper_projection；
placement=both => lower + upper；
placement=unsupported_only => 当前 unsupported_only；
placement=full_vertical_projection => 当前 full_vertical_projection，标记为 advanced/debug。
```

### 4.3 OuterVarnishShellConfig

建议新增：

```json
{
  "outerVarnish": {
    "enabled": false,
    "thicknessPx": 1,
    "pixelPitchUm": 42.3,
    "conflictPolicy": "support_wins",
    "value": 0
  }
}
```

换算：

```text
thicknessMm = thicknessPx * pixelPitchUm / 1000.0
thicknessPx = ceil(thicknessMm * 1000.0 / pixelPitchUm)
```

---

## 5. 切片执行链路

建议 12A legacy pipeline：

```text
1. Load config/model/texture；
2. Transform/autoOrient；
3. Build model occupancy / layer masks；
4. Compute texture surface mask；
5. Compute model fill mask = model mask - texture surface mask；
6. Compute support mask；
7. Compute internal void support mask；
8. Compute outer varnish shell mask；
9. Compose RGBWSV by semantic priority；
10. Write layer summary/report/preview。
```

---

## 6. 像素组合规则

推荐伪代码：

```text
if modelPixel:
    if textureSurfacePixel:
        write RGB from texture
        apply optional material policy W/V
        semantic = TextureSurface
    else:
        write ModelFill according to modelFill.material
        semantic = ModelFill
else if supportPixel:
    write S
    semantic = SupportFill
else if outerVarnishPixel:
    write V
    semantic = OuterVarnishShell
else:
    write empty
    semantic = Empty
```

注意：

```text
1. 如果模型表面需要白墨底层，属于 MaterialPolicy，不改变 semantic 主分类；
2. 如果 modelFill.material=varnish，则 V 是模型填充，不是外侧壳层；
3. 如果 support 与 outerVarnish 重叠，默认 support wins。
```

---

## 7. Report 增强

layer summary 建议新增：

```json
{
  "layerIndex": 169,
  "textureSurfacePixels": 12345,
  "modelFillPixels": 67890,
  "supportPixels": 54321,
  "internalVoidSupportPixels": 1200,
  "outerVarnishPixels": 900,
  "emptyPixels": 100000,
  "modelFillMaterial": "white",
  "supportPlacement": "lower",
  "semanticWarnings": []
}
```

package summary 建议新增：

```text
modelSemanticComparable=true/false
singleMaterialConsistency=true/false
outerVarnishEnabled=true/false
internalVoidSupportEnabled=true/false
```

---

## 8. Preview 增强

12A 需要支持：

```text
1. 生产 RGB 预览：直接读取 TIFF RGB，按 black_is_print 转显示；
2. 材料语义叠加：RGB/W/S/V 用配置伪彩；
3. 像素探针：显示 R/G/B/W/S/V、semantic、sourcePolicy；
4. 图例：空白、表面颜色、模型填充、支撑、外侧光油。
```

---

## 9. 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| 改变默认填充材料导致旧 golden 失败 | 回归波动 | 新字段缺省保持 legacy，UI 新 Profile 才启用新语义 |
| internal void support 误填模型外部空白 | 材料浪费 | 只填 enclosed-by-envelope，增加面积阈值 |
| 外侧光油壳层覆盖支撑 | 工艺错误 | 默认 support_wins，并输出冲突统计 |
| 彩色与单材料 pipeline 分叉 | 维护成本高 | 统一 semantic composer |

---

## 10. 实施顺序

```text
12A-1：新增文档和配置语义，不改默认输出；
12A-2：新增 report semantic 字段；
12A-3：新增 ModelFillPolicy，legacy 默认兼容；
12A-4：新增 InternalVoidSupportPolicy；
12A-5：新增 OuterVarnishShellPolicy；
12A-6：补 UI 设置入口和 preview 图例；
12A-7：建立 fixture/golden summary。
```

---

## 11. 验证方式

基础命令：

```powershell
cmake --build build --config Debug --target slicer_cli rip_reader_test
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\obj_mtl_texture_rgb_white_varnish.json
.\build\Debug\rip_reader_test.exe --package <packageDir>
```

专项验证：

```text
1. 检查 layer summary 中 modelFillPixels/supportPixels/outerVarnishPixels；
2. 检查 LayerPreview 像素探针；
3. 比较彩色与单材料同模型的 model/support mask；
4. 检查 internal void fixture；
5. 检查外侧光油厚度 px/mm。
```
