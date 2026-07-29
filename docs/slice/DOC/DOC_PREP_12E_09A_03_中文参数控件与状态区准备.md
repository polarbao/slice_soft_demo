# DOC_PREP 12E-09A-03 中文参数控件与状态区准备

> 日期：2026-07-29
> 状态：IMPLEMENTED
> 前置：12E-09A-01/02、13C-03、13D-04 COMPLETE

## 1. 任务边界

本任务只在 13D 单一 `ContextInspector` 的“切片设置”页增加诊断参数和只读状态，不启动
拓扑、距离场、宽度扫描、纹理转移或栅格映射，不写生产 Profile、Package 或 TIFF。

```text
可编辑：
  Texture Surface Layer 诊断宽度；
  Model Fill 诊断材料。

只读：
  当前场景/实例/revision；
  工程最小值、模型最大值、全纹理阈值；
  Legacy CPU/OpenVDB 候选后端可用状态；
  pending/blocked/unavailable 状态和阻断原因。
```

## 2. 控件合同

```text
QDoubleSpinBox：
  单位 mm；
  两位小数；
  步长 0.01 mm；
  预分析工程上限 6.00 mm；
  最小值 max(0.10, 2 * max(pixelPitchX, pixelPitchY, layerThickness))。

QSlider：
  整数范围 10..600；
  1 格等于 0.01 mm；
  与 SpinBox 双向同步。

Model Fill：
  white / varnish / rgb；
  不提前暴露当前切片核心尚未支持的独立 C/M/Y/K 填充材料。
```

模型最大宽度与全纹理阈值必须由 09A-04 分析结果提供。未分析时显示“未评估”，不能用 `0`
冒充测量结果。当前 DPI/层高可确定的两单元最小值会立即限制 SpinBox/Slider。

## 3. 状态规则

```text
无模型：控件禁用，显示“等待导入或选择模型”；
导入中：控件禁用；
模型 ready：控件可编辑，显示等待异步分析；
模型 blocked：控件仍可编辑以便诊断，但明确不得冒充生产准入；
模型 failed：控件禁用并显示失败原因；
OpenVDB 工具不存在：只显示候选后端不可用，不影响 Legacy CPU。
```

诊断控件只保存当前 UI 会话内的 requested 值。09A-04 在启动分析前使用 09A-02 的
`DiagnosticEffectiveConfig` 原子事务冻结 identity、requested、derived、effective 和 configHash。

## 4. 文件所有权

```text
apps/slicer_debug_ui/widgets/DiagnosticSettingsPanel.*；
apps/slicer_debug_ui/widgets/ContextInspector.*；
apps/slicer_debug_ui/MainWindow.*；
apps/slicer_debug_ui/services/UiSmokeTestRunner.*；
apps/slicer_debug_ui/CMakeLists.txt。
```

不修改：

```text
slicer_core 切片算法；
RGBWSV TIFF 协议；
09B Production Profile；
OpenVDB production admission；
13C TIFF 原生预览。
```

## 5. 验证

```powershell
cmake --build build --config Debug --target slicer_debug_ui --parallel
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe `
  --ui-smoke-test --case diagnostic-settings-controls
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

Smoke 必须覆盖双向同步、0.01 mm、中文材料、无模型不可用状态、最长中文和
1280x720/1440x900/1920x1080。
