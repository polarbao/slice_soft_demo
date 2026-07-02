# VSCode 与 Qt 调试 UI 运行环境说明

> 日期：2026-07-02
> 目的：说明精简后的 VSCode 编译/调试入口、Qt 调试 UI 场景选择器、全量样例矩阵脚本和本地环境清理任务。

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
| `SliceSoft: Debug Qt UI` | 构建并启动 Qt 调试 UI。 |
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
| `SliceSoft: Build Debug UI` | 只构建 `slicer_debug_ui`。 |
| `SliceSoft: CTest Debug` | 运行 `ctest --test-dir build -C Debug --output-on-failure`。 |
| `SliceSoft: Run slicer_cli current config` | 用输入配置运行一次切片。 |
| `SliceSoft: Run rip_reader current package` | 用输入输出包运行 RIP 摘要。 |
| `SliceSoft: Run sample matrix` | 按场景索引运行全量样例矩阵。 |
| `SliceSoft: Run quick regression` | 执行 `scripts/run_regression.ps1 -Mode quick`。 |
| `SliceSoft: Run UI self-test` | 执行 UI 非交互自检。 |
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

并自动加载配置编辑器、层预览、报告、材料工艺面板和叠加预览。点击“运行切片”仍然调用：

```powershell
build/Debug/slicer_cli.exe --config <config>
```

点击“运行 RIP 摘要”调用：

```powershell
build/Debug/rip_reader_test.exe --package <package> --summary
```

点击“导入模型并切片”会从任意目录选择 OBJ/STL/3MF，生成：

```text
output/ui_sessions/<模型名_时间戳>/slice_config.generated.json
output/ui_sessions/<模型名_时间戳>/package
```

点击“导入模型并 OpenVDB 诊断”会生成同类临时配置，并输出 experimental OpenVDB report；该入口当前不写生产 RGBWSV package。

## 4. 场景索引字段

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

## 5. 全量样例矩阵

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

## 6. 何时使用哪个入口

| 需求 | 推荐入口 |
|---|---|
| 调试 UI 代码 | VSCode `Debug Qt UI`。 |
| 调试一个任意配置的 CLI | VSCode `Debug slicer_cli current config`。 |
| 从任意目录导入模型并切片 | Qt 调试 UI `导入模型并切片`。 |
| OpenVDB 当前能力检查 | Qt 调试 UI `导入模型并 OpenVDB 诊断`。 |
| 日常切片、预览、报告查看 | Qt 调试 UI。 |
| 批量跑样例 | `scripts/run_sample_matrix.ps1` 或 VSCode `Run sample matrix`。 |
| 协议/回归验证 | `scripts/run_regression.ps1` / `scripts/run_ci_quick.ps1`。 |
| 清理本地构建或输出 | VSCode clean task 或 `scripts/clean_local_environment.ps1`。 |

## 7. 边界

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
