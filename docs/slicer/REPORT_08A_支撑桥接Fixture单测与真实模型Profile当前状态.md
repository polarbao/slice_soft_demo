# REPORT_08A_支撑桥接Fixture单测与真实模型Profile当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-11  
> 适用阶段：08A

---

## 1. 阶段结论

08A 已完成支撑桥接 Fixture、C++ 单测、Pipeline facade、真实 3MF 可选 profile 与回归脚本收口。

本阶段没有修改 RGBWSV 输出协议：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF channel
```

证据等级：

- [A] 当前代码新增 `SupportShapePipeline` facade。
- [A] 当前代码新增并通过 `support_shape_unit_tests`。
- [A] `support_bridge_gap_smoke` 可生成 package 且 RIP 校验通过。
- [A] `support_shape_report.bridgedGaps` 稳定非零。
- [A] 真实 3MF 可选 profile `three_mf_real_01_support_shape.json` 可生成 package 且 RIP 校验通过。
- [A] `run_support_shape_tests.ps1`、`run_schema_tests.ps1`、`run_golden_tests.ps1`、`run_ci_quick.ps1` 已通过。

---

## 2. 新增与修改内容

### 2.1 Bridge Gap Fixture

新增：

```text
samples/models/support/support_bridge_gap.obj
samples/configs/support/support_bridge_gap_smoke.json
```

fixture 设计：

- 两个浮空小块在 XY 平面形成两个独立支撑岛。
- 间隙约为 2 px。
- `xyDilationPx = 0`。
- `closingRadiusPx = 0`。
- `bridgeGapPx = 2`。

这样 bridge 行为不会被 dilation / closing 混淆。

验证输出：

```text
packageDir: output/SupportBridgeGapSmoke
grid: 21 x 12 x 12
modelPixels: 912
supportPixels: 2016
support_shape_report.addedSupportPixels: 192
support_shape_report.removedSupportPixels: 0
support_shape_report.bridgedGaps.length: 96
```

---

## 3. C++ Unit Test Target

新增：

```text
tests/unit/support_shape/main.cpp
support_shape_unit_tests
```

覆盖用例：

```text
component_analysis_single_component
component_analysis_two_components
filter_small_component
dilation_preserve_model_priority
bridge_horizontal_gap
bridge_vertical_gap
max_added_ratio_rollback
```

运行结果：

```text
PASS component_analysis_single_component
PASS component_analysis_two_components
PASS filter_small_component
PASS dilation_preserve_model_priority
PASS bridge_horizontal_gap
PASS bridge_vertical_gap
PASS max_added_ratio_rollback
Support shape unit tests complete.
```

---

## 4. Pipeline Facade

新增：

```text
src/slicer_core/support/SupportShapePipeline.h
src/slicer_core/support/SupportShapePipeline.cpp
```

新增 API：

```cpp
SupportShapeOptimizationResult ApplySupportShapePolicy(...);
SupportShapeOptimizationResult OptimizeSupportShapeForLayer(...);
```

当前作用：

- `slicer.cpp` 不再直接调用 `OptimizeSupportShape`。
- `slicer.cpp` 通过 support facade 调用支撑形态优化。
- 单层测试通过 `OptimizeSupportShapeForLayer` 调用相同逻辑。

未做内容：

- 未重写 support generation。
- 未改变 `SupportType` 写入规则。
- 未把 report 写入逻辑放入 support 模块。

---

## 5. Schema / Golden / CI 接入

修改：

```text
scripts/run_support_shape_tests.ps1
scripts/run_schema_tests.ps1
scripts/run_golden_tests.ps1
scripts/run_ci_quick.ps1
tests/golden/expected/r2_golden_summaries.json
```

新增 golden case：

```text
support_bridge_gap_smoke
```

golden 比较字段：

```text
manifest schema
grid width / height / layer count
modelPixels
supportPixels
supportShapeAddedPixels
supportShapeRemovedPixels
supportShapeBridgedGaps
```

`.gitignore` 已调整为继续忽略生成测试包，同时允许跟踪：

```text
tests/golden/**
tests/unit/**
```

---

## 6. 真实 3MF Support Shape Profile

新增：

```text
samples/configs/3mf/three_mf_real_01_support_shape.json
```

该配置不替换原始：

```text
samples/configs/3mf/three_mf_real_01.json
```

验证输出：

```text
packageDir: output/ThreeMfReal01SupportShape
grid: 292 x 533 x 631
modelPixels: 11779674
supportPixels: 41081734
support_shape_report.schema: p0.support_shape_report.1
support_shape_report.enabled: true
support_shape_report.addedSupportPixels: 195145
support_shape_report.removedSupportPixels: 3707
support_shape_report.bridgedGaps.length: 4
support_shape_report.layers.length: 457
```

RIP 校验：

```text
rip_reader_test: PASS
schema: p0.rgbwsv.2
bitDepth: 8
channelOrder: R G B W S V
channelPrintPixels: R=11779674 G=11779674 B=11779674 W=0 S=41081734 V=0
warnings: 0
```

---

## 7. 已运行验证

已运行并通过：

```powershell
cmake --build build --config Debug
.\build\Debug\support_shape_unit_tests.exe
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_bridge_gap_smoke.json
.\build\Debug\rip_reader_test.exe --package output\SupportBridgeGapSmoke --summary
.\build\Debug\slicer_cli.exe --config samples\configs\3mf\three_mf_real_01_support_shape.json
.\build\Debug\rip_reader_test.exe --package output\ThreeMfReal01SupportShape --summary
.\scripts\run_support_shape_tests.ps1
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\scripts\run_ci_quick.ps1
```

`run_ci_quick.ps1` 最终结果：

```text
CI quick complete.
```

说明：

- `run_ci_quick.ps1` 会执行 `make_3mf_samples.ps1`，因此会重新生成部分 `samples/models/3mf/*.3mf` fixture。
- 这些 3MF fixture 变化来自现有验证脚本副作用，不属于 08A 支撑算法改动。

---

## 8. 当前限制

08A 仍保持 08 阶段算法边界：

- Bridge gap 只支持水平/垂直短间隙。
- Closing 仍为轻量简化版，不是完整数学形态学 closing。
- 支撑形态优化仍基于 2D layer mask。
- 未引入 SDF / OpenVDB。
- 未实现树状支撑。
- 未做设备通信或 RIP 半色调。
- 未新增生产级 Qt UI。

---

## 9. 是否可以进入 09

可以进入 09，但建议将 09 明确限定为：

```text
OpenVDB / SDF 几何内核预研
```

进入 09 前建议保持以下边界：

- 不直接替换当前 2D support pipeline。
- 不改变 RGBWSV 输出协议。
- 不把 OpenVDB/SDF 结果直接写入生产输出，先做独立预研模块和 fixture。
- 保留 08A bridge fixture 与 unit test 作为旧 pipeline 回归守门。
