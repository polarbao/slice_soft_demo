# DEMO_04_彩色纹理切片验证方案

> 文档版本：v0.1  
> 文档状态：Draft / DEMO  
> 建议提交目录：`docs/slicer/`

## 1. Demo 目标

验证基础彩色纹理链路：

```text
OBJ + MTL + Texture
→ UV sample
→ RGB channel output
→ RGBWSV TIFF
→ preview / reports / rip_reader_test
```

## 2. 样例目录

新增：

```text
samples/models/textured/
  textured_relief.obj
  textured_relief.mtl
  textures/checker.png
  textures/gradient.png

samples/configs/textured/
  textured_relief_rgb.json
  textured_missing_texture_fallback.json
  textured_no_uv_fallback.json
```

## 3. 样例 1：Textured Relief RGB

配置要点：

```text
slicingMode = relief_heightfield
texture.enabled = true
texture.applyMode = solid_volume_from_top_surface
texture.sampler = bilinear
texture.uvAddressMode = clamp
texture.flipV = true
modelMaterial.materialChannel = RGB
support.enabled = true
```

验收：

```text
model_rgb preview 显示 checker / gradient 变化
RGB channelStats.printPixels > 0
texture_report.sampledPixels > 0
rip_reader_test pass
```

## 4. 样例 2：Missing Texture Fallback

故意配置缺失贴图。

验收：

```text
切片不崩溃
texture_report.missingTextures > 0 或 warnings 非空
fallbackPixels > 0
RGB 使用 fallbackRgb
rip_reader_test pass
```

## 5. 样例 3：No UV Fallback

使用无 vt 的 OBJ。

验收：

```text
texture_report.facesWithoutUv > 0
fallbackPixels > 0
RGB 使用 fallbackRgb
```

## 6. 验证命令

```powershell
cmake --build build --config Debug

build\Debug\slicer_cli.exe --config samples\configs\textured\textured_relief_rgb.json
build\Debug\rip_reader_test.exe --package output\TexturedReliefRgb

build\Debug\slicer_cli.exe --config samples\configs\textured\textured_missing_texture_fallback.json
build\Debug\rip_reader_test.exe --package output\TexturedMissingTextureFallback
```

## 7. 回归 Checklist

- [ ] 03 的 `scripts/run_regression.ps1` 仍通过。
- [ ] Textured Relief 样例通过。
- [ ] Missing texture fallback 通过。
- [ ] No UV fallback 通过。
- [ ] `texture_report.json` 存在。
- [ ] `model_rgb` preview 有颜色变化。
- [ ] RGB channelStats 有 printPixels。
- [ ] S 支撑通道不被破坏。
- [ ] bad package 负向测试仍通过。

## 8. 非目标

```text
ICC
CMYK
RIP 半色调
PBR
3MF
OpenVDB
Qt UI
纹理驱动光油
纹理驱动白墨
```

## 9. 状态报告

完成后必须生成：

```text
docs/slicer/REPORT_04_彩色纹理切片当前实现状态.md
```
