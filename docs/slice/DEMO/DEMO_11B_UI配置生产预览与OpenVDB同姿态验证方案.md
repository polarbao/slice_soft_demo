# DEMO_11B_UI配置生产预览与OpenVDB同姿态验证方案

> 文档版本：v0.1  
> 文档状态：DEMO / Stage 11B  
> 生成日期：2026-07-04

---

## 1. 验证目标

Stage 11B 验证目标：

```text
OpenVDB candidate UI 配置与 legacy 一键配置使用同样摆放策略；
生产 RGB / texture RGB / S/W/V 预览语义可解释；
配置设置入口能减少用户直接选择大量 JSON；
OpenVDB replacement gate 有可执行 benchmark 方案；
OpenVDB 仍不默认替代 legacy。
```

---

## 2. 验证模型

首批使用：

```text
model/obj/nai_you_new/MF_nai_you.obj
model/obj/nai_you_new/MF_nai_you.mtl
model/obj/nai_you_new/T_Nai_you.png
```

理由：

```text
真实 OBJ/MTL/Texture 模型；
历史上出现过 UI 预览白色 / TIFF RGB 黑色解释问题；
原始姿态高度超过 6mm，能验证 autoOrient。
```

---

## 3. 姿态一致验证

### 3.1 Legacy

```powershell
.\build\Debug\slicer_cli.exe --config <legacy-generated> --inspect-model
```

期望：

```text
autoOrient.enabled = true
autoOrient.applied = true
selectedOrientation = rotate_x_90
oriented height <= 6mm
```

### 3.2 OpenVDB Candidate

```powershell
.\build\Debug\slicer_cli.exe --config <openvdb-candidate-generated> --inspect-model
```

期望：

```text
autoOrient.enabled = true
autoOrient.applied = true
selectedOrientation = rotate_x_90
oriented height <= 6mm
```

失败判定：

```text
OpenVDB candidate selectedOrientation = identity；
OpenVDB candidate height > 6mm；
OpenVDB candidate 缺 modelTransform.scale。
```

---

## 4. 生产预览验证

### 4.1 生产 RGB 预览

期望 UI 可显示：

```text
source = production RGB from TIFF
value source = layers/layer_*.tiff R/G/B
```

### 4.2 纹理 RGB 预览

期望 UI 可显示：

```text
source = texture_rgb preview
value source = preview/texture_rgb_*.png 或 .ppm
```

### 4.3 像素探针

点击像素后显示：

```text
layer
zMm
x/y
R/G/B/W/S/V
interpretation
```

典型解释：

```text
(0,0,0,255,255,255) => RGB model / entity fill
(255,255,255,255,0,255) => S support
(255,255,255,255,255,255) => empty
```

---

## 5. UI 配置收敛验证

验证 UI 是否至少能覆盖：

```text
模型文件；
输出目录；
层高；
scale；
autoOrient；
纹理策略；
nonSurfaceRgbPolicy；
支撑模式；
预览通道；
预览间隔；
OpenVDB diagnostic/candidate 开关。
```

期望：

```text
普通用户无需直接从 70+ JSON 中挑选；
fixture 仍可通过测试脚本使用；
高级调试仍允许手动打开 JSON。
```

---

## 6. OpenVDB 同姿态 Benchmark

### 6.1 探索性结果记录

当前已执行一次 Debug 探索：

```text
模型：model/obj/nai_you_new/MF_nai_you.obj
姿态：scale=[0.8,0.8,0.8] + autoOrient rotate_x_90

legacy_total_seconds = 22.653
OpenVDB candidate_total_seconds = 40.794
```

但该结果不能作为正式替代 gate：

```text
OpenVDB candidate status = non_production_written；
productionPackageWritten = false；
strictClosedFailure = boundary edges；
supportPixels = 0；
OpenVDB 输出与 legacy 支撑/层数/网格不等价。
```

### 6.2 正式 benchmark 条件

正式 benchmark 必须满足：

```text
Release 构建；
同模型；
同姿态；
同 DPI / layerHeight；
同 preview 策略；
同生产输出语义；
OpenVDB productionAllowed=true；
RIP reader PASS；
UI preview PASS；
记录 timing + memory。
```

---

## 7. 验证命令建议

文档/配置检查：

```powershell
git diff --check
```

Legacy：

```powershell
.\scripts\run_11a_obj_standard_tests.ps1 -BuildDir build -Config Debug
```

OpenVDB：

```powershell
.\scripts\run_11a_r1_openvdb_candidate_on_lane.ps1 -OpenVdbBuildDir build-openvdb-09p -UiBuildDir build -Config Debug
```

UI：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

---

## 8. 通过标准

```text
P0 姿态一致 PASS；
P0 legacy 回归 PASS；
P0 OpenVDB candidate 不误写 production；
P0 文档明确 OpenVDB 当前不可替代 legacy；
P1 生产 RGB/像素探针/UI 设置进入任务清单；
P1 benchmark 方案可执行。
```
