# QT_DEBUG_UI_操作手册

> 日期：2026-07-02
> 适用程序：`slicer_debug_ui`
> 适用对象：切片 Demo 调试、样例验证、模型导入、层预览、报告查看。

## 1. 软件定位

`slicer_debug_ui` 是 SliceSoft 的本地调试 UI。它负责组织配置、调用命令行工具、加载输出包和展示报告，不直接实现喷头控制、RIP 半色调或设备通信。

固定输出协议仍为：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

## 2. 启动方式

PowerShell：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe
```

VSCode：

```text
SliceSoft: Debug Qt UI
```

## 3. 推荐工作流

### 3.1 使用已有场景

1. 在左侧“场景/Profile”选择一个场景。
2. UI 会自动填充“配置文件”和“输出包”。
3. 点击“运行切片”。
4. 切片完成后 UI 自动加载输出包。
5. 在“层预览”“报告”“曲线”“叠加预览”页检查结果。
6. 点击“运行 RIP 摘要”确认输出包协议兼容。

### 3.2 一键导入模型并切片

点击：

```text
导入模型并切片
```

然后选择任意目录下的：

```text
*.obj
*.stl
*.3mf
```

UI 会自动生成：

```text
output/ui_sessions/<模型名_时间戳>/slice_config.generated.json
output/ui_sessions/<模型名_时间戳>/package
```

并自动运行：

```powershell
build/Debug/slicer_cli.exe --config <generated_config>
```

如果选择的是 OBJ，全彩纹理要求：

```text
model.obj
model.mtl
texture.png / texture.jpg / ...
```

OBJ 中需要引用 MTL：

```text
mtllib model.mtl
```

MTL 中需要引用贴图：

```text
map_Kd texture.png
```

贴图文件可以放在 OBJ / MTL 同级目录或 MTL 内相对路径指向的位置。当前 importer 会按 OBJ/MTL 路径解析贴图。

### 3.3 OpenVDB 实验诊断

点击：

```text
导入模型并 OpenVDB 诊断
```

UI 会选择模型、生成临时配置，然后运行：

```powershell
slicer_cli --config <generated_config> --experimental-openvdb-shell --admission-mode diagnostic_only
```

当前该按钮只生成实验诊断报告：

```text
output/ui_sessions/<模型名_时间戳>_openvdb/reports/experimental_openvdb_shell_report.json
```

它不生成生产 RGBWSV 切片包。

## 4. 左侧区域说明

| 控件 | 作用 |
|---|---|
| 场景/Profile | 从 `samples/scenarios/slicer_scenarios.json` 选择样例。 |
| 配置文件 | 当前传给 `slicer_cli --config` 的 JSON。 |
| 输出包 | 当前加载的 RGBWSV package 目录。 |
| 对比包 A/B | 用于材料工艺 Profile 对比。 |
| 构建调试版 | 执行 `cmake --build build --config Debug`。 |
| 导入模型并切片 | 选择任意模型，生成临时配置，并执行 legacy production 切片。 |
| 导入模型并 OpenVDB 诊断 | 选择任意模型，生成临时配置，并执行 OpenVDB experimental diagnostic。 |
| 运行切片 | 运行当前配置文件。 |
| 运行 RIP 摘要 | 对当前输出包执行 RIP reader summary。 |
| 运行快速回归 | 执行 `scripts/run_regression.ps1 -Mode quick`。 |
| 对比工艺配置 | 对比两个输出包的材料工艺报告。 |
| 加载输出包 | 手动加载当前输出包路径。 |
| 打开输出目录 | 在文件管理器中打开当前输出包。 |

## 5. 中央页面说明

| 页面 | 作用 |
|---|---|
| 层预览 | 查看 RGB/W/S/V/occupancy/diagnostic 等 per-layer 视图。 |
| 报告 | 查看 manifest 和 reports JSON 摘要。 |
| 曲线 | 查看通道/层统计曲线。 |
| 配置 | 编辑当前配置 JSON 和常用参数。 |
| 叠加预览 | 查看 RGB 与支撑/白墨/光油伪彩叠加。 |
| 原始预览 | 查看输出包 preview 目录中的 PNG/PPM。 |

## 6. 常用配置建议

全彩 OBJ：

```text
场景/Profile: 纹理 OBJ / 彩色纹理浮雕 RGB
模板: samples/configs/textured/textured_relief_rgb.json
```

OBJ 全彩 + 白墨 + 光油：

```text
场景/Profile: 材料工艺 / OBJ 纹理 + RGB/W/V
模板: samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json
```

真实 3MF：

```text
场景/Profile: 3MF 真实模型 / 真实 3MF 01/02/03
```

UI smoke：

```text
场景/Profile: UI Smoke / UI 层预览 Smoke
```

## 7. 当前交互优化建议

当前 UI 已经比早期“配置文件 + 输出包”模式更清晰，但仍建议后续继续优化：

1. 把“导入模型并切片”升级为向导式流程：模型、材料策略、输出目录、预览设置四步。
2. 在导入 OBJ 后显示 MTL/贴图检测结果，包括贴图缺失、UV 缺失和 fallback 策略。
3. 将 OpenVDB 诊断按钮视觉上标为“实验”，避免误认为生产切片。
4. 在“运行切片”前显示当前执行引擎：Legacy / OpenVDB Diagnostic / Future OpenVDB Candidate。
5. 将常用配置页拆成“基础”“材料”“支撑”“预览”“实验”五组，减少参数堆叠。
6. 对模型高度、autoOrient 结果、输出层数给出运行前摘要。

## 8. 当前限制

当前 UI 不做：

```text
RIP 半色调
设备通信
喷头 bitstream
生产作业队列
完整 3D 模型编辑
OpenVDB production RGBWSV 输出
自动 mesh repair
```

OpenVDB 当前只能通过 UI 触发实验诊断，不应作为正式切片结果交付。
