# DEV_12A_彩色纹理材料填充支撑光油策略设计

> 文档版本：v0.2
> 文档状态：DEV / Stage 12A
> 生成日期：2026-07-05
> 更新日期：2026-07-06
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
7. report 必须解释每类像素来源；
8. 生产 Profile 中模型内部填充不允许为空；
9. 外侧光油壳层允许扩张 XY，且优先级高于支撑。
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
    "emptyAllowedInProduction": false,
    "legacyRgbFallback": false
  }
}
```

字段说明：

```text
enabled：是否启用模型填充；
material：white | varnish | rgb | profile_default | material_role；
scope：solid_volume | below_texture_surface | all_model；
value：写入通道的 8-bit 打印值，默认 0；
emptyAllowedInProduction：生产 Profile 固定为 false；
legacyRgbFallback：兼容旧配置的 RGB 黑色填充。
```

兼容策略：

```text
1. 老配置没有 modelFill 时，保持 texture.nonSurfaceRgbPolicy 的旧行为；
2. 新 UI 生产 Profile 默认写 modelFill.material=white；
3. regression fixture 可以继续显式使用 legacyRgbFallback；
4. 诊断 fixture 如果需要空填充，必须标记为 non-production，不进入生产 Profile。
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
      "enabled": true,
      "minAreaPx": 16,
      "fillRule": "all_internal_voids"
    },
    "upper": {
      "enabled": false,
      "outside": "outer_varnish_shell",
      "reason": "optional_detachable_surface_support"
    }
  }
}
```

映射关系：

```text
placement=lower => bottom_projection 或 unsupported_projection；
placement=upper => 上表面外部可剥离支撑；如启用 outerVarnish，则生成在外侧光油壳层之外；
placement=both => lower + upper；
placement=unsupported_only => 当前 unsupported_only；
placement=full_vertical_projection => 当前 full_vertical_projection，标记为 advanced/debug。
internalVoid.enabled 默认 true，生产 Profile 中内部镂空一律写 S 支撑。
```

### 4.3 OuterVarnishShellConfig

建议新增：

```json
{
  "outerVarnish": {
    "enabled": false,
    "thicknessMm": 0.0,
    "thicknessStepMm": 0.01,
    "pixelPitchUm": 42.3,
    "allowXYExpansion": true,
    "conflictPolicy": "varnish_shell_wins",
    "value": 0
  }
}
```

换算：

```text
thicknessPx = ceil(thicknessMm * 1000.0 / pixelPitchUm)
effectiveThicknessMm = thicknessPx * pixelPitchUm / 1000.0
thicknessMm = 0.0 表示不生成外侧光油壳层
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
6. Compute outer varnish shell mask；
7. Compute support mask；
8. Compute internal void support mask；
9. For upper support, use model envelope + outer varnish shell as outside boundary；
10. Compose RGBWSV by semantic priority；
11. Write layer summary/report/preview。
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
else if outerVarnishPixel:
    write V
    semantic = OuterVarnishShell
else if supportPixel:
    write S
    semantic = SupportFill
else:
    write empty
    semantic = Empty
```

注意：

```text
1. 如果模型表面需要白墨底层，属于 MaterialPolicy，不改变 semantic 主分类；
2. 如果 modelFill.material=varnish，则 V 是模型内部填充，不是外侧壳层；
3. 模型本体与支撑冲突时保持 Model > Support；
4. outerVarnish 与 support 重叠时 outerVarnish wins；
5. upper support 应在 outerVarnish shell 之外生成，避免同像素冲突；
6. internal void support 是 supportPixel，但 reason 必须标记为 internal_void。
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
  "upperSurfaceSupportPixels": 0,
  "emptyPixels": 100000,
  "modelFillMaterial": "white",
  "supportPlacement": "lower",
  "internalVoidSupportDefault": true,
  "outerVarnishThicknessMm": 0.0,
  "outerVarnishThicknessPx": 0,
  "semanticWarnings": []
}
```

package summary 建议新增：

```text
modelSemanticComparable=true/false
singleMaterialConsistency=true/false
outerVarnishEnabled=true/false
internalVoidSupportEnabled=true/false
semanticPriority="Model>OuterVarnishShell>Support>Empty"
singleMaterialAndColorConsistency=true/false
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
| 外侧光油壳层与支撑重叠 | 材料边界不清 | 默认 varnish_shell_wins；上表面支撑生成在光油壳层之外，并输出冲突统计 |
| 彩色与单材料 pipeline 分叉 | 维护成本高 | 统一 semantic composer |

---

## 10. 实施顺序

```text
12A-1：新增文档和配置语义，不改默认输出；
12A-2：新增 report semantic 字段；
12A-3：新增 ModelFillPolicy，legacy 默认兼容；
12A-4：新增 InternalVoidSupportPolicy；
12A-5：新增 OuterVarnishShellPolicy，支持 thicknessMm、0.01mm 精度、42.3um/px 换算和 XY 扩张；
12A-6：新增 UpperSurfaceSupportPolicy，确保上表面支撑在外侧光油壳层之外；
12A-7：补 UI 设置入口和 preview 图例；
12A-8：建立 fixture/golden summary。
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
5. 检查外侧光油厚度 mm/px 换算；
6. 检查上表面支撑是否位于外侧光油壳层之外。
```
