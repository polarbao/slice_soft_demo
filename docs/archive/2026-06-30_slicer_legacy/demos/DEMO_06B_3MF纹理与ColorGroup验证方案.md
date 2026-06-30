# DEMO_06B_3MF纹理与ColorGroup验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 适用阶段：PRD_06B / DEV_06B  
> 建议提交目录：`docs/slicer/`

---

## 1. Demo 目标

验证：

```text
3MF ColorGroup → RGB
3MF Texture2DGroup → TextureSampler → RGB
unsupported advanced resource → report fallback
bad texture/color package → expected failure
```

---

## 2. 正向样例

新增：

```text
samples/models/3mf/color_group_cube.3mf
samples/models/3mf/texture2d_checker_cube.3mf
samples/models/3mf/mixed_basematerial_colorgroup_texture.3mf
```

配置：

```text
samples/configs/3mf/three_mf_color_group_rgb.json
samples/configs/3mf/three_mf_texture2d_checker.json
samples/configs/3mf/three_mf_mixed_color_texture.json
```

---

## 3. 样例 1：ColorGroup RGB

验收：

```text
three_mf_report.colorGroups.count > 0
three_mf_report.colorGroups.colorCount > 0
three_mf_report.colorGroups.resolvedTriangles > 0
RGB printPixels > 0
rip_reader_test --summary pass
```

---

## 4. 样例 2：Texture2DGroup Checker

验收：

```text
three_mf_report.textures.texture2dCount > 0
three_mf_report.textures.texture2dGroupCount > 0
three_mf_report.textures.loadedCount > 0
texture_report.source = 3mf_internal
texture_report.sampledPixels > 0
model_rgb preview 出现 checker/gradient 变化
rip_reader_test --summary pass
```

---

## 5. 样例 3：Mixed Basematerial + ColorGroup + Texture

验收：

```text
basematerial triangles resolved
colorgroup triangles resolved
texture group triangles resolved
RGB printPixels > 0
warnings = 0 或仅有已知 unsupported warning
rip_reader_test --summary pass
```

---

## 6. 负向样例

新增：

```text
bad_3mf_texture_path_missing
bad_3mf_texture_decode_failed
bad_3mf_texture2dgroup_missing_texid
bad_3mf_tex2coord_index_out_of_range
bad_3mf_colorgroup_index_out_of_range
bad_3mf_unsupported_compositematerials
bad_3mf_unsupported_multiproperties
```

---

## 7. 验证命令

```powershell
cmake --build build --config Debug

build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_color_group_rgb.json
build\Debug\rip_reader_test.exe --package output\ThreeMfColorGroupRgb --summary

build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_texture2d_checker.json
build\Debug\rip_reader_test.exe --package output\ThreeMfTexture2dChecker --summary

.\scripts\run_regression.ps1 -Mode quick
```

---

## 8. 回归 Checklist

- [ ] 3MF ColorGroup positive pass。
- [ ] 3MF Texture2DGroup positive pass。
- [ ] Mixed color/texture positive pass。
- [ ] texture_report 记录 3mf_internal。
- [ ] three_mf_report 记录 colorGroups/textures 统计。
- [ ] bad texcoord index 能失败。
- [ ] bad color index 能失败。
- [ ] missing texture path 能 warning/fallback 或按配置失败。
- [ ] quick regression pass。
- [ ] p0.rgbwsv.2 不变。

---

## 9. 非目标

```text
PBR
ICC / CMYK
RIP 半色调
OpenVDB
Qt UI
Production Extension
Beam lattice
CompositeMaterials / MultiProperties 完整语义
texture-driven white/varnish mask
```

---

## 10. 状态报告

完成后生成：

```text
docs/slicer/REPORT_06B_3MF纹理与ColorGroup当前实现状态.md
```
