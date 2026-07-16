# VSCode 与 Qt 调试 UI 运行环境说明

> 日期：2026-07-16
> 目的：说明统一后的 Debug/Release 构建与运行环境、VS Code 入口、Qt 调试 UI 场景选择器和本地环境清理任务。

## 1. 整理原则

VSCode 只保留开发者调试入口，不再为每个模型、每个配置、每个输出包复制一套 launch/task。

业务样例和 Profile 统一放到：

```text
samples/scenarios/slicer_scenarios.json
```

Qt 调试 UI 读取该索引，在“场景/Profile”下拉框中选择模型和配置；也可以从任意目录导入模型并生成临时配置。全量样例运行由脚本读取同一个索引。

## 2. VSCode 保留入口

主配置文件：

```text
.vscode/launch.json
.vscode/tasks.json
```

历史全量配置已归档：

```text
.vscode/launch.full.legacy.json
.vscode/tasks.full.legacy.json
```

### 2.1 运行与调试

| 配置 | 作用 |
|---|---|
| `SliceSoft: Debug Qt UI` | 构建并部署统一 Debug Runtime，然后启动 Qt 调试器。 |
| `SliceSoft: Run Release Qt UI` | 构建并部署 Release Runtime，以非调试方式启动。 |
| `SliceSoft: Debug slicer_cli current config` | 调试 `slicer_cli --config <输入配置>`。 |
| `SliceSoft: Debug slicer_cli inspect current config` | 调试模型统计/诊断入口。 |
| `SliceSoft: Debug slicer_cli preview-only current config` | 调试 preview-only 入口。 |
| `SliceSoft: Debug rip_reader current package` | 调试 `rip_reader_test --package <输入输出包> --summary`。 |

`current config` 和 `current package` 会通过 VSCode 输入框填写路径，默认值为：

```text
samples/configs/material_process/nail_rgb_white_varnish_top2.json
output/NailRgbWhiteVarnishTop2
```

### 2.2 任务

| Task | 作用 |
|---|---|
| `SliceSoft: Configure` | `cmake -S . -B build`。 |
| `SliceSoft: Build Debug` | 构建 Debug 全部默认 target。 |
| `SliceSoft: Build Debug UI` | 构建 UI/CLI/RIP reader 并部署 Debug Runtime。 |
| `SliceSoft: Prepare Release Runtime` | 构建 Release 并部署 Qt/MSVC 运行依赖。 |
| `SliceSoft: CTest Debug` | 运行 `ctest --test-dir build -C Debug --output-on-failure`。 |
| `SliceSoft: Run slicer_cli current config` | 用输入配置运行一次切片。 |
| `SliceSoft: Run rip_reader current package` | 用输入输出包运行 RIP 摘要。 |
| `SliceSoft: Run sample matrix` | 按场景索引运行全量样例矩阵。 |
| `SliceSoft: Run quick regression` | 执行 `scripts/run_regression.ps1 -Mode quick`。 |
| `SliceSoft: Run UI self-test` | 对部署后的 Debug Runtime 执行 UI 非交互自检。 |
| `SliceSoft: Run Release Runtime self-test` | 对部署后的 Release Runtime 执行 UI 非交互自检。 |
| `SliceSoft: Clean build directory` | 删除本地 `build` 目录。 |
| `SliceSoft: Clean output directory` | 删除本地 `output` 目录。 |
| `SliceSoft: Clean build and output` | 同时删除本地 `build` 与 `output`。 |

清理任务调用：

```text
scripts/clean_local_environment.ps1
```

脚本会校验删除目标必须位于当前仓库根目录下。

## 3. Qt 调试 UI 场景/Profile

UI 左侧新增：

```text
场景/Profile
```

选择场景后会自动填充：

```text
配置文件
输出包
```

并自动加载配置编辑器、层预览、报告、材料工艺面板和叠加预览。部署后的 UI 优先调用同目录工具：

```powershell
runtime/slicesoft/<Config>/slicer_cli.exe --config <config>
```

点击“运行 RIP 摘要”调用：

```powershell
runtime/slicesoft/<Config>/rip_reader_test.exe --package <package> --summary
```

点击“导入模型并切片”会从任意目录选择 OBJ/STL/3MF，生成：

```text
output/ui_sessions/<模型名_时间戳>/slice_config.generated.json
output/ui_sessions/<模型名_时间戳>/package
```

点击“导入模型并 OpenVDB 诊断”会生成同类临时配置，并输出 experimental OpenVDB report；该入口当前不写生产 RGBWSV package。

## 4. 统一构建与运行目录

构建目录：

```text
build-slicesoft/Debug
build-slicesoft/Release
```

运行目录：

```text
runtime/slicesoft/Debug
runtime/slicesoft/Release
```

统一脚本：

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Debug
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release
```

脚本使用 Visual Studio x64 Developer Environment 和 NMake，构建 `slicer_cli`、`rip_reader_test`、`slicer_debug_ui`，再通过 `windeployqt` 部署 Qt DLL、platform plugin 和 MSVC runtime。

运行包同时包含：

```text
samples/scenarios/     场景索引
samples/configs/       Profile 配置
samples/models/        samples 内置模型与纹理
model/                 Profile 引用的产品样例模型与纹理
docs/slice/PRD/        场景索引引用的 Profile 说明
```

发布前脚本会逐项校验场景索引、默认场景、配置文件、模型路径、输出路径和说明文档。配置中的模型与输出路径必须是运行包内部可解析的相对路径。`runtime_manifest.json` 记录构建类型、Qt 路径、OpenVDB 状态、场景数量和默认场景。

Release UI 检测到同目录存在 `samples/scenarios/slicer_scenarios.json` 时，会将 EXE 所在目录作为应用资源根目录。因此即使从快捷方式或其他工作目录启动，场景/Profile 仍从运行包内部加载。

仅重新发布已有编译产物时可以执行：

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release -DeployOnly
```

历史 `Configure12CQtUi.ps1` 和 `build-12c-ui` 保留用于 12C fresh gate 追溯，不再作为第二个日常 VS Code Qt Debug 环境。

## 5. 场景索引字段

路径：

```text
samples/scenarios/slicer_scenarios.json
```

关键字段：

| 字段 | 作用 |
|---|---|
| `schema` | 当前固定为 `slice_soft.scenarios.1`。 |
| `defaultScenarioId` | UI 默认选中的场景。 |
| `id` | 场景唯一标识。 |
| `name` | UI 显示名称。 |
| `category` | UI 显示分组。 |
| `configPath` | 切片配置 JSON。 |
| `packageDir` | 输出包目录。 |
| `description` | UI 中显示的说明。 |
| `enabled` | 是否进入 UI 和默认矩阵。 |
| `experimental` | 是否为实验场景。 |
| `requiresOpenVdb` | 是否需要 OpenVDB 环境。 |
| `ripSummary` | 矩阵脚本是否默认执行 RIP 摘要。 |

OpenVDB 场景默认不启用，不会影响普通构建和 UI 默认路径。

## 6. 全量样例矩阵

脚本：

```text
scripts/run_sample_matrix.ps1
```

常用命令：

```powershell
.\scripts\run_sample_matrix.ps1
.\scripts\run_sample_matrix.ps1 -ScenarioId three_mf_real_03
.\scripts\run_sample_matrix.ps1 -Category "3MF"
.\scripts\run_sample_matrix.ps1 -DryRun
```

默认行为：

- 读取 `samples/scenarios/slicer_scenarios.json`。
- 构建 Debug。
- 运行所有 `enabled=true` 且非实验场景。
- 对 `ripSummary != false` 的输出包执行 RIP 摘要。

## 7. 何时使用哪个入口

| 需求 | 推荐入口 |
|---|---|
| 调试 UI 代码 | VSCode `Debug Qt UI`。 |
| 验证真实运行性能 | VSCode `Run Release Qt UI` 或 Release Runtime。 |
| 调试一个任意配置的 CLI | VSCode `Debug slicer_cli current config`。 |
| 从任意目录导入模型并切片 | Qt 调试 UI `导入模型并切片`。 |
| OpenVDB 当前能力检查 | Qt 调试 UI `导入模型并 OpenVDB 诊断`。 |
| 日常切片、预览、报告查看 | Qt 调试 UI。 |
| 批量跑样例 | `scripts/run_sample_matrix.ps1` 或 VSCode `Run sample matrix`。 |
| 协议/回归验证 | `scripts/run_regression.ps1` / `scripts/run_ci_quick.ps1`。 |
| 清理本地构建或输出 | VSCode clean task 或 `scripts/clean_local_environment.ps1`。 |

## 8. 边界

本次整理不改变：

```text
p0.rgbwsv.2
RGBWSV channelOrder
uint8 bitDepth
black_is_print polarity
slicer_cli 生产输出路径
OpenVDB 默认关闭策略
```

VSCode 的历史全量调试项保留在 legacy 文件中，只作为追溯参考，不再作为日常入口。
