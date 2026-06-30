# DEV_00C_relief_heightfield单材料浮雕切片设计

> 文档版本：v0.1  
> 文档状态：Draft / 00C 技术设计  
> 文档类型：DEV 增量设计  
> 所属模块：切片软件 / Slicer  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

在当前 P0/00B 代码基础上新增：

```text
slicingMode = relief_heightfield
```

用于单材料浮雕模型切片。

保持原有：

```text
slicingMode = closed_mesh_scanline
```

作为普通闭合实体模型路径。

---

## 2. 当前代码问题

当前模型 mask 生成路径：

```text
slice_triangles_to_segments
→ rasterize_segments
→ sample_model_masks
```

该方法对浮雕模型存在问题：

```text
1. 开口/薄壳模型可能出现 odd scanline intersections
2. 局部非闭合轮廓会导致填充缺失
3. 浮雕中高层复杂轮廓 segment 数量很高
4. 不能自然表达每个 XY 位置的高度场
```

因此 00C 不建议继续在该路径上硬修浮雕模型，而是新增 relief path。

---

## 3. 配置结构设计

### 3.1 SliceConfig 新增字段

建议在 `SliceConfig` 中加入：

```cpp
std::string slicing_mode{"closed_mesh_scanline"};
```

配置字段：

```json
{
  "slicingMode": "relief_heightfield"
}
```

允许值：

```text
closed_mesh_scanline
relief_heightfield
```

---

### 3.2 MaterialConfig 新增字段

当前已有：

```cpp
std::array<std::uint8_t, 3> rgb;
std::uint8_t white_value;
std::uint8_t varnish_value;
```

建议新增：

```cpp
std::string material_channel{"auto"};
std::string apply_mode{"solid_volume"};
```

配置：

```json
{
  "modelMaterial": {
    "materialChannel": "V",
    "applyMode": "solid_volume",
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  }
}
```

允许值：

```text
materialChannel:
  auto
  RGB
  W
  V

applyMode:
  solid_volume
```

00C 只实现 `solid_volume`。

---

### 3.3 ReliefConfig

新增：

```cpp
struct ReliefConfig {
    std::string fill_mode{"surface_to_base"};
    double base_z_mm{0.0};
};
```

配置：

```json
{
  "relief": {
    "fillMode": "surface_to_base",
    "baseZMm": 0.0
  }
}
```

允许值：

```text
surface_to_base
intersection_range
```

---

## 4. relief_heightfield 核心算法

### 4.1 算法目标

对每个 XY 像素列，估计模型在该位置的高度覆盖范围，然后生成模型占据层。

与 scanline fill 不同，relief 模式关注：

```text
XY → z range
```

而不是：

```text
Z slice → closed contour fill
```

---

### 4.2 推荐 P0+ 简化算法：垂直列采样

第一版可以采用列采样：

```text
for each XY pixel center:
  cast vertical ray along Z
  collect intersections with mesh triangles
  if intersections not empty:
      z_min = min(intersections)
      z_max = max(intersections)
      if fillMode == surface_to_base:
          fill baseZ..z_max
      else:
          fill z_min..z_max
```

对于浮雕薄壳，推荐默认：

```text
reliefFillMode = surface_to_base
```

原因：浮雕模型通常可以视为高度场，从基准面向上形成实体高度。

---

## 5. 材料写入逻辑

建议抽出：

```cpp
void write_model_pixel(...)
void write_support_pixel(...)
```

### 5.1 materialChannel 映射

当 `materialChannel = V`：

```text
R = 255
G = 255
B = 255
W = 255
S = 255
V = 0
```

当 `materialChannel = W`：

```text
R = 255
G = 255
B = 255
W = 0
S = 255
V = 255
```

当 `materialChannel = RGB`：

```text
R/G/B = config.material.rgb
W/S/V = 255
```

当 `materialChannel = auto`：

```text
继续使用当前 rgb/whiteValue/varnishValue 配置直接写入。
```

---

## 6. 支撑逻辑

00C 中，`relief_heightfield` 默认不启用 support。

如果配置中：

```json
{
  "support": {
    "enabled": true
  }
}
```

可先允许但输出 warning：

```text
relief_heightfield_support_is_experimental
```

原因：浮雕基底不应直接混用 S 支撑通道。

---

## 7. relief_report.json

新增报告：

```json
{
  "slicingMode": "relief_heightfield",
  "fillMode": "surface_to_base",
  "baseZMm": 0.0,
  "columns": {
    "total": 0,
    "hit": 0,
    "empty": 0,
    "multiHit": 0
  },
  "zRangeMm": {
    "min": 0.0,
    "max": 0.0
  },
  "warnings": []
}
```

---

## 8. Manifest 修改

Manifest 增加：

```json
{
  "slicing": {
    "mode": "relief_heightfield",
    "reliefFillMode": "surface_to_base"
  }
}
```

Reports 增加：

```json
{
  "relief": "reports/relief_report.json"
}
```

---

## 9. 推荐实现步骤

1. `SliceConfig` 增加 `slicing_mode`。
2. `MaterialConfig` 增加 `material_channel` 和 `apply_mode`。
3. 增加 `ReliefConfig`。
4. 增加 `sample_relief_heightfield_masks()`。
5. `run_slicer()` 中根据 `slicingMode` 选择采样路径。
6. 抽出模型像素写入函数。
7. 增加 `relief_report.json`。
8. Manifest 增加 `slicing.mode`。
9. 增加样例配置 `slice_config_relief_varnish.json`。
10. 更新 `REPORT_00_P0_Demo当前实现状态.md`。
