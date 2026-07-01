# DEMO_10_RealModelAcceptanceSet

> 文档版本：v0.1
> 文档状态：Formal DEMO / Stage 10-4
> 生成日期：2026-07-01
> 任务：Task 10-4 真实模型验收集

---

## 1. 目标

本文件定义 Stage 10 的真实模型验收集，用于验证 SliceSoft 输出包、manifest、layer summary、channel summary、texture fidelity 和 downstream handoff 信息是否足够稳定。

本文件不记录未运行得到的像素 exact 值；具体计数必须由 10-6 golden / schema 验证脚本生成或更新。

---

## 2. 验收原则

所有模型必须遵守固定生产协议：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

下游可依赖：

```text
manifest.tiff.*
manifest.layers[]
reports/slice_report.json layer/channel totals
reports/texture_report.json texture stats
reports/model_report.json input diagnostics
reports/three_mf_report.json 3MF resource diagnostics
reports/material_policy_report.json material policy summary
```

下游不可依赖：

```text
preview PNG 颜色；
UI 伪彩配置；
绝对路径 exact match；
experimental OpenVDB surface shell report 作为 production package；
真实打印色彩、RIP 半色调或设备 bitstream。
```

---

## 3. Release Gate 分组

| 分组 | 目的 | Gate 类型 |
|---|---|---|
| G1 OBJ texture | 验证 OBJ/MTL/PNG 纹理链路和 UV 覆盖 | production-safe candidate |
| G2 3MF material | 验证 3MF BaseMaterial / ColorGroup / Texture2DGroup 输入 | production-safe candidate |
| G3 RGB/W/V/S material policy | 验证 RGB、白墨、光油、支撑组合输出 | production-safe candidate |
| G4 texture fallback | 验证 missing texture / no UV fallback 可解释 | negative / diagnostic fixture |
| G5 real nail 3MF | 验证真实 3MF 指甲模型输出和纹理风险 | release candidate gate |
| G6 experimental surface shell | 验证 OpenVDB 壳层纹理原型诊断 | experimental diagnostic only |

---

## 4. 模型清单

### 4.1 G1 OBJ Texture

| ID | Config | Model | 期望输出摘要 | Production-safe 判断 |
|---|---|---|---|---|
| `g1_obj_textured_relief` | `samples/configs/textured/textured_relief_rgb.json` | `samples/models/textured/textured_relief.obj` | RGB 通道有打印像素；S 通道有支撑像素；`texture_report.enabled=true`；`uvCoverageRate` 应接近 1；`fallbackPixelRate` 应接近 0 | Candidate |

验收重点：

```text
RGB 颜色来自纹理采样；
supportPrintPixels > 0；
textureResolvedRate 可计算；
preview 只用于人工检查，不参与 hard gate。
```

### 4.2 G2 3MF Material

| ID | Config | Model | 期望输出摘要 | Production-safe 判断 |
|---|---|---|---|---|
| `g2_3mf_basematerial_single_rgb` | `samples/configs/3mf/three_mf_single_rgb.json` | `samples/models/3mf/single_rgb_cube.3mf` | RGB 通道有打印像素；3MF importer 无 invalid reference；无纹理时 texture fidelity 标记 NotApplicable | Candidate |
| `g2_3mf_colorgroup` | `samples/configs/3mf/three_mf_color_group_rgb.json` | `samples/models/3mf/color_group_cube.3mf` | `colorGroupCount > 0`；`colorGroupCoverage` 可计算；`uvCoverageRate` 可为 NotApplicable | Candidate |
| `g2_3mf_texture2d_checker` | `samples/configs/3mf/three_mf_texture2d_checker.json` | `samples/models/3mf/texture2d_checker_cube.3mf` | `texture2dGroupCount > 0`；`textureLoadedCount > 0`；RGB 纹理采样可追踪 | Candidate |
| `g2_3mf_mixed_color_texture` | `samples/configs/3mf/three_mf_mixed_color_texture.json` | `samples/models/3mf/mixed_basematerial_colorgroup_texture.3mf` | BaseMaterial、ColorGroup、Texture2DGroup 的 report 字段均可解释；优先记录冲突和 fallback | Diagnostic until golden |

验收重点：

```text
three_mf_report.validation.invalidReferenceCount = 0；
ColorGroup / Texture2DGroup 相关 count 与 resolvedTriangles 可解释；
missingTextureRate = 0 才能进入 release candidate。
```

### 4.3 G3 RGB/W/V/S Material Policy

| ID | Config | Model | 期望输出摘要 | Production-safe 判断 |
|---|---|---|---|---|
| `g3_rgb_white_varnish_support` | `samples/configs/material_policy/textured_rgb_white_varnish.json` | `samples/models/textured/fixtures/policy_textured_small.obj` | RGB、W、V、S 四类通道均有对应 summary；V 仅 top n layers；W 为 underbase；S 为支撑投影 | Candidate |
| `g3_material_process_rgbwv` | `samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json` | `samples/models/material_mapping/obj_mtl_texture_rgbwv.obj` | material_process_report 和 material_policy_report 都能解释 RGB/W/V/S 输出 | Candidate |

验收重点：

```text
channelStats.R/G/B/W/S/V 均满足 printPixels + emptyPixels = width * height；
materialPolicy.validation 不允许覆盖 production protocol；
SupportType 只进入 report，不进入 TIFF value。
```

### 4.4 G4 Texture Fallback

| ID | Config | Model | 期望输出摘要 | Production-safe 判断 |
|---|---|---|---|---|
| `g4_missing_texture` | `samples/configs/textured/textured_missing_texture_fallback.json` | `samples/models/textured/fixtures/missing_texture_small.obj` | `missingTextures > 0`；`fallbackPixels > 0`；输出应记录 warning | Not production-safe |
| `g4_no_uv` | `samples/configs/textured/textured_no_uv_fallback.json` | `samples/models/textured/fixtures/no_uv_small.obj` | `facesWithoutUv > 0`；`uvCoverageRate < 1`；fallback 可追踪 | Not production-safe |

不可 production-safe 原因：

```text
缺失纹理或缺失 UV 会导致真实颜色不可保证；
这些 fixture 用于证明 fallback 诊断可解释，不用于 release candidate 放行。
```

### 4.5 G5 Real Nail 3MF

| ID | Config | Model | 期望输出摘要 | Production-safe 判断 |
|---|---|---|---|---|
| `g5_real_3mf_01` | `samples/configs/3mf/three_mf_real_01.json` | `samples/models/3mf/01.3mf` | RGB 和 S 通道均有打印像素；autoOrient 后高度符合 profile；无纹理要求时 texture fidelity 为 NotApplicable | Candidate |
| `g5_real_3mf_02` | `samples/configs/3mf/three_mf_real_02.json` | `samples/models/3mf/02.3mf` | RGB 和 S 通道均有打印像素；支撑随模型拱形收敛；layer/channel summary 稳定 | Candidate |
| `g5_real_3mf_03_texture` | `samples/configs/3mf/three_mf_real_03.json` | `samples/models/3mf/03.3mf` | RGB、S 通道均有打印像素；`texture.applyMode=top_surface_band`；texture fidelity 和 fallback 必须可解释 | Candidate after texture golden |

验收重点：

```text
03.3mf 必须检查顶面纹理不要被误解释为全体积真实颜色；
若 `fallbackPixelRate > 0` 或 `missingTextureRate > 0`，必须降级为 diagnostic；
真实模型验收必须记录 package path、manifest schema、layerCount、supportPrintPixels、rgbPrintPixels 和 texture fidelity summary。
```

### 4.6 G6 Experimental Surface Shell

| ID | Config | Model | 期望输出摘要 | Production-safe 判断 |
|---|---|---|---|---|
| `g6_surface_shell_obj_golden` | `samples/configs/openvdb/surface_shell_nail_obj_golden.json` | `samples/models/textured/textured_relief.obj` | surface shell report 可输出 `transferStats`、nearest query、fallback 诊断 | Experimental only |
| `g6_surface_shell_3mf_golden` | `samples/configs/openvdb/surface_shell_nail_3mf_golden.json` | `samples/models/3mf/03.3mf` | 3MF 真实模型可进入壳层纹理诊断，但不写 production package | Experimental only |
| `g6_missing_texture_openvdb` | `samples/configs/openvdb/surface_shell_obj_missing_texture.json` | `samples/models/openvdb/surface_shell_cube_missing_texture.obj` | missing texture voxel 诊断存在 | Experimental negative |
| `g6_no_uv_openvdb` | `samples/configs/openvdb/surface_shell_obj_no_uv.json` | `samples/models/openvdb/surface_shell_cube_no_uv.obj` | missing UV voxel 诊断存在 | Experimental negative |

不可 production-safe 原因：

```text
OpenVDB 当前仍是 experimental diagnostic path；
不默认启用；
不作为 production RGBWSV package 的唯一验收来源；
需要独立 OpenVDB ON 环境复测。
```

---

## 5. 每个模型必须记录的验收摘要

10-6 golden / schema 生成时，每个模型至少记录：

```text
id
configPath
modelPath
packagePath
productionSafeCandidate
notProductionSafeReason
manifest.schema
manifest.tiff.channelOrder
manifest.tiff.bitDepth
manifest.tiff.polarity
grid.widthPx / heightPx / layerCount
slice_report.totals.rgbPrintPixels
slice_report.totals.whitePrintPixels
slice_report.totals.varnishPrintPixels
slice_report.totals.supportPrintPixels
textureFidelity.available
textureResolvedRate
uvCoverageRate
fallbackPixelRate
missingTextureRate
threeMf.invalidReferenceCount
warningsCount
```

可选记录：

```text
package size；
runtimeMs；
memory peak；
preview folder path；
OpenVDB transferStats。
```

---

## 6. 放行规则

### 6.1 Production-safe Candidate

必须满足：

```text
manifest / TIFF 协议通过 rip_reader；
schema = p0.rgbwsv.2；
layer/channel summary 约束成立；
无 missing layer / wrong storage / wrong polarity；
真实模型的 RGB 或材料通道符合预期；
支撑模型必须 supportPrintPixels > 0；
纹理模型必须 missingTextureRate = 0；
fallback fixture 之外 fallbackPixelRate 不得异常升高；
invalidReferenceCount = 0。
```

### 6.2 Diagnostic Only

以下情况只允许作为诊断：

```text
missingTextures > 0；
facesWithoutUv > 0 且需要纹理；
texture2dGroupCount > 0 但 tex2CoordCount = 0；
invalidReferenceCount > 0；
OpenVDB experimental output；
只生成 preview PNG、未生成 production package。
```

---

## 7. 后续任务衔接

```text
10-5：把本验收集转化为 downstream handoff checklist 的模型清单和反馈入口。
10-6：为本验收集建立 schema / golden summary，填入实际计数和阈值。
10-7：REPORT_10 记录哪些模型通过、哪些仍为 diagnostic 或 pending。
```
