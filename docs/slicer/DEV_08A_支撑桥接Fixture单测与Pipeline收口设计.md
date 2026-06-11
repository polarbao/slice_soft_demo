# DEV_08A_支撑桥接Fixture单测与Pipeline收口设计

> 文档版本：v0.1
> 适用阶段：08A
> 建议提交目录：`docs/slicer/`

## 1. 技术目标

08A 在 08 基础上做三类收口：

```text
bridge gap fixture；
C++ unit test target；
pipeline/support module wrapper 接入。
```

## 2. Bridge Gap Fixture

新增配置：

```text
samples/configs/support/support_bridge_gap_smoke.json
```

如已有模型难以稳定制造 gap，可以新增最小测试模型：

```text
samples/models/support/support_bridge_gap.obj
```

建议配置：

```text
support.shape.enabled = true
support.shape.bridgeGapPx = 2
support.shape.xyDilationPx = 0 或 1
support.shape.closingRadiusPx = 0
support.shape.minComponentAreaPx = 0
```

目标是让 `bridgedGaps` 主要由 bridge 产生，而不是 dilation/closing 混淆。

## 3. Unit Test Target

新增：

```text
tests/unit/support_shape/main.cpp
support_shape_unit_tests
```

第一版可使用简单 assert/return code，不必引入 GoogleTest。

测试用例：

```text
component_analysis_single_component
component_analysis_two_components
filter_small_component
dilation_preserve_model_priority
bridge_horizontal_gap
bridge_vertical_gap
max_added_ratio_rollback
```

## 4. Test API

可通过单层 vector 调用现有多层 API，或新增轻量 helper：

```cpp
SupportShapeOptimizationResult OptimizeSupportShapeForLayer(
    const SupportShapePolicy& policy,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    int width,
    int height,
    int connectivity);
```

## 5. Pipeline Wrapper 接入

当前 08 集成顺序是：

```text
GenerateSupport
→ OptimizeSupportShape
→ SynchronizeSupportShapeTypeMaps
→ RecalculateSupportGenerationStats
→ ComposeMaterialChannels
```

08A 建议进一步封装：

```text
src/slicer_core/support/SupportShapePipeline.*
```

或在现有 support wrapper 中新增 facade：

```cpp
SupportShapeOptimizationResult ApplySupportShapePolicy(...);
```

目标是减少 `slicer.cpp` 直接知道过多 optimizer 细节。

## 6. Report / Schema / Golden

新增 golden case：

```text
support_bridge_gap_smoke
```

比较字段：

```text
support_shape_report.schema
support_shape_report.addedSupportPixels
support_shape_report.bridgedGaps count
supportPixels
modelPixels
manifest schema
```
