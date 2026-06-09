# QT_DEBUG_UI_软件配置说明

> 日期：2026-06-09  
> 适用程序：`slicer_debug_ui`  
> 所属阶段：07 / Qt 调试 UI 基础版  
> 目的：说明本地构建、运行、切片任务、输出包和报告查看相关配置文件的作用。

---

## 1. 软件定位

`slicer_debug_ui` 是本地调试工具，不是生产 UI。它不直接修改切片算法，也不改变 RGBWSV 输出协议，而是包装现有命令和本地文件：

```text
slicer_cli
rip_reader_test
scripts/run_regression.ps1
scripts/compare_material_profiles.ps1
manifest.json
reports/*.json
preview/*.png / *.ppm
```

冻结协议：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

---

## 2. UI 程序构建配置

### 2.1 根 CMake 配置

路径：

```text
CMakeLists.txt
```

相关配置：

```cmake
option(BUILD_SLICER_DEBUG_UI "Build Qt slicer debug UI" ON)
find_package(Qt5 COMPONENTS Widgets QUIET)
```

作用：

- 控制是否构建 Qt 调试 UI。
- 找到 Qt5 Widgets 时添加 `apps/slicer_debug_ui`。
- 找不到 Qt5 Widgets 时跳过 UI target，不影响 `slicer_cli`、`rip_reader_test` 和 `slicer_core`。

### 2.2 UI target 配置

路径：

```text
apps/slicer_debug_ui/CMakeLists.txt
```

作用：

- 定义 `slicer_debug_ui` 可执行程序。
- 开启 Qt `AUTOMOC`。
- 链接 `Qt5::Widgets`。
- MSVC 下启用 `/utf-8`，保证中文界面字符串按 UTF-8 编译。

输出路径：

```text
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe
```

---

## 3. VS Code 本地运行调试配置

### 3.1 tasks.json

路径：

```text
.vscode/tasks.json
```

关键任务：

| Task | 功能 |
|---|---|
| `SliceSoft: Configure` | 执行 `cmake -S . -B build`，生成/刷新构建目录。 |
| `SliceSoft: Build Debug` | 构建 Debug 下所有默认 target，包括 CLI、RIP reader 和 UI。 |
| `SliceSoft: Build Debug UI` | 只构建 `slicer_debug_ui` target。 |
| `SliceSoft: Run ...` | 运行特定样例配置，生成对应输出包。 |
| `SliceSoft: Run rip_reader_test ...` | 对指定输出包执行 RIP 协议校验。 |

### 3.2 launch.json

路径：

```text
.vscode/launch.json
```

关键入口：

```text
SliceSoft: Debug Qt slicer_debug_ui
```

作用：

- 在 VS Code“运行与调试”界面启动 Qt 调试 UI。
- 运行前自动执行 `SliceSoft: Build Debug UI`。
- 工作目录为 `${workspaceFolder}`，因此 UI 默认以仓库根目录解析相对路径。

注意：

```text
slicer_debug_ui.exe 不在 build/Debug 下，
而在 build/apps/slicer_debug_ui/Debug 下。
```

---

## 4. UI 内部默认路径配置

实现位置：

```text
apps/slicer_debug_ui/services/ToolPaths.*
apps/slicer_debug_ui/MainWindow.cpp
```

默认工具路径：

| 项 | Windows 默认路径 |
|---|---|
| 切片工具 | `build/Debug/slicer_cli.exe` |
| RIP 校验工具 | `build/Debug/rip_reader_test.exe` |
| PowerShell | `powershell` |
| UI 程序 | `build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe` |

UI 默认选择的样例：

| UI 字段 | 默认值 | 作用 |
|---|---|---|
| 配置文件 | `samples/configs/material_process/nail_rgb_white_varnish_top2.json` | 点击“运行切片”时传给 `slicer_cli --config`。 |
| 输出包 | `output/NailRgbWhiteVarnishTop2` | 点击“加载输出包”时读取 manifest、reports、preview。 |
| 对比包 A | `output/NailRgbWhiteVarnishTop1` | 点击“对比工艺配置”时作为 `PackageA`。 |
| 对比包 B | `output/NailRgbWhiteVarnishTop3` | 点击“对比工艺配置”时作为 `PackageB`。 |

---

## 5. 切片任务 JSON 配置

切片任务配置统一位于：

```text
samples/configs/
```

UI 中“配置文件”选择的就是这些 JSON。点击“运行切片”后，UI 执行：

```powershell
build/Debug/slicer_cli.exe --config <config.json>
```

### 5.1 顶层公共字段

以 `samples/configs/material_process/nail_rgb_white_varnish_top2.json` 为例：

| 字段 | 功能 |
|---|---|
| `slicingMode` | 切片模式，例如 `relief_heightfield`。 |
| `input.modelPath` | 输入模型路径，相对配置文件目录解析。 |
| `input.format` | 输入格式，常用 `auto`。 |
| `output.packageDir` | 输出包目录，UI 运行切片后会自动尝试加载该目录。 |
| `output.dpiX / dpiY` | X/Y 方向 DPI。 |
| `output.layerThicknessMm` | 层厚，单位 mm。 |
| `output.channelOrder` | 通道顺序，当前固定 `R G B W S V`。 |
| `output.bitDepth` | 输出位深，当前固定 8。 |
| `output.storageMode` | TIFF 存储模式，支持 `stripped` / `tiled`。 |
| `background.value` | 空白区域值，当前协议中通常为 255。 |
| `autoOrient` | 自动摆正/高度约束策略。 |
| `texture` | 贴图采样配置。 |
| `materialPolicy` | RGB/W/V 材料输出策略。 |
| `materialProcessProfile` | 工艺 profile 报告与校验配置。 |
| `support` | 支撑生成配置。 |
| `relief` | 浮雕 heightfield 采样配置。 |
| `preview` | 预览图输出配置。 |

### 5.2 配置目录功能

| 目录 | 代表用途 |
|---|---|
| `samples/configs/` | 基础 P0/P1 样例和 `0.3.obj` 等入口。 |
| `samples/configs/relief/` | 单材料/浮雕模型切片样例。 |
| `samples/configs/textured/` | OBJ/MTL/Texture 彩色纹理样例。 |
| `samples/configs/support/` | 支撑生成、unsupported-only、孤岛过滤样例。 |
| `samples/configs/storage_mode/` | TIFF `stripped/tiled` 存储模式兼容样例。 |
| `samples/configs/material_policy/` | RGB、白墨、光油组合策略样例。 |
| `samples/configs/material_mapping/` | OBJ/MTL 多材料角色映射样例。 |
| `samples/configs/3mf/` | 3MF basematerial、ColorGroup、Texture2DGroup 样例。 |
| `samples/configs/material_process/` | 真实材料工艺 profile 验证样例，推荐用于 UI 调试。 |

### 5.3 推荐 UI 调试配置

| 配置文件 | 输出包 | 用途 |
|---|---|---|
| `samples/configs/material_process/nail_rgb_white_varnish_top2.json` | `output/NailRgbWhiteVarnishTop2` | RGB + W underbase + V top2 + support。 |
| `samples/configs/material_process/three_mf_texture_rgb_white_varnish.json` | `output/ThreeMfTextureRgbWhiteVarnish` | 3MF Texture2DGroup 纹理进入 RGB/W/V 工艺链路。 |
| `samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json` | `output/ObjMtlTextureRgbWhiteVarnish` | OBJ/MTL Texture 纹理进入 RGB/W/V 工艺链路。 |
| `samples/configs/3mf/three_mf_texture2d_checker.json` | `output/ThreeMfTexture2dChecker` | 3MF 内部贴图基础验证。 |
| `samples/configs/textured/textured_relief_rgb.json` | `output/TexturedReliefRgb` | 大尺寸彩色纹理浮雕模型验证。 |

---

## 6. Preview 配置与 UI 显示

配置段：

```json
"preview": {
  "enabled": true,
  "format": "png",
  "interval": 10,
  "channels": ["texture_rgb", "white", "varnish", "support"],
  "onlyNonEmptyLayers": true
}
```

字段功能：

| 字段 | 功能 |
|---|---|
| `enabled` | 是否生成预览图。若为 `false`，UI 仍可查看 reports，但 preview 面板无图。 |
| `format` | 预览格式，UI 支持 PNG / PPM。 |
| `interval` | 每隔多少层输出一组预览。 |
| `channels` | 需要输出的预览通道。 |
| `onlyNonEmptyLayers` | 是否只输出非空层。 |

UI 会扫描：

```text
<package>/preview/*.png
<package>/preview/*.ppm
```

并提供：

- 通道筛选。
- 层滑块。
- 适应窗口。
- 1:1。
- 放大/缩小。

---

## 7. 输出包结构

切片完成后，`output.packageDir` 指向的目录通常包含：

```text
output/<Package>/
  manifest.json
  layers/
    layer_000000.tiff
    ...
  reports/
    slice_report.json
    support_report.json
    material_policy_report.json
    material_process_report.json
    texture_report.json
    three_mf_report.json
    preview_report.json
    ...
  preview/
    *.png / *.ppm
```

UI 读取逻辑：

| 文件/目录 | UI 功能 |
|---|---|
| `manifest.json` | 报告页 raw JSON，确认 schema、通道、极性、layer list。 |
| `reports/*.json` | 报告页 raw JSON 和摘要视图。 |
| `reports/material_process_report.json` | 右侧“材料工艺”面板摘要。 |
| `preview/*.png / *.ppm` | 中央“预览”页图像显示。 |

---

## 8. 脚本配置与 UI 按钮关系

| UI 按钮 | 调用命令 | 依赖配置/输入 |
|---|---|---|
| 构建调试版 | `cmake --build build --config Debug` | `CMakeLists.txt`、build 目录。 |
| 运行切片 | `slicer_cli --config <config>` | UI 中选择的配置 JSON。 |
| 运行 RIP 摘要 | `rip_reader_test --package <package> --summary` | UI 中选择的输出包目录。 |
| 运行快速回归 | `scripts/run_regression.ps1 -Mode quick` | `samples/configs/`、`scripts/`、现有构建产物。 |
| 对比工艺配置 | `scripts/compare_material_profiles.ps1 -PackageA <A> -PackageB <B> -Output output/MaterialProfileCompare_ui.json` | 两个输出包中的 `material_process_report.json`。 |

本地脚本：

| 脚本 | 功能 |
|---|---|
| `scripts/run_regression.ps1` | 分层执行 quick/full/heavy 回归。 |
| `scripts/compare_material_profiles.ps1` | 对比两个材料工艺输出包。 |
| `scripts/make_3mf_samples.ps1` | 生成/刷新 3MF 正向样例包。 |
| `scripts/make_bad_3mf_packages.ps1` | 生成 bad 3MF 负向测试包。 |
| `scripts/run_3mf_negative_tests.ps1` | 执行 bad 3MF 负向测试。 |
| `scripts/make_bad_packages.ps1` | 生成 RGBWSV 协议坏包。 |

---

## 9. 运行方式

### 9.1 VS Code

在“运行与调试”中选择：

```text
SliceSoft: Debug Qt slicer_debug_ui
```

### 9.2 PowerShell

```powershell
cmake -S . -B build
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe
```

非交互初始化检查：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

---

## 10. 常见问题

### 10.1 UI target 没生成

原因通常是 CMake 没找到 Qt5 Widgets。检查：

```powershell
cmake -S . -B build
```

如果输出包含：

```text
Qt5 Widgets not found; slicer_debug_ui target is skipped.
```

说明 CLI 仍可构建，但 UI 不会生成。

### 10.2 UI 中没有预览图

检查配置：

```json
"preview": {
  "enabled": true
}
```

以及输出包中是否存在：

```text
preview/*.png
preview/*.ppm
```

### 10.3 材料工艺面板为空

检查输出包是否存在：

```text
reports/material_process_report.json
```

只有启用 `materialProcessProfile.enabled = true` 的配置才会产生完整材料工艺报告。

### 10.4 运行切片后 UI 没自动加载输出包

UI 通过配置文件中的：

```json
"output": {
  "packageDir": "..."
}
```

推导输出包路径。若配置文件 JSON 解析失败或 `packageDir` 缺失，需要手动选择输出包目录。

---

## 11. 当前边界

当前 UI 不做：

- 生产任务系统。
- 设备通信。
- 喷头 bitstream。
- RIP 半色调。
- ICC / CMYK。
- OpenVDB。
- 新切片算法。
- 完整 3D 模型视图。
- JSON 表单化参数编辑。

后续建议进入 `07A` 时再做：

- 配置 JSON 表单编辑。
- 材料工艺 profile 可视化编辑。
- 白墨/光油参数面板。
- per-layer 图表。
- preview overlay。
