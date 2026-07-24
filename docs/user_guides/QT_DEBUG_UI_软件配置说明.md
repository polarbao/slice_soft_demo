# QT_DEBUG_UI_软件配置说明

> 日期：2026-07-24
> 适用程序：`slicer_debug_ui`  
> 所属阶段：12E-09B / Qt 双模式生产入口已收口
> 目的：说明本地构建、运行、切片任务、输出包和报告查看相关配置文件的作用。

> 更新说明：当前 VSCode 运行环境已精简为通用入口；模型/Profile 场景改由 `samples/scenarios/slicer_scenarios.json` 和 Qt 调试 UI 的“场景/Profile”选择器管理。详见 `docs/user_guides/VSCode与Qt调试UI运行环境说明.md`。

---

## 1. 软件定位

`slicer_debug_ui` 是本地切片调试工作台，并已接通 Legacy / Global Surface Shell 两种受控生产入口。它不改变 RGBWSV 输出协议，而是通过 Effective Config、模型预检和当前 session 身份包装现有命令和本地文件：

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
| `SliceSoft: Run slicer_cli current config` | 通过 VSCode 输入框运行指定配置。 |
| `SliceSoft: Run rip_reader current package` | 通过 VSCode 输入框对指定输出包执行 RIP 摘要。 |
| `SliceSoft: Run sample matrix` | 读取 `samples/scenarios/slicer_scenarios.json` 批量运行样例。 |
| `SliceSoft: Clean build/output ...` | 显式清理本地 `build` / `output` 目录。 |

### 3.2 launch.json

路径：

```text
.vscode/launch.json
```

关键入口：

```text
SliceSoft: Debug Qt UI
SliceSoft: Debug slicer_cli current config
SliceSoft: Debug rip_reader current package
```

作用：

- 在 VS Code“运行与调试”界面启动 Qt 调试 UI。
- 运行前自动执行 `SliceSoft: Build Debug UI`。
- 工作目录为 `${workspaceFolder}`，因此 UI 默认以仓库根目录解析相对路径。
- CLI 和 RIP 调试入口通过 VSCode 输入框填写当前配置或输出包，不再为每个样例复制一个环境。

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
| 场景/Profile | `material_process_top2` | 从 `samples/scenarios/slicer_scenarios.json` 读取，自动填充配置文件和输出包。 |
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
| 导入模型并切片 | `slicer_cli --config output/ui_sessions/<model>/slice_config.generated.json` | 从任意目录选择模型后生成的临时配置。 |
| 导入模型并 OpenVDB 诊断 | `slicer_cli --config <generated> --experimental-openvdb-shell --admission-mode diagnostic_only` | 只生成 experimental OpenVDB report，不生成生产 RGBWSV 包。 |
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

## 9. Legacy / Global 双模式生产操作

### 9.1 入口位置

打开中央“配置”页，在“生产切片模式”区域选择：

| 选项 | 行为 |
|---|---|
| `传统切片` | 默认模式；沿用当前场景/Profile 的 RGB、白墨、支撑和光油设置。 |
| `全局纹理壳层` | 显式 opt-in；必须再选择一个已准入 Global Profile。 |

Global Profile 当前包括：

| Profile | 能力 |
|---|---|
| `全局纹理壳层（受限材料）` | RGB + 白墨；不生成支撑和光油。 |
| `全局纹理壳层（材料一致）` | RGB + 白墨；lower/internal-void 支撑；surface/outer 光油。 |

Global 下由 Profile 管理的材料、支撑和光油控件会锁定。OpenVDB backend 不是第三种产品模式，
普通配置页不会把它暴露为产品模式选择。

### 9.2 一键切片

1. 在“配置”页选择 `传统切片` 或 `全局纹理壳层`。
2. Global 模式下选择目标 Global Profile。
3. 点击左侧“导入模型并切片”，从任意目录选择 OBJ/STL/3MF。
4. UI 生成当前 session 的 `slice_config.effective.json`。
5. 模型预检和生产准入通过后启动 `slicer_cli`。
6. 完成后仅加载本次 session 的 package。

也可以先选择“场景/Profile”，再点击“运行切片”。两种入口都经过同一 Effective Config、
preflight、ProcessRunner 和 package identity 校验。

### 9.3 结果校验与显示

“生产切片模式”区域在本次运行后显示：

```text
requested/effective 模式；
sessionId；
production TIFF 是否写入；
fallback 是否发生；
当前 package 路径；
本次总耗时；
本次峰值工作集。
```

只有以下条件全部满足时才加载预览和报告：

```text
schema = p0.rgbwsv.2；
manifest 与 slice_report 模式一致；
模式与 UI 本次请求一致；
productionOutputWritten = true；
fallbackApplied = false；
source.configPath 与本次 Effective Config 一致；
preview/report 位于同一个当前 session package。
```

校验失败时不会回退到 Legacy，也不会加载上一次成功的旧包。

### 9.4 性能提示

Global 是高资源开销显式候选。2026-07-24 的 xiao_ma/yecan Release 矩阵中：

```text
Global 总耗时约为 Legacy 的 4.09x-5.92x；
Global 峰值内存约为 Legacy 的 8.19x-8.74x。
```

UI 显示的是本次 `slicer_cli` 实际总耗时和峰值工作集；平台不能采集时显示“未提供”，
不会拿历史倍数代替本次数据。Legacy 因此继续作为默认模式。

---

## 10. 运行方式

### 10.1 VS Code

在“运行与调试”中选择：

```text
SliceSoft: Debug Qt slicer_debug_ui
```

### 10.2 PowerShell

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

## 11. 常见问题

### 11.1 UI target 没生成

原因通常是 CMake 没找到 Qt5 Widgets。检查：

```powershell
cmake -S . -B build
```

如果输出包含：

```text
Qt5 Widgets not found; slicer_debug_ui target is skipped.
```

说明 CLI 仍可构建，但 UI 不会生成。

### 11.2 UI 中没有预览图

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

### 11.3 材料工艺面板为空

检查输出包是否存在：

```text
reports/material_process_report.json
```

只有启用 `materialProcessProfile.enabled = true` 的配置才会产生完整材料工艺报告。

### 11.4 运行切片后 UI 没自动加载输出包

UI 通过配置文件中的：

```json
"output": {
  "packageDir": "..."
}
```

推导输出包路径。若配置文件 JSON 解析失败或 `packageDir` 缺失，需要手动选择输出包目录。

---

## 12. 当前边界

当前 UI 仍不做：

- 生产任务系统。
- 设备通信。
- 喷头 bitstream。
- RIP 半色调。
- ICC / CMYK。
- 完整 3D 模型视图。
- 自动修复复杂 confirmed self-intersection 模型。
- 把 OpenVDB 暴露成第三种产品模式。
- 静默从 Global 回退到 Legacy。
- 解除 Global Profile 的材料能力锁定。

12E-09A diagnostic UI 和 12E-09C X/Y DPI 是后续独立任务，不属于 09B 已完成范围。
