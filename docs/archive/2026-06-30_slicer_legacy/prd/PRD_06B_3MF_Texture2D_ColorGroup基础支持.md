# PRD_06B_3MF_Texture2D_ColorGroup基础支持

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_06A 之后  
> 所属模块：Slicer / 3MF Importer / Texture / ColorGroup  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

06 阶段已经支持：

```text
3MF / OBJ-MTL material role mapping
3MF basematerials / displaycolor 基础解析
OBJ/MTL texture RGB 输入
MaterialPolicy RGB/W/V
p0.rgbwsv.2 输出
```

06A 阶段已经增强：

```text
3MF stored / deflate package 读取
3MF validation
bad 3MF package
three_mf_report 增强
```

但当前 3MF 仍不支持：

```text
3MF ColorGroup
3MF Texture2D
3MF Texture2DGroup
```

这些是很多彩色 3MF 文件的重要颜色来源，因此 06B 需要补充。

---

## 2. 产品目标

06B 目标：

```text
让 3MF 输入支持基础 ColorGroup 与 Texture2DGroup，
并将其映射到当前 RGB 通道与 MaterialRoleMapping 体系。
```

完整链路：

```text
3MF package
→ ColorGroup / Texture2D / Texture2DGroup
→ triangle property / UV resolve
→ RGB color source
→ MaterialRoleMapping
→ current slicing / texture / material pipeline
→ p0.rgbwsv.2 SlicePackage
```

---

## 3. 用户场景

### 3.1 3MF ColorGroup 彩色模型

```text
输入：3MF colorgroup + triangle property
行为：按 ColorGroup 的颜色写 RGB
输出：RGBWSV TIFF
```

### 3.2 3MF Texture2DGroup 彩色模型

```text
输入：3MF texture2d + texture2dgroup
行为：按三角面 UV 采样 texture
输出：RGB 纹理切片
```

### 3.3 3MF Basematerial + Texture 混合模型

```text
输入：部分 triangle 来自 basematerial，部分来自 texture2dgroup
行为：按 property source 分别 resolve
输出：RGB/W/V/S
```

### 3.4 不支持的高级材质

```text
输入：PBR / CompositeMaterials / MultiProperties
行为：记录 unsupportedResources，使用 fallback，不崩溃
```

---

## 4. 必须支持范围

### 4.1 ColorGroup

必须支持：

```text
<colorgroup id="...">
  <color color="#RRGGBB" />
</colorgroup>
```

需要支持：

```text
triangle pid / p1 / p2 / p3 或 pindex 关联颜色
```

第一版可以将三角面的三个顶点颜色简化为：

```text
如果 p1/p2/p3 相同：使用该颜色；
如果 p1/p2/p3 不同：使用三者平均色或第一个颜色，并在 report 中记录 interpolatedColorFallback。
```

---

### 4.2 Texture2D

必须支持：

```text
<texture2d id="..." path="/3D/Textures/xxx.png" contenttype="image/png" />
```

路径必须从 3MF package 内部读取，不访问外部文件。

支持贴图格式以当前 TextureImage 能力为准。

当前 Windows 构建可复用 WIC 图片解码。

---

### 4.3 Texture2DGroup

必须支持：

```text
<texture2dgroup id="..." texid="...">
  <tex2coord u="..." v="..." />
</texture2dgroup>
```

triangle 通过：

```text
pid = texture2dgroup id
p1 / p2 / p3 = tex2coord index
```

解析 UV 后复用：

```text
TextureSampler
```

---

## 5. XML Parser 要求

06B 推荐引入：

```text
tinyxml2
```

如果暂不引入 tinyxml2，则必须增强 `ThreeMfXmlReader`：

```text
namespace/local-name 处理
self-closing tag
嵌套 block
attribute entity decode
错误定位
```

禁止：

```text
外部实体
外部 DTD
网络访问
```

---

## 6. 与 MaterialRoleMapping 的关系

ColorGroup / Texture2DGroup 默认 role：

```text
rgb
```

如果关联的 3MF object / material name 命中 MaterialRoleMapping：

```text
white / varnish / ignore
```

则按 role 处理。

第一版建议：

```text
ColorGroup / Texture2DGroup 主要作为 RGB source；
white/varnish 仍优先由 material name / role rule 决定。
```

---

## 7. 输出语义

继续遵守：

```text
schema = p0.rgbwsv.2
RGBWSV
uint8
black_is_print
```

ColorGroup：

```text
RGB = resolved color
W/S/V = 255
```

Texture2DGroup：

```text
RGB = sampled texture RGB
W/S/V = 255
```

如果 MaterialPolicy overlay 开启：

```text
可继续叠加 W underbase / V top_n_layers
```

---

## 8. Report 需求

`three_mf_report.json` 增强：

```text
colorGroupCount
colorCount
texture2dCount
texture2dGroupCount
tex2CoordCount
textureResourceCount
textureLoadedCount
textureMissingCount
textureSampledPixels
colorGroupResolvedTriangles
textureGroupResolvedTriangles
interpolatedColorFallbackCount
unsupportedResources
warnings
```

`texture_report.json` 应记录来自 3MF package 内部的 texture：

```text
source = 3mf_internal
path = 3D/Textures/xxx.png
```

---

## 9. Fallback 策略

| 场景 | 行为 |
|---|---|
| texture2d path 缺失 | warning + fallbackRgb |
| texture decode 失败 | warning + fallbackRgb |
| tex2coord index 越界 | error 或 fallback，建议 error |
| color index 越界 | error 或 fallback，建议 error |
| unsupported CompositeMaterials | warning + fallback |
| unsupported MultiProperties | warning + fallback |

---

## 10. 验收标准

1. 3MF ColorGroup 样例可切片。
2. ColorGroup RGB printPixels > 0。
3. 3MF Texture2DGroup 样例可切片。
4. Texture2DGroup textureSampledPixels > 0。
5. `texture_report.source = 3mf_internal`。
6. `three_mf_report` 记录 color/texture group 统计。
7. unsupported CompositeMaterials / MultiProperties 被 report 记录。
8. bad texture path 能 warning/fallback。
9. tex2coord 越界能报错。
10. `rip_reader_test --summary` 通过。
11. `run_regression.ps1 -Mode quick` 通过。
12. p0.rgbwsv.2 输出协议不变。

---

## 11. 非目标

06B 不做：

```text
完整 PBR；
metallic / roughness；
ICC / CMYK；
RIP 半色调；
OpenVDB / SDF；
Qt UI；
Production Extension 完整语义；
Beam lattice；
Slice extension；
CompositeMaterials / MultiProperties 完整语义；
texture-driven varnish mask；
texture-driven white mask。
```

---

## 12. 结论

PRD_06B 的核心是：

```text
补齐 3MF 中最常见的基础颜色来源：ColorGroup 与 Texture2DGroup。
```
