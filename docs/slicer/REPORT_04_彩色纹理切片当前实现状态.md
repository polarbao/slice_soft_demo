# REPORT_04_彩色纹理切片当前实现状态

> 文档版本：v0.2  
> 文档状态：当前实现状态  
> 阶段范围：04 / 彩色纹理模型切片基础版  
> 生成时间：2026-06-08

---

## 1. 阶段结论

04 阶段已完成基础 RGB 纹理数据链路，并已针对真实彩色浮雕美甲模型修正切片填充方式：

```text
OBJ + MTL + Texture
→ vt / material 解析
→ relief top surface UV 采样
→ RGB 通道写入
→ RGBWSV TIFF package
→ preview / reports / rip_reader_test
```

当前 `samples/configs/textured/textured_relief_rgb.json` 使用：

```text
relief.fillMode = intersection_range
relief.baseZMm = 0.0
preview.channels = texture_rgb, support
```

其中 `texture_rgb` 是 true-color 预览通道，直接显示 RGB 纹理值；原 `rgb/model_rgb` preview 仍遵守 00B 的生产极性反相显示语义。

本阶段没有改变 03 固化协议：

- `schema = p0.rgbwsv.1`
- `channelOrder = R G B W S V`
- `bitDepth = 8`
- `polarity = black_is_print`
- `printValue = 0`
- `emptyValue = 255`
- `Model > Support > Empty`
- `SupportType` 不进入 TIFF 通道

---

## 2. 当前已实现范围

### 2.1 OBJ Loader

已支持：

- `vt` 纹理坐标解析
- face `v`
- face `v/vt`
- face `v//vn`
- face `v/vt/vn`
- `mtllib`
- `usemtl`
- triangulated face 的 material name 记录
- `facesWithUv` / `facesWithoutUv` 统计

### 2.2 MTL Loader

已支持：

- `newmtl`
- `Kd`
- `map_Kd`
- `map_Kd` 相对 MTL 文件目录解析
- 若 MTL 相对路径未命中，再按 OBJ 文件目录解析
- missing texture warning

### 2.3 Texture Loader

当前使用 Windows Imaging Component 读取纹理：

- PNG
- JPG / JPEG
- BMP

实现不依赖 Qt，不引入 vcpkg 第三方库。

### 2.4 Texture Sampler

已支持：

- `nearest`
- `bilinear`
- `clamp`
- `repeat`
- `flipV`
- `fallbackRgb`

### 2.5 Relief Top Surface Sampling

当前优先支持：

```text
texture.applyMode = solid_volume_from_top_surface
```

实现方式：

1. `relief_heightfield` 每个 XY column 记录最高命中的 triangle。
2. 同时记录该命中的 barycentric coordinate。
3. 使用 triangle 的 3 个 UV 插值 column UV。
4. 使用 MTL `map_Kd` 对应 texture 采样 RGB。
5. 将该 column 的所有 model occupied layers 写入同一 RGB。

### 2.6 Layer Compose

`texture.enabled = true` 时：

- 模型区域：`R/G/B = sampled texture RGB`
- 模型区域：`W/S/V = 255`
- 支撑区域：`S = 0`，其他通道 `255`
- 空白区域：所有通道 `255`

支撑通道仍由 02 支撑逻辑生成，没有被纹理 RGB 覆盖。合成优先级保持：

```text
Model > Support > Empty
```

因此同一 XY 像素在某层既属于模型又属于支撑投影时，最终 TIFF 中写入模型 RGB，S 通道保持 `255`。

---

## 3. Reports

新增：

```text
reports/texture_report.json
```

字段包括：

- `enabled`
- `applyMode`
- `materials`
- `textureFiles`
- `loadedTextures`
- `missingTextures`
- `stats.facesWithUv`
- `stats.facesWithoutUv`
- `stats.sampledPixels`
- `stats.fallbackPixels`
- `stats.uvOutOfRangePixels`
- `warnings`

`reports/slice_report.json` 已增加：

```text
totals.texture.enabled
totals.texture.sampledPixels
totals.texture.fallbackPixels
totals.texture.uvOutOfRangePixels
```

`reports/model_report.json` 已增加：

```text
texcoordCount
facesWithUv
facesWithoutUv
materialInfos
```

manifest 的 `reports` 已增加：

```text
"texture": "reports/texture_report.json"
```

---

## 4. 样例

新增模型目录：

```text
samples/models/textured/
```

包含：

- `textured_relief.obj`
- `textured_relief.mtl`
- `textured_missing_texture.obj`
- `textured_missing_texture.mtl`
- `textured_no_uv.obj`
- `textures/gradient.png`
- `textures/checker.png`

新增配置目录：

```text
samples/configs/textured/
```

包含：

- `textured_relief_rgb.json`
- `textured_missing_texture_fallback.json`
- `textured_no_uv_fallback.json`

---

## 5. 已验证样例

### 5.1 Textured Relief RGB

命令：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\textured\textured_relief_rgb.json
build\Debug\rip_reader_test.exe --package output\TexturedReliefRgb
```

结果：

- `grid = 289 x 718 x 728`
- `loadedTextures = 1`
- `missingTextures = 0`
- `facesWithUv = 70262`
- `facesWithoutUv = 0`
- `sampledPixels = 19602925`
- `fallbackPixels = 0`
- `uvOutOfRangePixels = 0`
- `supportPixels = 62664673`
- `preview.texture_rgb = 72`
- `preview.support = 47`
- 第 50 层：`modelPrintPixels = 1212`，`supportPrintPixels = 182692`
- 第 400 层：`modelPrintPixels = 55450`，`supportPrintPixels = 70425`
- RIP reader 通过

当前真实模型会自动旋转：

```text
autoOrient.selectedOrientation = rotate_x_90
original height = 30.371799 mm
oriented height ~= 7.27 mm
```

### 5.2 Missing Texture Fallback

命令：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\textured\textured_missing_texture_fallback.json
build\Debug\rip_reader_test.exe --package output\TexturedMissingTextureFallback
```

当前状态：

- 本轮重跑 `slicer_cli` 后输出包可生成。
- 当前 `samples/models/textured/textured_missing_texture.obj` 已被替换为 38MB 真实纹理模型级别数据，不再是早期缺失纹理轻量 fixture。
- 当前输出报告显示 `loadedTextures = 1`，`missingTextures = 0`，`facesWithUv = 70262`，`fallbackPixels = 0`。
- 因此该用例当前不能证明 missing texture fallback 行为。
- 本轮并行执行两个大模型 fallback 包的 `rip_reader_test` 超过 120 秒未完成，已停止后台进程；不把该项记录为本轮 RIP 通过。

处理建议：

```text
重建小型 missing-texture fixture，使 OBJ/MTL 引用不存在的 map_Kd；
再单独验证 missingTextures > 0、warnings > 0、fallbackPixels > 0。
```

### 5.3 No UV Fallback

命令：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\textured\textured_no_uv_fallback.json
build\Debug\rip_reader_test.exe --package output\TexturedNoUvFallback
```

当前状态：

- 本轮重跑 `slicer_cli` 后输出包可生成。
- 当前 `samples/models/textured/textured_no_uv.obj` 已被替换为 38MB 真实纹理模型级别数据，不再是早期无 UV 轻量 fixture。
- 当前输出报告显示 `facesWithUv = 70262`，`facesWithoutUv = 0`，`sampledPixels = 19602925`，`fallbackPixels = 0`。
- 因此该用例当前不能证明 no-UV fallback 行为。
- 本轮并行执行两个大模型 fallback 包的 `rip_reader_test` 超过 120 秒未完成，已停止后台进程；不把该项记录为本轮 RIP 通过。

处理建议：

```text
重建小型 no-UV fixture，使 OBJ face 不包含 vt；
再单独验证 facesWithUv = 0、facesWithoutUv > 0、fallbackPixels > 0。
```

---

## 6. 回归状态

已运行或保留的历史验证：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -SkipHeavyRelief
.\scripts\run_regression.ps1
```

历史结果：均通过，其中完整回归最终输出 `Regression complete.`。

本轮补充验证：

```powershell
cmake --build build --config Debug
build\Debug\slicer_cli.exe --config samples\configs\textured\textured_relief_rgb.json
build\Debug\rip_reader_test.exe --package output\TexturedReliefRgb
```

结果：Textured Relief RGB 主样例通过。

本轮未完成：

```text
TexturedMissingTextureFallback / TexturedNoUvFallback 的 RIP reader 大模型重验
```

原因：当前两个 fallback fixture 已被替换为 38MB 真实纹理模型级别数据，并行执行 `rip_reader_test` 超过 120 秒未完成。该问题不影响 `TexturedReliefRgb` 主样例结论，但说明 fallback fixture 需要重建。

覆盖：

- ordinary P0
- Support samples
- Textured Relief RGB
- Missing texture fallback
- No UV fallback
- Relief V/W/RGB
- Bad package 负向错误码矩阵

`scripts/run_regression.ps1` 已加入 texture 正向用例。

---

## 7. 当前未实现范围

04 当前仍不是完整全彩切片系统，以下内容未实现：

- ICC / 色彩管理
- CMYK
- RIP 半色调
- 3MF
- OpenVDB
- Qt UI
- PBR
- 法线贴图
- 多贴图混合
- 纹理驱动白墨
- 纹理驱动光油
- `surface_shell`
- `per_layer_surface_sample`
- `color_shell_volume`
- 普通闭合模型的完整外壳纹理投影
- 设备侧真实 RIP 接口
- 支撑孤岛/狭缝的美甲业务级形态优化

### 7.1 当前已知诊断：第 68 层支撑局部割裂

在 `output/TexturedReliefRgb/layers/layer_000068.tiff` 中，S 通道按 `S=0` 解析得到：

```text
supportPrintPixels = 181537
modelPrintPixels = 2123
support connected components = 3
largest support component = 181150 px, bbox x=6..276, y=0..716
right small support component = 383 px, bbox x=280..287, y=500..573
tiny edge component = 4 px, bbox x=3..3, y=475..478
right model component = 1066 px, bbox x=268..281, y=263..649
```

结论：

```text
这不是 RGB 纹理覆盖支撑造成的协议错误；
也不是 S 通道整层缺失；
而是模型右侧存在低层侧壁/边缘模型像素，按 Model > Support 优先级覆盖了同位置支撑，
导致主支撑与右侧小支撑岛在视觉上被模型像素隔开。
```

该现象与当前 `bottom_projection` 支撑策略有关：支撑按 relief lower surface 做逐列投影，但合成 TIFF 时模型像素优先于支撑像素。后续如需让支撑形态更符合美甲业务预期，需要在 05 或后续阶段设计支撑形态策略，例如支撑连通修复、狭缝过滤、外侧小支撑岛合并/剔除、或按业务轮廓约束支撑区域。

---

## 8. 下一阶段建议

建议进入 05 前先确认业务优先级：

1. 如果目标是美甲/浮雕生产链路，建议进入 `05 材料策略`，定义 RGB/W/V/S 的业务组合规则。
2. 如果目标是提高支撑可制造性，建议先定义支撑连通性与小岛处理规则，避免真实美甲模型边缘侧壁造成局部割裂。
3. 如果目标是更通用的彩色 OBJ，建议先扩展 `color_shell_volume` 或闭合模型外壳采样，而不是先做白墨/光油策略。
4. 如果目标是设备对接，建议基于 `p0.rgbwsv.1` 和 `texture_report.json` 增加更严格的 package 验收脚本。

不建议在 05 前直接展开 RIP 半色调、ICC、OpenVDB 或 Qt UI。
