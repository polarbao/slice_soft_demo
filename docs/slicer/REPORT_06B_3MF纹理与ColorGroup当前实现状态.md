# REPORT_06B_3MF纹理与ColorGroup当前实现状态

> 日期：2026-06-09  
> 阶段：06B / 3MF Texture2D 与 ColorGroup 基础支持  
> 状态：已完成实现与 quick regression 验证

---

## 1. 本阶段目标

06B 在 06A 的 3MF stored/deflate、安全校验和负向测试基础上，补齐 3MF 彩色输入的最小闭环：

- 支持 `ColorGroup` 基础颜色解析。
- 支持 `Texture2D / Texture2DGroup` 基础贴图解析。
- 支持 triangle `pid / p1 / p2 / p3` 到颜色或 UV 的解析。
- 支持 3MF 包内 PNG 贴图提取并复用现有 `TextureSampler`。
- 增强 `three_mf_report.json` 与 `texture_report.json`。
- 增加 06B 正向样例、bad package 负向用例和 quick regression 校验。

本阶段保持不变：

- `schema = p0.rgbwsv.2`
- `channelOrder = R G B W S V`
- `bitDepth = 8`
- `polarity = black_is_print`
- `0=打印，255=不打印`
- `MaterialRoleMapping` 基础语义不变
- `MaterialPolicy` RGB/W/V overlay 语义不变
- S 支撑仍由 Support pipeline 独立生成

---

## 2. XML Parser 方案

本阶段未引入 `tinyxml2`，采用增强后的 `ThreeMfXmlReader v2`：

- 支持 namespace/local-name 匹配。
- 支持 self-closing tag。
- 支持嵌套 resource block 扫描。
- 继续拒绝 `DOCTYPE / ENTITY`。
- 不访问外部资源，不启用外部实体。

依赖取舍：

- `tinyxml2` 解析能力更完整，适合后续 06C 或真实复杂 3MF 文件扩展。
- 当前 reader v2 依赖少、无需 vcpkg/DLL 变更，适合 06B 基础闭环。

限制：

- 仍不是完整 XML parser。
- 未实现 DTD、entity、复杂 namespace edge case 和完整 3MF extension 语义。

---

## 3. ColorGroup 支持范围

已支持：

- 解析 `<colorgroup id="...">`。
- 解析 `<color color="#RRGGBB"/>`。
- 兼容 `#AARRGGBB`，当前忽略 alpha。
- 建立 `group id -> color table`。
- 解析 triangle `pid / p1 / p2 / p3`。
- `p1/p2/p3` 三顶点颜色不一致时，使用三色平均 fallback。
- 将 ColorGroup 解析结果作为 RGB 材料输入，进入现有 `MaterialRoleMapping` 和 compose 链路。

新增错误：

```text
E_3MF_COLORGROUP_INDEX_OUT_OF_RANGE
```

---

## 4. Texture2D / Texture2DGroup 支持范围

已支持：

- 解析 `<texture2d id path contenttype>`。
- 解析 `<texture2dgroup id texid>`。
- 解析 `<tex2coord u v/>`。
- 支持 `/3D/Textures/xxx.png` 与 `3D/Textures/xxx.png` 包内路径。
- 拒绝 texture path traversal。
- 将包内贴图提取到 `output/<Package>/cache/3mf_textures/`。
- 复用现有 `TextureSampler` 进行 RGB 采样。
- 将 triangle `pid / p1 / p2 / p3` 解析为三角形 UV。
- `texture_report.json` 记录 `source = 3mf_internal`。

新增错误或 warning：

```text
E_3MF_TEXTURE_PATH_TRAVERSAL
E_3MF_TEXTURE_PATH_MISSING
E_3MF_TEXTURE2DGROUP_MISSING_TEXID
E_3MF_TEX2COORD_INDEX_OUT_OF_RANGE
```

贴图解码失败仍走现有 texture fallback 策略：

- `missingTexturePolicy = warn_and_fallback`：继续输出并记录 warning。
- `missingTexturePolicy = fail_fast`：失败退出。

---

## 5. Report 增强

`reports/three_mf_report.json` 新增或补齐：

```text
colorGroups.count
colorGroups.colorCount
colorGroups.resolvedTriangles
colorGroups.interpolatedColorFallbackCount
textures.texture2dCount
textures.texture2dGroupCount
textures.tex2CoordCount
textures.resourceCount
textures.loadedCount
textures.missingCount
textures.sampledPixels
textures.resolvedTriangles
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
```

`reports/texture_report.json` 新增或补齐：

```text
source
materials[].source
```

3MF 内部贴图样例验证摘录：

```text
ThreeMfTexture2dChecker:
textures.texture2dCount = 1
textures.texture2dGroupCount = 1
textures.loadedCount = 1
textures.missingCount = 0
textures.sampledPixels = 252050
texture_report.source = 3mf_internal
texture_report.stats.sampledPixels = 252050
```

---

## 6. 样例与配置

新增正向样例模型：

```text
samples/models/3mf/color_group_cube.3mf
samples/models/3mf/texture2d_checker_cube.3mf
samples/models/3mf/mixed_basematerial_colorgroup_texture.3mf
```

新增正向样例配置：

```text
samples/configs/3mf/three_mf_color_group_rgb.json
samples/configs/3mf/three_mf_texture2d_checker.json
samples/configs/3mf/three_mf_mixed_color_texture.json
```

`scripts/make_3mf_samples.ps1` 已扩展：

- 继续生成 06A stored/deflate 样例。
- 生成 06B ColorGroup 样例。
- 生成 06B Texture2DGroup checker 样例。
- 生成 basematerial / ColorGroup / Texture2DGroup mixed 样例。

---

## 7. Bad 3MF 用例

新增并通过：

- `bad_3mf_texture_path_missing`：成功 fallback，report 记录 `E_3MF_TEXTURE_PATH_MISSING`。
- `bad_3mf_texture_decode_failed`：成功 fallback，texture report 记录 decode failed warning。
- `bad_3mf_texture2dgroup_missing_texid`：失败，`E_3MF_TEXTURE2DGROUP_MISSING_TEXID`。
- `bad_3mf_tex2coord_index_out_of_range`：失败，`E_3MF_TEX2COORD_INDEX_OUT_OF_RANGE`。
- `bad_3mf_colorgroup_index_out_of_range`：失败，`E_3MF_COLORGROUP_INDEX_OUT_OF_RANGE`。
- `bad_3mf_unsupported_compositematerials`：成功 fallback，report 记录 unsupported resource。
- `bad_3mf_unsupported_multiproperties`：成功 fallback，report 记录 unsupported resource。

说明：

- 06B 起 `colorgroup / texture2d / texture2dgroup` 不再作为 unsupported resource。
- `compositematerials / multiproperties` 仍只记录 unsupported，不实现完整语义。

---

## 8. 已运行验证

已运行并通过：

```powershell
cmake --build build --config Debug
.\scripts\make_3mf_samples.ps1
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_color_group_rgb.json
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_texture2d_checker.json
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_mixed_color_texture.json
.\scripts\make_bad_3mf_packages.ps1
.\scripts\run_3mf_negative_tests.ps1
.\scripts\run_regression.ps1 -Mode quick
```

`run_regression.ps1 -Mode quick` 最终结果：

```text
Regression complete. mode=quick
```

---

## 8.1 真实 03.3mf 纹理顶面带修复

2026-06-10 针对真实模型 `samples/models/3mf/03.3mf` 补充修复：

- 新增 `texture.applyMode = top_surface_band`。
- 新增 `texture.topSurfaceLayers` 配置项。
- `top_surface_band` 按每个 XY 列自身的真实上表面向下取 N 层写入纹理 RGB。
- 该模式避免 `solid_volume_from_top_surface` 将顶面纹理贯穿到整个实体体积。
- 该模式也避免 `top_surface_only` 只写 1 层导致 UI/preview 大面积显示黑色。
- `samples/configs/3mf/three_mf_real_03.json` 已切换为：

```json
{
  "texture": {
    "applyMode": "top_surface_band",
    "topSurfaceLayers": 50
  },
  "preview": {
    "interval": 25
  }
}
```

重新切片验证结果：

```text
packageDir: output/ThreeMfReal03
grid: 283 x 718 x 798
modelPixels: 20928026
supportPixels: 62927008
texture_report.applyMode = top_surface_band
texture_report.topSurfaceLayers = 50
texture_report.source = 3mf_internal
texture_report.loadedTextures = 3
texture_report.sampledPixels = 8851501
texture_report.fallbackPixels = 0
texture_report.uvOutOfRangePixels = 0
```

RGB preview 验证：

- 0 层附近不再出现贯穿式顶面花纹。
- 300 层以后开始出现连续彩色纹理区域。
- `model_rgb_000300.png` 彩色像素约 21621。
- `model_rgb_000350.png` 彩色像素约 50716。
- `model_rgb_000700.png` 彩色像素约 3758。

已通过回归：

```powershell
cmake --build build --config Debug --target slicer_cli
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_real_03.json
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\scripts\run_ci_quick.ps1
```

限制说明：

- `top_surface_band` 是当前 heightfield 路线下的轻量顶面颜色带，不是完整三维纹理壳层算法。
- 侧壁与复杂倒扣表面的纹理仍按当前 relief heightfield 的 top-column 采样近似处理。
- 如后续需要真实外表面壳层纹理，应单独进入 `surface_shell_texture` 或等价阶段，不在 06B 修复中展开。

---

## 9. 当前未支持范围

06B 仍不支持：

- PBR / metallic / roughness。
- ICC / CMYK / 色彩管理。
- CompositeMaterials 完整混合语义。
- MultiProperties 完整多属性组合语义。
- Production extension 完整语义。
- Beam lattice / slice extension。
- 3MF 外部 relationship texture 资源。
- ZIP64。
- encrypted ZIP。
- OpenVDB。
- Qt UI。
- RIP 半色调。
- 白墨/光油策略新增能力。

---

## 10. 下一阶段建议

建议优先进入以下阶段之一：

- `05A`：若业务重点是打印效果，继续强化白墨 underbase、光油 top layer、材料策略验收。
- `07`：若业务重点是调试效率，进入 Qt/可视化诊断 UI。
- `06C`：若业务重点是真实 3MF 文件兼容性，建议引入 `tinyxml2` 并扩展 Composite/MultiProperties/外部 relationship 兼容。

当前建议：

```text
如果近期继续围绕 3MF 真实文件兼容推进，优先 06C；
如果近期需要验证打印工艺输出，优先 05A；
如果需要降低无 UI 排障成本，优先 07。
```
