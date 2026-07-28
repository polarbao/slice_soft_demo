# DOC_PREP 13C-04 Preview IO 收口准备

> 文档状态：COMPLETE
> 版本：v1.0
> 日期：2026-07-28
> 对应任务：13C-04
> 前置：13C-03 COMPLETE

## 1. 目标

在 13C-03 已建立 TIFF 原生生产预览后，关闭常规生产切片中重复写出的逐通道 preview 图像，
同时保留显式诊断输出和旧配置兼容能力。

本任务必须把三类概念分开：

```text
生产层：manifest/layers/*.tiff，始终由生产 writer 写出，不受 preview 配置控制；
自动诊断图：切片时按间隔写入 package/preview，由 preview output policy 控制；
按需导出：未来由 UI 从当前 TIFF buffer 导出到用户指定位置，不属于生产 package 自动写出。
```

13C-04 只收口“自动诊断图”。不实现新的按需导出 UI，也不允许通过 preview 设置关闭生产 TIFF。

## 2. 当前代码事实

```text
PreviewConfig.enabled 默认 false，但 UI SliceSettingsModel 默认 true；
RgbwsvProductionPreviewSpec.enabled 默认 true；
preview.enabled 同时被用户理解为“生产预览开关”，语义不清；
RgbwsvPackageWriter 在 enabled=true 时每个命中间隔的层写 RGB/W/S/V 四张图；
enabled=false 时 writer 已可不创建 preview 目录，但仍写空的 preview_report.json；
manifest 和 ProductionPackageResultValidator 已允许 preview.files 为空；
13C-03 生产预览只读取 manifest/layers TIFF，不读取 preview PNG；
旧 Overlay/Raw 诊断视图仍可读取显式生成的 preview 图。
```

因此本任务不需要重写 TIFF writer，只需要冻结输出策略、统一默认值、补齐迁移语义和无 preview
目录证据。

## 3. 配置合同

### 3.1 新增 `preview.outputPolicy`

稳定值：

```text
tiff_native：
  仅写生产 RGBWSV TIFF 和报告；
  不自动写 package/preview 逐通道图；
  是新建配置和 UI Profile 的默认值。

tiff_native_with_diagnostics：
  生产 TIFF 保持不变；
  额外按 interval/format/channels 等现有参数写诊断图；
  仅用于调试、回归和明确要求的工艺检查。
```

`PreviewConfig.enabled` 在代码内部继续作为“是否自动写诊断图”的兼容有效值，不代表生产预览或
生产 TIFF 是否可用。

### 3.2 旧 `preview.enabled` 迁移

读取优先级：

```text
存在 preview.outputPolicy：
  outputPolicy 为权威值；
  tiff_native -> enabled=false；
  tiff_native_with_diagnostics -> enabled=true。

不存在 preview.outputPolicy：
  preview.enabled=true -> 迁移为 tiff_native_with_diagnostics；
  preview.enabled=false 或字段缺失 -> 迁移为 tiff_native。
```

新 UI 生成的 effective config 同时写：

```json
{
  "preview": {
    "outputPolicy": "tiff_native",
    "enabled": false
  }
}
```

保留 `enabled` 是为了旧 CLI/脚本和人工配置兼容；两个字段必须一致。若显式
`outputPolicy` 与旧 `enabled` 冲突，`outputPolicy` 胜出，生成配置不得产生冲突值。

### 3.3 默认值

```text
PreviewConfig.output_policy = tiff_native；
PreviewConfig.enabled = false；
RgbwsvProductionPreviewSpec.enabled = false；
SliceSettingsModel.preview.enabled = false；
production_rgb_inspection 等显式诊断 Profile 可继续设为 true。
```

## 4. Writer 和报告合同

无论 output policy 为何：

```text
layers/*.tiff 必须完整；
manifest schema 仍为 p0.rgbwsv.2；
preview_report schema 仍为 p0.preview_report.1；
RIP strict 必须通过。
```

`preview_report.json` 和 `manifest.preview` 增加说明字段：

```text
outputPolicy；
productionSource = rgbwsv_tiff；
automaticDiagnosticImages = true/false。
```

`tiff_native`：

```text
不创建 preview 目录；
preview_report.files/generated 为空；
manifest.preview.files 为空；
previewWriteMs 接近 0；
PackageLoader 和生产 UI 仍可加载全部层。
```

`tiff_native_with_diagnostics`：

```text
保留当前 RGB/W/S/V 诊断图写出；
interval/format 和现有诊断路径保持兼容；
诊断图不进入生产 TIFF 协议。
```

## 5. UI 合同

配置页现有“生成预览”开关改为“自动生成诊断图”，默认关闭。帮助文本必须明确：

```text
关闭不会影响生产 TIFF 或生产预览；
开启会增加 package/preview 文件数量和保存耗时；
生产预览始终从 RGBWSV TIFF 读取；
诊断 Profile 可显式开启。
```

本任务不新增按需导出按钮；后续若实现，必须从当前 TIFF buffer 导出到 package 之外，不能重新
引入每层自动重复 IO。

## 6. 文件所有权

计划修改：

```text
src/slicer_core/config.h/.cpp；
src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.h/.cpp；
apps/slicer_debug_ui/services/SliceSettingsModel.cpp；
apps/slicer_debug_ui/services/EffectiveConfigGenerator.cpp；
apps/slicer_debug_ui/services/HelpTextProvider.cpp；
apps/slicer_debug_ui/widgets/QuickConfigPanel.cpp（仅必要文案）；
apps/slicer_debug_ui/services/UiSmokeTestRunner.h/.cpp；
相关 unit test、fixture、状态报告。
```

允许新增：

```text
tests/unit/preview_output_policy/；
samples/configs/ui_smoke/ui_tiff_native_no_preview.json。
```

不得修改：

```text
RGBWSV 通道顺序、位深、极性；
生产材料策略；
TIFF storageMode；
RIP Reader 协议；
13C-03 的生产/诊断一级入口。
```

## 7. 自动化验证

新增核心测试 `preview_output_policy_unit_tests`，覆盖：

```text
字段缺失默认 tiff_native；
旧 enabled=true/false 迁移；
新 outputPolicy 两个合法值；
新旧字段冲突时 outputPolicy 胜出；
非法 outputPolicy 稳定失败；
UI effective config 输出一致字段。
```

扩展共享 writer 测试：

```text
tiff_native 无 preview 目录；
preview report/manifest 文件列表为空；
TIFF 层数和 hash/协议不受影响；
tiff_native_with_diagnostics 继续写 RGB/W/S/V；
两个模式均通过 validate_slice_package/RIP strict。
```

新增 UI smoke：

```text
tiff-native-preview-no-png
```

必须读取真实无 preview 目录 package，并验证生产视图可以浏览首/中/末层和
`RGB + S + W + V`。

最低验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case tiff-native-preview-no-png --package output\UiSmokeTiffNativeNoPreview
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
git diff --check
```

## 8. IO 对比证据

同一 fixture 分别运行：

```text
outputPolicy=tiff_native；
outputPolicy=tiff_native_with_diagnostics。
```

报告至少记录：

```text
TIFF 文件数量和总字节数；
preview 文件数量和总字节数；
previewWriteMs；
tiffWriteMs；
outputWriteMs；
总耗时；
生产层 hash/统计是否一致。
```

时间只作为本机基线，不把一次运行的绝对值写成跨设备性能承诺。

## 9. 验收与停止条件

验收：

```text
新 UI/Profile 默认不写 preview 目录；
显式诊断策略仍可写旧诊断图；
旧 enabled 配置不失效；
无 preview 目录时生产 UI 完整可用；
生产 TIFF、manifest、RIP strict 不回归；
IO 文件数和 previewWriteMs 有可复核下降证据。
```

停止并记录：

```text
关闭诊断图会关闭 TIFF；
ProductionPackageResultValidator 强制要求 preview 文件非空；
生产 UI 仍读取 preview PNG；
必须修改 p0.rgbwsv.2 才能继续；
旧 enabled=true 无法保持诊断图兼容。
```

`13C-04 PASS -> 13C-05 READY`。

## 10. 2026-07-28 执行结论

```text
preview.outputPolicy 已实现 tiff_native / tiff_native_with_diagnostics；
新配置与 Qt Profile 默认 tiff_native，旧 enabled 配置保持迁移兼容；
Legacy、Global Surface Shell 和多模型共享 writer 均输出一致报告字段；
tiff_native 不创建 preview 目录，生产 TIFF 与诊断策略逐层 SHA-256 一致；
Qt 配置改为“自动生成诊断图”，默认关闭；
tiff-native-preview-no-png 在真实无 preview 目录 package 上通过；
Debug 全量构建、82/82 CTest、Qt self-test、Quick CI 和 RIP strict 通过。
```

状态：`13C-04 COMPLETE / 13C-05 READY`。
