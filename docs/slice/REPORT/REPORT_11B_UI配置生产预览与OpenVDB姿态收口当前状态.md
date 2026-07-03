# REPORT_11B_UI配置生产预览与OpenVDB姿态收口当前状态

> 文档版本：v0.1  
> 文档状态：Stage Report / Implemented  
> 生成日期：2026-07-04

---

## 1. 阶段目标

Stage 11B 目标是对 Stage 11A 后暴露的 UI 与 OpenVDB candidate 问题做小收口：

```text
1. 修复 OpenVDB candidate UI 自动生成配置的姿态差异；
2. 增加 production RGB 预览，避免 texture_rgb preview 与生产 TIFF 混淆；
3. 增加 RGBWSV 六通道像素探针；
4. 增加 texture.nonSurfaceRgbPolicy，明确非表面纹理区域写入策略；
5. 收敛 UI 配置入口和 Profile/fixture 展示；
6. 增加 core-only benchmark，用于 legacy 与 OpenVDB candidate 同类耗时对比。
```

本阶段不改变以下生产协议：

```text
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
OpenVDB 仍为 optional / candidate，不替代 legacy 默认路径。
```

---

## 2. 已完成内容

### 2.1 OpenVDB candidate 姿态配置修复

UI 一键生成 OpenVDB candidate 配置时，已写入与 legacy 一键切片一致的姿态配置：

```text
modelTransform.scale = [0.8, 0.8, 0.8]
autoOrient.enabled = true
autoOrient.maxHeightMm = 6.0
autoOrient.strategy = minimize_height_by_right_angle_rotation
```

这解决的是“同一 OBJ 模型 legacy 能趴放而 OpenVDB candidate 不能趴放”的配置差异问题，不代表 OpenVDB candidate 已经具备完整生产替代能力。

### 2.2 Production RGB 预览

`LayerPreviewDataProvider` 增加 `production_rgb` 通道，`LayerPreviewPanel` 通过 manifest layer TIFF 读取生产 RGBWSV TIFF 并渲染 RGB 视图。

当前 UI 中：

```text
production_rgb：来自生产 TIFF 的 R/G/B 通道；
texture_rgb：来自 preview 图，主要用于纹理可视化；
support / white / varnish：仍为伪彩预览。
```

### 2.3 六通道像素探针

`LayerPreviewPanel` 支持对当前生产 RGB 预览进行像素探针读取：

```text
RGBWSV=(R,G,B,W,S,V)
all 255 => 空白
S < 255 => 支撑
RGB 任一通道 < 255 => RGB 模型
W < 255 => 白墨
V < 255 => 光油
多个打印通道同时命中 => 混合
```

该功能用于解释 UI 白色区域、Photoshop RGB 黑色区域和真实六通道生产数据之间的差异。

### 2.4 texture.nonSurfaceRgbPolicy

新增配置字段：

```json
"texture": {
  "nonSurfaceRgbPolicy": "model_material"
}
```

支持值：

```text
model_material：默认值，保持当前 legacy 行为，非表面区域使用 modelMaterial 写 RGB；
empty：非表面纹理区域 RGB 按空白写入；
fallback_rgb：非表面纹理区域写 texture.fallbackRgb；
material_policy：当前基础版等价于 model_material，保留后续材料策略插入点。
```

`texture_report.json` 会输出 `nonSurfaceRgbPolicy`，UI 常用配置面板已提供“非表面 RGB”设置。

### 2.5 UI 配置与 Profile 分层

常用配置面板已按以下分组显示：

```text
基础：模型、输出、层高；
材料：纹理策略、非表面 RGB、白墨、光油；
支撑：支撑开关；
预览：预览开关、预览间隔；
实验：OpenVDB 实验管线开关。
```

`samples/scenarios/slicer_scenarios.json` 增加：

```text
visibility = default | advanced | fixture
audience = user | debug | test | experimental
```

UI 默认隐藏 `advanced` / `fixture`，勾选“显示高级/测试”后可访问。测试脚本仍可读取完整场景索引。

### 2.6 OpenVDB / legacy core-only benchmark

`slicer_cli` 新增：

```powershell
slicer_cli --config <path> --benchmark-core-only --engine legacy|openvdb-candidate
```

core-only 模式禁用：

```text
TIFF 写入；
preview 图片生成；
report/manifest 写入；
package publish。
```

新增脚本：

```text
scripts/run_11b_openvdb_legacy_core_benchmark.ps1
```

脚本输出：

```text
output/benchmarks/openvdb_legacy_core_benchmark_11b.json
schema = p0.openvdb_legacy_core_benchmark.1
```

---

## 3. 验证结果

已运行：

```powershell
cmake --build build --config Debug --target slicer_cli slicer_debug_ui rip_reader_test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\Debug\slicer_cli.exe --config samples\configs\slice_config.json
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\SlicePackage
.\build\Debug\rip_reader_test.exe --package output\SlicePackage --summary
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scenario-registry
.\build\Debug\slicer_cli.exe --config output/tmp_11b_non_surface_empty.json --benchmark-core-only --engine legacy
.\build-openvdb-09p\Debug\slicer_cli.exe --config samples\configs\openvdb_candidate\closed_textured_obj_candidate.json --benchmark-core-only --engine openvdb-candidate
.\scripts\run_11b_openvdb_legacy_core_benchmark.ps1 -LegacyCli .\build\Debug\slicer_cli.exe -OpenVdbCli .\build-openvdb-09p\Debug\slicer_cli.exe -LegacyConfig samples\configs\slice_config.json -OpenVdbConfig samples\configs\openvdb_candidate\closed_textured_obj_candidate.json -Output output\benchmarks\openvdb_legacy_core_benchmark_11b.json
```

关键结果：

```text
UI self-test：PASS startup / PASS experimental-report-summary
layer-preview-load：PASS，channels=production_rgb,support,varnish,occupancy,diagnostic
rip_reader_test：PASS，schema=p0.rgbwsv.2，bitDepth=8，channelOrder=R G B W S V
scenario-registry：PASS default=11 fixture=7 advanced=6
legacy coreComputeMs=49.716
openvdb coreComputeMs=1038.711
openvdb/legacy ratio=20.893
OpenVDB outputSemanticsComparable=false
OpenVDB replacementPass=false
```

---

## 4. 当前结论

### 4.1 11B UI 收口已完成基础版

当前 UI 已能区分生产 RGB 与纹理 preview，也能通过六通道像素探针解释某个位置到底是空白、RGB 模型、支撑、白墨、光油还是混合打印。

配置入口也已从“大量 JSON 直接暴露”向“长期 Profile + 常用 UI 参数 + 高级/测试夹具”过渡。

### 4.2 OpenVDB 当前仍不能替代 legacy

当前 OpenVDB candidate 不满足 replacement gate：

```text
supportPixels = 0；
outputSemanticsComparable = false；
replacementPass = false；
当前 Debug core-only 样例下耗时高于 legacy；
真实模型仍可能受 strict_closed / boundary edges / repair 能力约束。
```

因此：

```text
legacy 继续作为默认生产切片路径；
OpenVDB 保持 candidate / diagnostic；
不得把 OpenVDB non-production 或 semantics 不可比结果作为生产通过。
```

---

## 5. 未完成与后续建议

短期建议：

```text
1. 为 texture.nonSurfaceRgbPolicy=empty 增加正式 golden fixture，比较 RGB printPixels 与 texture_report；
2. 为 UI 一键 OpenVDB candidate 生成配置增加自动 inspect-model smoke；
3. 将 core-only benchmark 扩展到同一真实 OBJ 模型、同一姿态、同一 dpi/layerThickness 的 release 对比；
4. benchmark 报告增加 output write timing，用于区分 coreCompute 与 TIFF/preview/report I/O；
5. 继续保持 OpenVDB optional，不进入默认生产路径。
```

中期建议：

```text
1. OpenVDB candidate 需要补足支撑语义，与 legacy S 通道输出可比；
2. OpenVDB 需要稳定处理真实 OBJ/3MF 的 boundary/self-intersection；
3. 只有 outputSemanticsComparable=true 且 performance/memory gate 达标后，才进入“替代 legacy”评审。
```
