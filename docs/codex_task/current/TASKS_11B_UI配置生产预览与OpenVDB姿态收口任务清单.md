# TASKS_11B_UI配置生产预览与OpenVDB姿态收口任务清单

> 文档版本：v0.1  
> 文档状态：Task List / Stage 11B  
> 生成日期：2026-07-04

---

## 0. 执行边界

Stage 11B 只做小收口：

```text
修复 OpenVDB candidate UI 姿态配置；
补生产 RGB / 六通道预览解释；
补 UI 配置收敛；
补 OpenVDB replacement gate 和 benchmark。
```

不做：

```text
OpenVDB 默认替代 legacy；
修改 p0.rgbwsv.2；
删除 samples/configs；
设备通信 / RIP 半色调；
完整 mesh repair。
```

---

## Task 11B-1：OpenVDB candidate 姿态配置修复

状态：

```text
已实现；
OpenVDB candidate UI 生成配置已与 legacy 一键配置保持 autoOrient / modelTransform 一致；
cmake --build build --config Debug --target slicer_debug_ui 通过；
slicer_debug_ui.exe --self-test 通过；
OpenVDB ON 轨道 slicer_cli --help 可执行；
仍建议后续补更细的 UI 端到端 inspect-model smoke。
```

修改：

```text
apps/slicer_debug_ui/MainWindow.cpp
MainWindow::CreateOpenVdbCandidateConfig
```

要求：

```text
新增 modelTransform，与 legacy 一键配置保持一致；
autoOrient.enabled=true；
maxHeightMm=6.0；
strategy=minimize_height_by_right_angle_rotation。
```

验证：

```powershell
.\build\Debug\slicer_cli.exe --config <generated-legacy> --inspect-model
.\build\Debug\slicer_cli.exe --config <generated-openvdb-candidate> --inspect-model
```

通过标准：

```text
同一模型 selectedOrientation 一致；
oriented height <= 6mm；
legacy 回归不变。
```

---

## Task 11B-2：生产 RGB 预览模式

状态：

```text
已实现；
LayerPreviewDataProvider 增加 production_rgb 通道；
LayerPreviewPanel 读取 manifest layer TIFF，按 RGBWSV 生产 TIFF 渲染生产 RGB；
生产 RGB 与 texture_rgb preview 分离。
```

目标：

```text
UI 能明确显示 production RGB from TIFF；
与 texture_rgb preview 分开。
```

修改：

```text
apps/slicer_debug_ui/services/*
apps/slicer_debug_ui/widgets/LayerPreviewPanel.*
src/slicer_core/tiff_io.*
apps/slicer_debug_ui/CMakeLists.txt
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
```

通过标准：

```text
UI 可选择生产 RGB；
source 标识清楚；
不改变 production TIFF。
```

已验证：

```powershell
cmake --build build --config Debug --target slicer_cli slicer_debug_ui rip_reader_test
.\build\Debug\slicer_cli.exe --config samples\configs\slice_config.json
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\SlicePackage
.\build\Debug\rip_reader_test.exe --package output\SlicePackage --summary
```

---

## Task 11B-3：六通道像素探针

状态：

```text
已实现；
LayerPreviewPanel 支持点击生产 RGB 预览后读取同层 TIFF 像素；
显示 RGBWSV=(R,G,B,W,S,V) 和 empty/RGB模型/白墨/支撑/光油/混合解释；
UI smoke 已通过 PixelProbeForTest 验证返回 RGBWSV。
```

目标：

```text
点击层预览像素，显示 R/G/B/W/S/V 和解释结果。
```

解释规则：

```text
all 255 => empty
S < 255 => support
RGB any < 255 => RGB model
W < 255 => white
V < 255 => varnish
multiple print channels => mixed
```

验证：

```text
UI self-test 或新增 ui-smoke-test case；
手动抽样与 TIFF 像素值一致。
```

已验证：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\SlicePackage
```

---

## Task 11B-4：texture.nonSurfaceRgbPolicy

状态：

```text
已实现基础版；
config parser / validator 支持 model_material、empty、fallback_rgb、material_policy；
默认值为 model_material，保持 legacy 默认输出不变；
texture_report 输出 nonSurfaceRgbPolicy；
QuickConfigPanel 已暴露“非表面 RGB”设置。
```

目标：

```text
明确非表面纹理带模型内部 RGB 如何写入。
```

候选值：

```text
model_material
empty
fallback_rgb
material_policy
```

默认：

```text
model_material
```

验证：

```text
config parser 单测；
legacy 默认输出不变；
empty profile 输出 RGB printPixels 下降且 report 可解释。
```

已验证：

```powershell
.\build\Debug\slicer_cli.exe --config output/tmp_11b_non_surface_empty.json --benchmark-core-only --engine legacy
```

说明：

```text
当前已验证 parser/core-only 接受 empty；
后续若要把 empty 作为正式工艺 profile，还需要补 golden fixture 对比 RGB printPixels 与 texture_report。
```

---

## Task 11B-5：UI 切片设置分组

状态：

```text
已实现基础版；
ConfigEditorPanel 保持长期 tab 分组；
QuickConfigPanel 常用配置细分为基础、材料、支撑、预览、实验；
常用面板可直接编辑模型、输出、层高、纹理策略、非表面 RGB、支撑、白墨、光油、预览和 OpenVDB 实验开关。
```

目标：

```text
减少用户直接面对大量 JSON。
```

分组：

```text
基础；
材料；
支撑；
预览；
实验。
```

验证：

```text
字段级 UI smoke；
保存后 JSON 可被 slicer_cli 读取。
```

已验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

---

## Task 11B-6：Profile / fixture 分层

状态：

```text
已实现基础版；
ScenarioRegistry 支持 audience / visibility；
UI 默认隐藏 visibility=advanced / fixture；
勾选“显示高级/测试”后可访问高级和测试夹具；
samples/scenarios/slicer_scenarios.json 已标记 UI smoke、支撑 fixture、fallback fixture、部分 3MF/工艺对比样例。
```

目标：

```text
UI 默认展示长期 Profile；
回归 fixture 不默认展示；
高级/测试分类仍可访问。
```

建议：

```text
扩展 samples/scenarios/slicer_scenarios.json 分类；
增加 visibility / audience 字段；
UI 默认隐藏 fixture。
```

验证：

```text
场景下拉列表默认只显示 production/debug 常用项；
脚本仍可读取全部 fixture。
```

已验证：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scenario-registry
```

---

## Task 11B-7：OpenVDB replacement benchmark 脚本

状态：

```text
已实现 Debug 可运行基础版；
slicer_cli 增加 --benchmark-core-only --engine legacy|openvdb-candidate；
scripts/run_11b_openvdb_legacy_core_benchmark.ps1 可运行 legacy/OpenVDB candidate 并输出合并报告；
核心 benchmark 禁用 TIFF、preview、reports 和 package publish；
报告 schema 为 p0.openvdb_legacy_core_benchmark.1。
```

目标：

```text
同模型、同姿态、同输出策略比较 legacy 与 OpenVDB candidate。
拆分核心切片耗时和输出写入耗时。
```

要求：

```text
Release 构建；
记录 coreComputeMs；
记录 endToEndMs；
coreComputeMs 不包含 TIFF 保存；
coreComputeMs 不包含 preview 图片生成；
coreComputeMs 不包含 report/manifest 写入；
记录分阶段 timing；
记录内存；
记录 production/non-production；
记录 support/RGB/W/V 统计；
记录 outputSemanticsComparable；
禁止把 non-production 误判为替代通过。
```

通过标准：

```text
输出 benchmark report；
报告 schema 为 p0.openvdb_legacy_core_benchmark.1 或后续兼容版本；
明确 OpenVDB 是否满足 replacement gate。
```

已验证：

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\slice_config.json --benchmark-core-only --engine legacy
.\build-openvdb-09p\Debug\slicer_cli.exe --config samples\configs\openvdb_candidate\closed_textured_obj_candidate.json --benchmark-core-only --engine openvdb-candidate
.\scripts\run_11b_openvdb_legacy_core_benchmark.ps1 -LegacyCli .\build\Debug\slicer_cli.exe -OpenVdbCli .\build-openvdb-09p\Debug\slicer_cli.exe -LegacyConfig samples\configs\slice_config.json -OpenVdbConfig samples\configs\openvdb_candidate\closed_textured_obj_candidate.json -Output output\benchmarks\openvdb_legacy_core_benchmark_11b.json
```

当前结果：

```text
legacy coreComputeMs=49.716；
openvdb coreComputeMs=1038.711；
openvdb/legacy ratio=20.893；
outputSemanticsComparable=false；
replacementPass=false。
```

---

## Task 11B-8：阶段报告

状态：

```text
已生成；
见 docs/slice/REPORT/REPORT_11B_UI配置生产预览与OpenVDB姿态收口当前状态.md。
```

输出：

```text
docs/slice/REPORT/REPORT_11B_UI配置生产预览与OpenVDB姿态收口当前状态.md
```

报告必须包括：

```text
已完成任务；
验证命令；
OpenVDB 当前是否可替代 legacy；
后续建议。
```
