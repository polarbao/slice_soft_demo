# DEV_08_支撑形态与工艺优化设计

> 文档版本：v0.1  
> 阶段：08  
> 建议目录：`docs/slicer/`

## 1. 技术目标

在现有 support 生成后增加轻量形态优化层：

```text
SupportGenerationResult
→ SupportShapeOptimizer
→ OptimizedSupportResult
→ ComposeMaterialChannels
```

该阶段只能改变支撑 mask 中的空白区域，不得覆盖 model mask。

## 2. 推荐新增模块

```text
src/slicer_core/support/SupportShapePolicy.h
src/slicer_core/support/SupportShapePolicy.cpp
src/slicer_core/support/SupportComponentAnalysis.h
src/slicer_core/support/SupportComponentAnalysis.cpp
src/slicer_core/support/SupportShapeOptimizer.h
src/slicer_core/support/SupportShapeOptimizer.cpp
src/slicer_core/support/SupportShapeReport.h
src/slicer_core/support/SupportShapeReport.cpp
```

## 3. SupportShapePolicy

```cpp
struct SupportShapePolicy {
    bool enabled{false};
    int min_component_area_px{0};
    int xy_dilation_px{0};
    int closing_radius_px{0};
    int bridge_gap_px{0};
    bool preserve_model_priority{true};
    double max_added_support_ratio{0.25};
};
```

解析来源：

```text
support.shape.*
```

也可兼容已有字段：

```text
support.minIslandAreaPx
support.xyDilationPx
support.connectivity
```

## 4. Component Analysis

输入：

```text
supportMask
modelMask
width
height
connectivity
```

输出：

```text
component_count
largest_component_area
small_component_count
tiny_component_count
components[].bbox
```

## 5. Optimizer 阶段

### 小岛过滤

```text
component.area < minComponentAreaPx → clear component
```

### XY Dilation

```text
只允许写入 empty pixels
不得写入 model pixels
```

### Closing

第一版可简化为 dilation + small hole fill，必须受 modelMask 约束。

### Bridge Gap

第一版只做水平/垂直方向小间隙，不做复杂路径搜索。

## 6. Report

新增：

```json
{
  "schema": "p0.support_shape_report.1",
  "enabled": true,
  "policy": {},
  "pre": {},
  "post": {},
  "filteredComponents": [],
  "bridgedGaps": [],
  "addedSupportPixels": 0,
  "removedSupportPixels": 0,
  "warnings": []
}
```

## 7. 集成点

```text
GenerateSupport 之后
ComposeMaterialChannels 之前
```

必须保持：

```text
if modelMask[pixel] == true:
    supportMask[pixel] = false
```
