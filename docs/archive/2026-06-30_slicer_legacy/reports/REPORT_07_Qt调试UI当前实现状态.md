# REPORT_07_Qt调试UI当前实现状态

> 日期：2026-06-09  
> 阶段：07 / Qt 调试 UI 基础版  
> 状态：已完成基础实现、构建验证与 CLI quick regression 验证

---

## 1. 本阶段目标

07 阶段目标是新增本地调试 UI：

```text
slicer_debug_ui
```

用于包装现有命令行与报告体系：

- 选择 config。
- 选择 output package。
- 运行 `slicer_cli`。
- 运行 `rip_reader_test --summary`。
- 运行 `scripts/run_regression.ps1 -Mode quick`。
- 运行 `scripts/compare_material_profiles.ps1`。
- 查看 manifest / reports。
- 查看 preview PNG / PPM。
- 查看 `material_process_report.json` summary。
- 查看日志、stderr、exit code、耗时和 `E_*` 错误码。

本阶段保持不变：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
0=打印，255=不打印
Model > Support > Empty
MaterialRoleMapping 语义不变
MaterialPolicy 语义不变
MaterialProcessProfile 语义不变
S support 仍由 Support pipeline 独立生成
```

---

## 2. 工程结构

新增目录：

```text
apps/slicer_debug_ui/
```

当前结构：

```text
apps/slicer_debug_ui/
  CMakeLists.txt
  main.cpp
  MainWindow.h
  MainWindow.cpp
  services/
    PackageLoader.*
    ProcessRunner.*
    ReportLoader.*
    ToolPaths.*
  widgets/
    LogPanel.*
    MaterialProcessPanel.*
    PreviewPanel.*
    ReportPanel.*
```

---

## 3. CMake 状态

根 `CMakeLists.txt` 新增：

```cmake
option(BUILD_SLICER_DEBUG_UI "Build Qt slicer debug UI" ON)
find_package(Qt5 COMPONENTS Widgets QUIET)
```

行为：

- 找到 Qt5 Widgets 时，添加 `apps/slicer_debug_ui`。
- 找不到 Qt5 Widgets 时，输出提示并跳过 `slicer_debug_ui`，不影响 `slicer_cli` / `rip_reader_test` / `slicer_core`。

当前环境验证：

```text
Qt5 Widgets 已找到
slicer_debug_ui 已生成
```

输出路径：

```text
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe
```

---

## 4. UI 已实现功能

### 4.1 Project / Config / Package

已实现：

- 显示 repo root。
- 显示 build dir 推导路径。
- 显示 `slicer_cli` 路径。
- 显示 `rip_reader_test` 路径。
- 选择 config JSON。
- 选择 output package。
- 选择工艺对比包 A / B。
- 打开输出目录。

默认样例：

```text
Config = samples/configs/material_process/nail_rgb_white_varnish_top2.json
Package = output/NailRgbWhiteVarnishTop2
对比包 A = output/NailRgbWhiteVarnishTop1
对比包 B = output/NailRgbWhiteVarnishTop3
```

### 4.2 ProcessRunner

已实现：

- `QProcess` 异步执行。
- stdout 捕获。
- stderr 捕获。
- exit code 捕获。
- duration 统计。
- command line 显示。
- `E_*` 错误码高亮。

### 4.3 Run Panel

已实现按钮：

- `构建调试版`
- `运行切片`
- `运行 RIP 摘要`
- `运行快速回归`
- `对比工艺配置`
- `加载输出包`
- `打开输出目录`

命令：

```text
cmake --build build --config Debug
build/Debug/slicer_cli.exe --config <config>
build/Debug/rip_reader_test.exe --package <package> --summary
powershell -ExecutionPolicy Bypass -File scripts/run_regression.ps1 -Mode quick
powershell -ExecutionPolicy Bypass -File scripts/compare_material_profiles.ps1 -PackageA <A> -PackageB <B> -Output output/MaterialProfileCompare_ui.json
```

### 4.4 Report Viewer

已实现：

- `manifest.json` raw JSON。
- `reports/*.json` raw JSON。
- 基础 summary view。
- warnings / failures / errors 递归收集。
- 支持 `slice_report`、`material_process_report`、`material_policy_report`、`material_role_mapping_report`、`texture_report`、`three_mf_report`、`support_report` 等现有 JSON。

### 4.5 Preview Viewer

已实现：

- 扫描 `preview` 目录。
- 支持 PNG。
- 支持 PPM。
- channel selector。
- layer slider。
- fit。
- actual size。
- zoom in / zoom out。

### 4.6 MaterialProcessPanel

已实现读取：

```text
reports/material_process_report.json
```

显示：

- `profileName`
- `target`
- `inputFormat`
- RGB/W/V/S `printPixels`
- RGB/W/V/S `coverageRatio`
- V `activeLayerIndices`
- W `missingUnderbasePixels`
- `unexpectedOverlapPixels`
- `validation.pass`
- `validation.failures`
- `warnings`

### 4.7 Profile Compare

已实现：

- 选择 Package A。
- 选择 Package B。
- 调用 `scripts/compare_material_profiles.ps1`。
- 输出到 `output/MaterialProfileCompare_ui.json`。
- UI 显示对比结果 JSON 摘要和 raw JSON。

### 4.8 界面中文化

已完成：

- 主窗口标题中文化。
- 配置文件、输出包、对比包路径选择区中文化。
- 运行按钮中文化。
- 预览、报告、材料工艺、警告/失败、工艺对比标签中文化。
- 进程状态、错误提示、退出码和耗时显示中文化。
- 报告摘要字段中文化。
- Preview channel selector 显示中文通道名。

保留原文的内容：

- 原始 JSON key。
- 文件名。
- 命令行参数。
- 协议字段值。
- 外部 CLI stdout/stderr 原始输出。

---

## 5. VS Code 运行调试入口

已新增 task：

```text
SliceSoft: Build Debug UI
```

已新增 launch：

```text
SliceSoft: Debug Qt slicer_debug_ui
```

注意实际程序路径是：

```text
${workspaceFolder}/build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe
```

不是：

```text
${workspaceFolder}/build/Debug/slicer_debug_ui.exe
```

---

## 6. 已运行验证

已运行并通过：

```powershell
cmake -S . -B build
cmake --build build --config Debug --target slicer_debug_ui
cmake --build build --config Debug
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_regression.ps1 -Mode quick
```

`slicer_debug_ui --self-test` 说明：

- 构造 `QApplication`。
- 构造 `MainWindow`。
- 加载默认 package 路径。
- 不进入 GUI event loop。
- 返回 0 表示基础 UI 初始化通过。

`run_regression.ps1 -Mode quick` 最终结果：

```text
Regression complete. mode=quick
```

界面中文化后追加验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

---

## 7. 当前未实现 / 非目标范围

07 当前仍不支持：

- 生产级 UI。
- 设备通信。
- 喷头 bitstream。
- RIP 半色调。
- ICC / CMYK。
- OpenVDB / SDF。
- 新切片算法。
- 完整 3D 模型 viewport。
- JSON 表单化参数编辑。
- MaterialProcessProfile 可视化编辑。
- 支撑 overlay 专项视图。
- 多任务队列。
- 多设备调度。

实现限制：

- Report summary 是第一版通用摘要，复杂 report 仍主要依赖 raw JSON 查看。
- Preview channel 通过文件名 token 粗分类，不解析每个 preview 的完整业务元数据。
- `Open Output Folder` 使用系统桌面服务，实际可用性依赖本机 GUI 环境。

---

## 8. 下一阶段建议

建议进入：

```text
07A：Qt 参数编辑与 profile 可视化增强
```

优先事项：

- JSON config 表单编辑。
- `materialProcessProfile` 可视化编辑。
- white / varnish 参数面板。
- topLayers slider。
- material_process per-layer 图表。
- preview overlay 与 channel 对比。

备选路线：

- `08`：支撑形态与工艺优化。
- `06C`：复杂 3MF CompositeMaterials / MultiProperties / external relationship texture。

---

## 9. 配置说明文档

已新增：

```text
docs/slicer/QT_DEBUG_UI_软件配置说明.md
```

内容覆盖：

- CMake / Qt target 配置。
- VS Code task / launch 配置。
- UI 默认路径配置。
- `samples/configs` 各类切片任务配置用途。
- output package 结构。
- reports / preview 与 UI 面板关系。
- 本地脚本与 UI 按钮关系。
