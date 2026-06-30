# DEV_02_支撑孤岛检测与SupportType设计_v0.2

> 文档版本：v0.2  
> 文档状态：Draft / DEV 强化版  
> 适用阶段：PRD_02  
> 所属模块：Slicer / Support  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

在当前 bottom_projection 基础上实现：

```text
unsupported_only
bottom_projection_plus_unsupported
connected component island detection
SupportType metadata
support report 增强
```

不改变：

```text
RGBWSV TIFF 协议
S 通道语义
Model > Support > Empty
```

---

## 2. 建议模块边界

短期可以在 `slicer.cpp` 中实现，但建议逐步拆出：

```text
src/slicer_core/support/
  support_config.*
  support_generator.*
  island_detector.*
  support_report.*
```

阶段 02 不强制重构目录，但代码应保持函数边界清晰。

---

## 3. 配置结构

### 3.1 SupportMode

```cpp
enum class SupportMode {
    BottomProjection,
    UnsupportedOnly,
    BottomProjectionPlusUnsupported,
    FullVerticalProjection
};
```

### 3.2 SupportConfig

```cpp
struct SupportConfig {
    bool enabled{true};
    SupportMode mode{SupportMode::BottomProjection};
    std::uint8_t value{0};

    double min_overlap_ratio{0.2};
    int min_island_area_px{16};
    int connectivity{8};

    std::string unsupported_projection{"project_to_build_plate"};
    int xy_dilation_px{0};
    bool write_support_type_debug{true};
};
```

兼容 JSON：

```json
{
  "support": {
    "mode": "bottom_projection_plus_unsupported",
    "minOverlapRatio": 0.2,
    "minIslandAreaPx": 16,
    "connectivity": 8,
    "unsupportedProjection": "project_to_build_plate"
  }
}
```

---

## 4. 内部数据结构

### 4.1 Mask2D

继续使用当前 2D mask 表示每层 model / support。

### 4.2 SupportType

```cpp
enum class SupportType : std::uint8_t {
    None = 0,
    BottomProjection = 1,
    UnsupportedIsland = 2,
    FullVerticalProjection = 3
};
```

### 4.3 SupportTypeMap

可选 debug map：

```cpp
std::vector<SupportType> support_type_map(width * height);
```

注意：

```text
SupportTypeMap 不写入生产 TIFF。
```

### 4.4 IslandComponent

```cpp
struct IslandComponent {
    int layer_index;
    int component_id;
    int area_px;
    int overlap_px;
    double overlap_ratio;
    bool filtered;
    std::vector<int> pixels;
};
```

### 4.5 SupportGenerationResult

```cpp
struct SupportGenerationResult {
    std::vector<Mask2D> support_masks;
    std::vector<SupportTypeMap> support_type_maps;
    SupportReport report;
};
```

---

## 5. 执行管线

### 5.1 输入

```text
model_masks
config.support
optional relief column info
```

### 5.2 输出

```text
support_masks
support_type_maps
support_report
```

### 5.3 推荐流程

```text
1. 初始化 support_masks 为空。
2. 如果 mode 包含 bottom_projection：
     生成 bottom projection support。
3. 如果 mode 包含 unsupported：
     逐层做 island detection。
     对 island 生成 unsupported support。
4. 合并 support mask。
5. 输出 support report。
6. compose_layer 时按 Model > Support 写入 TIFF。
```

---

## 6. Bottom Projection

### 6.1 closed_mesh_scanline

可继续使用：

```text
first_model_layers
```

### 6.2 relief_heightfield

优先使用：

```text
ReliefColumnInfo.lower_layer
```

不要再用 `surface_to_base` 的 base 区间误判支撑。

---

## 7. Island Detection 算法

### 7.1 Connected Components

对当前层 `model_mask[layer]` 做连通域。

连通性：

```text
4-neighborhood
8-neighborhood
```

默认：

```text
8-neighborhood
```

### 7.2 支撑基础

```text
base_mask = model_masks[layer - 1] OR support_masks[layer - 1]
```

可选扩张：

```text
base_mask = dilate(base_mask, xy_dilation_px)
```

### 7.3 Overlap Ratio

```text
overlapRatio = count(component AND base_mask) / component.area
```

判断：

```text
if overlapRatio < min_overlap_ratio:
    island
```

小岛过滤：

```text
if component.area < min_island_area_px:
    filtered
```

---

## 8. Unsupported Support 生成

### 8.1 project_to_build_plate

第一版推荐实现。

对 island component 的 XY footprint：

```text
for z in [0, layer_index):
    if !model_masks[z][xy]:
        support_masks[z][xy] = true
        support_type_maps[z][xy] = UnsupportedIsland
```

注意：

```text
不得覆盖 model mask。
```

### 8.2 project_to_nearest_supported_layer

可作为后续增强，不强制实现。

---

## 9. SupportType 合并规则

如果同一像素已经有 bottom_projection 支撑，又被 island 支撑覆盖，建议优先级：

```text
UnsupportedIsland > BottomProjection > FullVerticalProjection
```

生产 TIFF 中仍只体现：

```text
S = 0
```

report 中统计 SupportType。

---

## 10. Report 设计

### 10.1 support_report.json

建议 schema：

```json
{
  "enabled": true,
  "mode": "bottom_projection_plus_unsupported",
  "value": 0,
  "minOverlapRatio": 0.2,
  "minIslandAreaPx": 16,
  "connectivity": 8,
  "unsupportedProjection": "project_to_build_plate",
  "totals": {
    "supportPixels": 0,
    "islandCount": 0,
    "islandPixels": 0,
    "unsupportedPixels": 0,
    "filteredIslandCount": 0,
    "filteredIslandPixels": 0
  },
  "supportTypeStats": {
    "bottom_projection": 0,
    "unsupported_island": 0,
    "full_vertical_projection": 0
  },
  "layers": []
}
```

### 10.2 slice_report.json

每层增加：

```json
{
  "islandCount": 0,
  "islandPixels": 0,
  "unsupportedPixels": 0,
  "filteredIslandPixels": 0,
  "supportTypeStats": {}
}
```

---

## 11. Preview

新增 debug preview：

```text
preview/island_mask_*.png
preview/unsupported_mask_*.png
preview/support_type_*.png
```

如当前不实现新增 preview，也必须先实现 report 字段。

---

## 12. 回归测试

新增：

```text
samples/models/support/
samples/configs/support/
```

建议模型：

```text
floating_island.stl
bridge_test.stl
stepped_overhang.stl
```

建议配置：

```text
support_bottom_projection.json
support_unsupported_only.json
support_bottom_plus_unsupported.json
support_island_filter.json
```

---

## 13. 兼容要求

必须保证：

```text
普通 P0 配置通过
Relief V + S 配置通过
Relief W + S 配置通过
RGB no support 配置通过
rip_reader_test 全部通过
```

---

## 14. 实施顺序

```text
1. SupportMode 配置解析
2. Connected components
3. Island detection
4. unsupported_only support generation
5. bottom_projection_plus_unsupported
6. support_report 增强
7. slice_report 增强
8. support 样例配置
9. 回归验证
10. REPORT_02
```

---

## 15. 结论

DEV_02 v0.2 重点是建立支撑基础设施：

```text
可检测
可生成
可统计
可回归
```

不是复杂支撑结构优化。
