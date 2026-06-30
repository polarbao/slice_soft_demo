# DEV_01_relief_heightfield正式切片设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 适用阶段：PRD_01  
> 所属模块：Slicer / Relief  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

将 00C 中的 `relief_heightfield` 简化实现整理为正式模块。

当前 00C 已实现：

```text
slicingMode = relief_heightfield
relief.fillMode = surface_to_base / intersection_range
relief.baseZMm
materialChannel = V / W / RGB / auto
support bottom_projection
relief_report.json
manifest.slicing
```

DEV_01 目标是使该模块具备：

```text
更清晰的数据结构
更强的诊断报告
更稳定的测试样例
更明确的扩展边界
```

---

## 2. 模块建议

建议将 relief 逻辑从通用 slicer 中逐步拆出：

```text
src/slicer_core/
  relief/
    relief_config.*
    relief_sampler.*
    relief_report.*
    relief_column.*
```

短期可不立即拆目录，但要在代码层明确函数边界。

---

## 3. 核心数据结构建议

### 3.1 ReliefConfig

```cpp
struct ReliefConfig {
    std::string fill_mode{"intersection_range"};
    double base_z_mm{0.0};
    std::string invalid_column_policy{"empty"};
    bool enable_diagnostics{true};
};
```

### 3.2 ReliefColumnInfo

```cpp
struct ReliefColumnInfo {
    bool has_model{false};
    int lower_layer{-1};
    int upper_layer{-1};
    double z_min_mm{0.0};
    double z_max_mm{0.0};
    int hit_count{0};
    bool multi_hit{false};
};
```

### 3.3 ReliefSamplingResult

```cpp
struct ReliefSamplingResult {
    std::vector<Mask2D> model_masks;
    std::vector<ReliefColumnInfo> columns;
    ReliefReport report;
};
```

---

## 4. relief sampler 设计

### 4.1 输入

```text
MeshModel
GridSpec
ReliefConfig
```

### 4.2 输出

```text
model_masks
column_info
relief_report
```

### 4.3 fillMode

#### intersection_range

```text
对每个 XY：
  z_min = column 最低命中
  z_max = column 最高命中
  填充 z_min..z_max 为模型
```

适合：

```text
拱形甲片
薄壳/有厚度浮雕
需要下表面支撑的模型
```

#### surface_to_base

```text
对每个 XY：
  z_max = column 最高命中
  填充 baseZ..z_max 为模型
```

适合：

```text
贴底浮雕
高度图类模型
无支撑实体填充
```

当前业务默认应使用：

```text
intersection_range
```

---

## 5. 支撑生成设计

### 5.1 当前方式

当前可复用：

```text
compute_first_model_layers(model_masks)
```

再由 `compose_layer` 生成：

```text
layer_index < first_model_layer → S 通道
```

### 5.2 正式建议

建议在 relief 模式中使用 `ReliefColumnInfo.lower_layer` 作为支撑源。

支撑条件：

```cpp
if (!is_model &&
    support.enabled &&
    column.has_model &&
    layer_index < column.lower_layer) {
    write_support_pixel();
}
```

### 5.3 报告字段

```json
{
  "supportSource": "relief_lower_surface",
  "supportPixels": 0,
  "columnsWithSupport": 0
}
```

---

## 6. Material 写入设计

保持 00C 逻辑：

```text
materialChannel = V:
  V = 0, others = 255

materialChannel = W:
  W = 0, others = 255

materialChannel = RGB:
  R/G/B = config.rgb, W/S/V = 255

materialChannel = auto:
  使用 rgb / whiteValue / varnishValue 直接写入
```

继续保持：

```text
Model > Support > Empty
```

---

## 7. Report 设计

### 7.1 relief_report.json

建议 schema：

```json
{
  "slicingMode": "relief_heightfield",
  "fillMode": "intersection_range",
  "baseZMm": 0.0,
  "columns": {
    "total": 0,
    "hit": 0,
    "empty": 0,
    "multiHit": 0,
    "coverageRatio": 0.0
  },
  "height": {
    "zMinMm": 0.0,
    "zMaxMm": 0.0,
    "thicknessMinMm": 0.0,
    "thicknessMaxMm": 0.0
  },
  "support": {
    "enabled": true,
    "source": "relief_lower_surface",
    "supportPixels": 0
  },
  "warnings": []
}
```

### 7.2 slice_report.json

建议逐层记录：

```text
modelPixels
supportPixels
varnishPrintPixels
whitePrintPixels
rgbPrintPixels
```

注意：后续应逐步把 `nonZeroPixels` 改为 `printPixels`，避免与 black_is_print 极性冲突。

---

## 8. Manifest 设计

Manifest 中建议固化：

```json
{
  "slicing": {
    "mode": "relief_heightfield",
    "reliefFillMode": "intersection_range"
  }
}
```

并继续保留：

```json
{
  "tiff": {
    "bitDepth": 8,
    "polarity": "black_is_print",
    "printValue": 0,
    "emptyValue": 255,
    "channelOrder": ["R", "G", "B", "W", "S", "V"]
  }
}
```

---

## 9. 测试建议

新增测试集：

```text
tests/relief/
  relief_varnish_support
  relief_white_support
  relief_rgb_no_support
  relief_surface_to_base
  relief_intersection_range
```

每个测试检查：

```text
manifest
relief_report
support_report
rip_reader_test
preview
关键通道像素统计
```

---

## 10. 暂不实现

DEV_01 不实现：

```text
UV texture
MTL 材质映射
top_surface_only
top_n_layers
OpenVDB
复杂支撑树
Qt UI
```

---

## 11. 结论

DEV_01 的重点不是大改算法，而是把 00C relief 能力整理为正式模块边界：

```text
ReliefConfig
ReliefColumnInfo
ReliefSampler
ReliefReport
Relief test suite
```
