# DEV_11B_UI配置生产预览与OpenVDB姿态收口设计

> 文档版本：v0.1  
> 文档状态：DEV / Stage 11B  
> 生成日期：2026-07-04

---

## 1. Goal

在现有 Stage 11 UI 和 Stage 11A-R1 OpenVDB candidate 基础上，完成以下小收口：

```text
OpenVDB candidate UI 一键配置与 legacy 姿态一致；
UI 支持生产 RGB 和 texture RGB 的清晰区分；
UI 可查看单像素 RGBWSV 六通道生产值；
配置/Profile/fixture 分层，为减少 JSON 暴露做技术准备；
OpenVDB replacement gate 和 benchmark 能指导后续阶段。
```

---

## 2. Current Code Reality

### 2.1 Legacy 一键配置

位置：

```text
apps/slicer_debug_ui/MainWindow.cpp
MainWindow::CreateOneClickConfig
```

当前行为：

```text
写 modelTransform.scale=[0.8,0.8,0.8]
写 autoOrient.enabled=true
写 autoOrient.maxHeightMm=6.0
写 texture.applyMode=top_surface_band
写 support.mode=full_vertical_projection
```

### 2.2 OpenVDB candidate 一键配置

位置：

```text
apps/slicer_debug_ui/MainWindow.cpp
MainWindow::CreateOpenVdbCandidateConfig
```

11B-1 修复前行为：

```text
未写 modelTransform
autoOrient.enabled=false
texture.applyMode=surface_shell_from_sdf
support.enabled=false
support.mode=none
experimental.openvdbPipeline.writeProductionRgbwsv=true
failurePolicy=non_production_only
```

11B-1 修复后行为：

```text
写 modelTransform.scale=[0.8,0.8,0.8]
写 autoOrient.enabled=true
写 autoOrient.maxHeightMm=6.0
texture.applyMode=surface_shell_from_sdf
support.enabled=false
support.mode=none
experimental.openvdbPipeline.writeProductionRgbwsv=true
failurePolicy=non_production_only
```

### 2.3 Preview

当前已有：

```text
LayerPreviewPanel 可读取 preview/report/package；
PreviewOverlayPanel 可做 RGB/S/W/V 叠加；
texture_rgb preview 会隐藏非表面纹理带的默认 RGB。
```

缺口：

```text
生产 RGB 预览没有作为明确模式暴露；
没有六通道像素探针；
没有区分 preview source 的 UI 标签；
没有 texture.nonSurfaceRgbPolicy。
```

---

## 3. Proposed Approach

### 3.1 OpenVDB candidate 姿态一致

修改：

```text
CreateOpenVdbCandidateConfig 增加 modelTransform，与 CreateOneClickConfig 保持一致；
autoOrient.enabled 改为 true；
maxHeightMm 保持 6.0；
strategy 保持 minimize_height_by_right_angle_rotation。
```

实现状态：

```text
已实现；
构建和 UI self-test 已通过；
后续仍需补 UI smoke case 或手动按钮生成配置后执行 inspect-model。
```

验收：

```text
nai_you_new 模型 inspect：
  legacy selectedOrientation = rotate_x_90；
  OpenVDB candidate selectedOrientation = rotate_x_90；
  两者 oriented height <= 6mm。
```

### 3.2 生产 RGB 预览

推荐实现：

```text
LayerPreviewPanel 增加 mode：production_rgb；
production_rgb 从 layers/layer_*.tiff 读取 R/G/B；
texture_rgb 继续读取 preview/texture_rgb_*；
UI 标签显示 source=TIFF RGB 或 source=texture preview。
```

依赖：

```text
已有 TIFF reader 若不适合 UI 直接复用，可新增 UI service 层读取 RGBWSV TIFF 摘要；
UI service 不访问 slicer.cpp 内部结构；
slicer_core 不依赖 Qt。
```

### 3.3 六通道像素探针

推荐实现：

```text
LayerPreviewPanel / PreviewOverlayPanel 捕获鼠标点击；
映射 UI 坐标到 image pixel；
读取对应 layer TIFF 的 R/G/B/W/S/V；
显示：
  x/y/layer/z
  R/G/B/W/S/V value
  interpreted role：RGB model / S support / W white / V varnish / empty / mixed
```

解释规则：

```text
all channels 255 => empty
S < 255 and RGB/W/V 255 => support
RGB any < 255 and S/W/V 255 => RGB model
W < 255 => white
V < 255 => varnish
多个通道同时打印 => mixed/conflict candidate
```

### 3.4 texture.nonSurfaceRgbPolicy

新增配置：

```json
{
  "texture": {
    "nonSurfaceRgbPolicy": "model_material"
  }
}
```

候选值：

```text
model_material：当前行为，非表面纹理带写 modelMaterial.rgb；
empty：非表面纹理带 RGB 写 255；
fallback_rgb：写 texture.fallbackRgb；
material_policy：交给 MaterialPolicy/Profile 决定。
```

默认建议：

```text
保持 model_material，避免改变历史输出；
UI 一键 OBJ 彩色纹理 profile 可提供 empty 选项，但需要明确工艺确认。
```

### 3.5 UI 配置收敛

新增或扩展“切片设置”界面：

```text
基础：模型、输出、层高、scale、autoOrient；
材料：RGB 默认值、白墨、光油、nonSurfaceRgbPolicy；
支撑：enabled、mode、shape、connectivity；
预览：生产 RGB / texture RGB、interval、channels、pseudo colors；
实验：OpenVDB diagnostic/candidate、admission、non-production。
```

配置文件分层：

```text
profiles/production：长期用户可选 profile；
profiles/debug：工程调试 profile；
fixtures/regression：测试夹具，不默认展示；
samples/configs 保留历史兼容，但 UI 默认只索引常用 profile。
```

---

## 4. OpenVDB Replacement Gate

OpenVDB 取代 legacy 前必须满足：

```text
1. 同模型同姿态输出语义对齐；
2. 支撑策略与 legacy 等价或有明确差异说明；
3. 真实 OBJ/3MF 集合 strict_closed PASS，或 repair_then_strict PASS；
4. RIP reader strict PASS；
5. LayerPreview / OverlayPreview PASS；
6. texture fidelity 达到阈值；
7. Release benchmark 性能不慢于 legacy，或质量收益足以接受；
8. 内存峰值在预算内；
9. OpenVDB OFF 默认轨道不退化；
10. 连续回归稳定。
```

当前不满足：

```text
nai_you_new 同姿态探索中 OpenVDB candidate 为 non_production_written；
strictClosedFailure = boundary edges；
supportPixels = 0；
Debug 下耗时 40.794s，高于 legacy 22.653s；
输出 grid/layerCount 不一致。
```

---

## 5. Validation Plan

### 5.1 姿态一致

```powershell
.\build\Debug\slicer_cli.exe --config <legacy> --inspect-model
.\build\Debug\slicer_cli.exe --config <openvdb-candidate> --inspect-model
```

### 5.2 Legacy 回归

```powershell
.\scripts\run_11a_obj_standard_tests.ps1 -BuildDir build -Config Debug
```

### 5.3 OpenVDB ON

```powershell
.\scripts\run_11a_r1_openvdb_candidate_on_lane.ps1 -OpenVdbBuildDir build-openvdb-09p -UiBuildDir build -Config Debug
```

### 5.4 UI

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
```

### 5.5 Benchmark

正式 benchmark 必须：

```text
Release 构建；
同模型；
同 scale；
同 autoOrient；
同 output resolution；
同输出语义；
拆分 coreComputeMs 与 endToEndMs；
coreComputeMs 不计入 TIFF 保存、preview 图片生成、report/manifest 写入；
记录生产/非生产状态；
记录 support/RGB/W/V 像素；
记录 timing + memory；
禁止把 non-production 结果与 production 结果直接等价比较。
```

详细 benchmark 设计见：

```text
docs/slice/DEV/DEV_11B_OpenVDB_LegacyCoreBenchmark设计.md
```

---

## 6. Rollback

```text
如果 OpenVDB candidate 姿态修复导致回归，恢复 CreateOpenVdbCandidateConfig 的 autoOrient=false；
如果生产 RGB 预览读取 TIFF 风险过高，保留 texture_rgb preview，只先实现像素探针；
如果 nonSurfaceRgbPolicy 引发输出差异，默认继续 model_material，仅在显式 profile 启用 empty；
任何失败不得影响 legacy run_slicer。
```
