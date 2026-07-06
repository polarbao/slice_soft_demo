# DEMO_12A_彩色纹理材料支撑光油验证方案

> 文档版本：v0.2
> 文档状态：DEMO / Stage 12A
> 生成日期：2026-07-05
> 更新日期：2026-07-06

---

## 1. 验证目标

验证 12A 的材料语义是否可被用户理解、被 report 解释、被 UI 检查。

---

## 2. 验证模型

建议模型集：

| 模型 | 用途 |
|---|---|
| `model/obj/nai_you_new` | 标准彩色纹理甲片 |
| `model/obj/aishen_fudiao` | 不规则浮雕、高 Z 局部区域支撑验证 |
| `samples/models/relief/relief.obj` | 单材料浮雕基线 |
| 内部镂空 fixture | internalVoidSupport 验证 |
| 外侧光油 fixture | outerVarnishShell 厚度验证 |

---

## 3. 验证场景

### Case 12A-01 彩色纹理 + 白墨填充

配置：

```text
texture.applyMode = surface/top_surface_band
modelFill.material = white
support.placement = lower
support.internalVoid.enabled = true
outerVarnish.enabled = false
```

期望：

```text
TextureSurface 写 RGB；
ModelFill 写 W；
Support 写 S；
非打印区域全 255；
report 显示 modelFillMaterial=white。
生产 Profile 不允许 ModelFill 为空。
```

### Case 12A-02 彩色纹理 + 光油填充

配置：

```text
modelFill.material = varnish
support.internalVoid.enabled = true
```

期望：

```text
ModelFill 写 V；
RGB 不再用黑色隐式填充；
LayerPreview 可区分模型填充光油和外侧光油。
```

### Case 12A-03 中间镂空支撑

配置：

```text
support.internalVoid.enabled = true
support.internalVoid.fillRule = all_internal_voids
```

期望：

```text
被模型外轮廓包围的空洞写 S；
模型外部空白保持 255；
report 显示 internalVoidSupportPixels > 0；
internalVoidSupport 默认开启，生产 Profile 不应关闭。
```

### Case 12A-04 不规则浮雕支撑

配置：

```text
support.placement = unsupported_only 或 bottom_projection_plus_unsupported
```

期望：

```text
aishen_fudiao 高 Z 局部悬空区域有支撑原因；
support_report 中出现 unsupported_island / high_z_overhang；
支撑不被光油或 RGB 覆盖。
```

### Case 12A-05 外侧光油壳层

配置：

```text
outerVarnish.enabled = true
outerVarnish.thicknessMm = 0.05
outerVarnish.thicknessStepMm = 0.01
outerVarnish.pixelPitchUm = 42.3
outerVarnish.allowXYExpansion = true
outerVarnish.conflictPolicy = varnish_shell_wins
```

期望：

```text
模型外轮廓向 XY 外侧扩张并产生 V 通道壳层；
report 显示 thicknessMm、thicknessPx 和 effectiveThicknessMm；
thicknessPx = ceil(thicknessMm * 1000 / 42.3)；
support 与 outerVarnish 冲突时 OuterVarnishShell wins。
```

### Case 12A-06 外侧光油 + 上表面支撑

配置：

```text
outerVarnish.enabled = true
outerVarnish.thicknessMm = 0.05
support.placement = upper
support.upper.enabled = true
```

期望：

```text
先生成外侧光油壳层；
上表面支撑生成在外侧光油壳层之外；
同像素冲突时执行 Model > OuterVarnishShell > Support > Empty；
report 显示 upperSurfaceSupportPixels > 0。
```

### Case 12A-07 彩色/单材料一致性

同一模型分别使用彩色 Profile 和单材料 Profile。

期望：

```text
layerCount 一致；
model mask 可比较；
support mask 可比较；
几何轮廓和通道统计逻辑可比较；
材料通道差异符合 Profile：彩色模型写 RGB 纹理，单材料模型写单色材料。
```

---

## 4. 验证输出

每个 Case 需要保留：

```text
package/manifest.json
package/reports/slice_report.json
package/reports/support_report.json
package/reports/material_report.json
package/preview/*.png 或 *.ppm
关键 layer TIFF
UI 截图或 smoke log
```

---

## 5. 通过标准

```text
1. rip_reader_test PASS；
2. schema/polarity/channelOrder 不变；
3. report 中每类像素可解释；
4. UI 图例和像素探针能解释白色/黑色/绿色/灰色伪彩；
5. 彩色与单材料一致性报告通过；
6. 未启用新策略的旧 fixture 不发生非预期输出变化。
```
