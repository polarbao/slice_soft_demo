# REPORT_05A_真实材料工艺参数验证当前实现状态

> 日期：2026-06-09  
> 阶段：05A / 真实材料工艺参数验证  
> 状态：已完成实现与 quick regression 验证

---

## 1. 本阶段目标

05A 在 06B 已完成 3MF ColorGroup / Texture2DGroup 彩色输入闭环的基础上，不新增输入格式、不重写材料合成链路，而是把现有 RGB / W / V / S 输出能力推进为可复用、可比较、可回归的材料工艺 profile：

- 新增 `materialProcessProfile` 配置段。
- 新增 `reports/material_process_report.json`。
- 验证 RGB + W + V profile。
- 验证 W underbase 覆盖。
- 验证 V `top_n_layers` 层分布差异。
- 新增 profile compare 脚本。
- 使用 3MF Texture2DGroup 与 OBJ/MTL Texture 样例进入 quick regression。

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

## 2. 实现范围

### 2.1 MaterialProcessProfile 配置

新增 `materialProcessProfile`，默认 disabled，不影响旧配置。已支持字段：

```text
enabled
name
target
rgb.enabled
rgb.source
white.enabled
white.mode
white.coverage
white.value
white.expandPx
white.shrinkPx
varnish.enabled
varnish.mode
varnish.topLayers
varnish.value
varnish.coverage
support.expected
support.mode
validation.requireRgbPixels
validation.requireWhitePixels
validation.requireVarnishPixels
validation.requireSupportPixels
validation.maxUnexpectedOverlapPixels
```

第一版定位为 report / validation 层，不覆盖 `MaterialPolicy` 执行逻辑。

### 2.2 material_process_report.json

新增输出：

```text
reports/material_process_report.json
```

报告包含：

- profile 名称与 target。
- 输入格式与源模型。
- grid 与 layerCount。
- RGB / W / V / S `printPixels`。
- RGB / W / V / S `coverageRatio`。
- per-layer RGB / W / V / S 统计。
- V `activeLayerIndices`。
- W `missingUnderbasePixels`。
- `unexpectedOverlapPixels`。
- `validation.pass` / `validation.failures` / `warnings`。

`manifest.json` 已新增：

```text
reports.materialProcess = reports/material_process_report.json
```

### 2.3 Validation

已支持：

- `requireRgbPixels`
- `requireWhitePixels`
- `requireVarnishPixels`
- `requireSupportPixels`
- W underbase 基础覆盖校验
- V top_n_layers active layer 校验
- Support S 独立性基础校验

新增错误码：

```text
E_MATERIAL_PROCESS_PROFILE_EMPTY_RGB
E_MATERIAL_PROCESS_PROFILE_EMPTY_WHITE
E_MATERIAL_PROCESS_PROFILE_EMPTY_VARNISH
E_MATERIAL_PROCESS_PROFILE_EMPTY_SUPPORT
E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_OVERLAP
E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW
E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_VARNISH_LAYER
```

---

## 3. Profile Compare

新增脚本：

```text
scripts/compare_material_profiles.ps1
```

输入两个 package，读取各自的：

```text
reports/material_process_report.json
```

输出：

```text
material_profile_compare.v1
```

包含：

- `profileA / profileB`
- RGB / W / V / S `printPixels` delta
- `changedLayers`
- package validation pass 状态
- per-layer delta

命令示例：

```powershell
.\scripts\compare_material_profiles.ps1 `
  -PackageA output\NailRgbWhiteVarnishTop1 `
  -PackageB output\NailRgbWhiteVarnishTop3 `
  -Output output\MaterialProfileCompare_top1_top3.json
```

---

## 4. 新增样例

新增目录：

```text
samples/configs/material_process/
```

新增配置：

```text
nail_rgb_white_varnish_top1.json
nail_rgb_white_varnish_top2.json
nail_rgb_white_varnish_top3.json
nail_white_underbase_only.json
nail_varnish_only.json
three_mf_texture_rgb_white_varnish.json
obj_mtl_texture_rgb_white_varnish.json
```

quick regression 已接入轻量 profile cases：

```text
nail_rgb_white_varnish_top1.json
nail_rgb_white_varnish_top2.json
three_mf_texture_rgb_white_varnish.json
obj_mtl_texture_rgb_white_varnish.json
```

---

## 5. 验证结果

已运行并通过：

```powershell
cmake --build build --config Debug
.\scripts\make_3mf_samples.ps1
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\nail_rgb_white_varnish_top1.json
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\nail_rgb_white_varnish_top2.json
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\nail_rgb_white_varnish_top3.json
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\three_mf_texture_rgb_white_varnish.json
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\obj_mtl_texture_rgb_white_varnish.json
.\scripts\compare_material_profiles.ps1 -PackageA output\NailRgbWhiteVarnishTop1 -PackageB output\NailRgbWhiteVarnishTop3 -Output output\MaterialProfileCompare_top1_top3.json
.\scripts\run_regression.ps1 -Mode quick
```

`run_regression.ps1 -Mode quick` 最终结果：

```text
Regression complete. mode=quick
```

关键样例统计：

| Package | RGB | W | V | S | V active layers | validation |
|---|---:|---:|---:|---:|---:|---|
| `NailRgbWhiteVarnishTop1` | 22560 | 22560 | 1128 | 5640 | 1 | pass |
| `NailRgbWhiteVarnishTop2` | 22560 | 22560 | 2256 | 5640 | 2 | pass |
| `NailRgbWhiteVarnishTop3` | 22560 | 22560 | 3384 | 5640 | 3 | pass |
| `ThreeMfTextureRgbWhiteVarnish` | 252050 | 252050 | 10082 | 0 | 2 | pass |
| `ObjMtlTextureRgbWhiteVarnish` | 5320 | 5320 | 5040 | 3920 | 20 | pass |

W underbase 覆盖验证：

```text
NailRgbWhiteVarnishTop1 missingUnderbasePixels = 0
NailRgbWhiteVarnishTop2 missingUnderbasePixels = 0
NailRgbWhiteVarnishTop3 missingUnderbasePixels = 0
ThreeMfTextureRgbWhiteVarnish missingUnderbasePixels = 0
ObjMtlTextureRgbWhiteVarnish missingUnderbasePixels = 0
```

Profile compare 验证：

```text
Top1 -> Top2:
delta.varnishPrintPixels = 1128
changedLayers = 1
validation.pass = true

Top1 -> Top3:
delta.varnishPrintPixels = 2256
changedLayers = 2
validation.pass = true
```

---

## 6. 当前边界

05A 仍不支持：

- ICC / CMYK。
- RIP 半色调。
- 真实喷头 bitstream。
- 设备通信。
- OpenVDB / SDF。
- Qt UI。
- 3MF CompositeMaterials 完整语义。
- PBR。
- 支撑形态大改。
- texture-driven white / varnish mask。

实现层面的边界：

- `materialProcessProfile` 第一版是报告和校验层，不替代 `MaterialPolicy`。
- W underbase 校验当前基于已输出通道统计与 print pixel 覆盖关系，不做复杂形态学扩张/收缩。
- OBJ/MTL material role 样例中 V active layers 反映输入材质角色在多层中的分布，不等同于 `top_n_layers` 工艺。

---

## 7. 下一阶段建议

05A 已完成材料工艺 profile 的最小闭环。后续建议：

```text
优先 07：Qt 调试 UI
```

原因：05A 已经产出可比较的 profile 报告，下一步最需要通过 UI 查看、对比、调参和诊断。

备选：

- `06C`：如果近期要接真实复杂 3MF 文件，继续扩展 CompositeMaterials / MultiProperties。
- `08`：如果近期重点转向支撑工艺，再做支撑连通性、膨胀/收缩和形态优化。
