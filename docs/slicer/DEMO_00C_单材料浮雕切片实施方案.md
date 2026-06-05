# DEMO_00C_单材料浮雕切片实施方案

> 文档版本：v0.1  
> 文档状态：Draft / 00C 实施方案  
> 建议提交目录：`docs/slicer/`

---

## 1. 实施目标

让 `0.3.obj` 这类单材料浮雕模型可以稳定输出光油 V 通道切片数据。

保持：

```text
uint8
0 = 打印
255 = 不打印
R G B W S V
```

---

## 2. 目标配置

新增样例配置：

```text
samples/configs/slice_config_relief_varnish.json
```

建议内容：

```json
{
  "slicingMode": "relief_heightfield",
  "input": {
    "modelPath": "samples/models/relief/0.3.obj",
    "format": "auto"
  },
  "output": {
    "packageDir": "output/SlicePackage_relief_varnish",
    "dpiX": 600,
    "dpiY": 600,
    "layerThicknessMm": 0.01,
    "channelOrder": ["R", "G", "B", "W", "S", "V"],
    "bitDepth": 8,
    "planarConfig": "contiguous",
    "tiled": true,
    "tileSize": [256, 256]
  },
  "background": {
    "value": 255
  },
  "modelMaterial": {
    "materialChannel": "V",
    "applyMode": "solid_volume",
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  },
  "support": {
    "enabled": false,
    "mode": "bottom_projection",
    "value": 0
  },
  "relief": {
    "fillMode": "surface_to_base",
    "baseZMm": 0.0
  },
  "preview": {
    "enabled": true,
    "format": "png",
    "interval": 10,
    "channels": ["varnish"],
    "onlyNonEmptyLayers": true
  }
}
```

---

## 3. 实施步骤

### Step 1：配置解析

新增字段：

```text
slicingMode
modelMaterial.materialChannel
modelMaterial.applyMode
relief.fillMode
relief.baseZMm
```

验证规则：

```text
slicingMode in [closed_mesh_scanline, relief_heightfield]
materialChannel in [auto, RGB, W, V]
applyMode == solid_volume
relief.fillMode in [surface_to_base, intersection_range]
```

### Step 2：新增 relief sampler

新增函数：

```cpp
std::vector<std::vector<std::uint8_t>> sample_relief_heightfield_masks(...)
```

第一版可以实现：

```text
for each XY:
  对 mesh triangles 做垂直射线求交
  取最高交点 z_top
  baseZ = relief.baseZMm
  将 baseZ..z_top 对应层标记为 model
```

### Step 3：切换采样路径

在 `run_slicer()` 中：

```cpp
if (config.slicing_mode == "relief_heightfield") {
    model_masks = sample_relief_heightfield_masks(...);
} else {
    model_masks = sample_model_masks(...);
}
```

### Step 4：材料通道写入

新增：

```cpp
write_model_pixel_by_material_channel(...)
```

规则：

```text
V: V=0，其余未使用通道=255
W: W=0，其余未使用通道=255
RGB: RGB=配置值，W/S/V=255
auto: 使用当前 rgb/whiteValue/varnishValue 直接写入
```

### Step 5：浮雕模式默认不生成支撑

当：

```text
slicingMode = relief_heightfield
support.enabled = false
```

不写 S 通道。

如果用户强行开启 support，可以先保留现有逻辑，但输出 warning。

### Step 6：输出 relief_report.json

输出：

```text
reports/relief_report.json
```

### Step 7：更新 Manifest

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

## 4. 验收 Checklist

- [ ] `slicingMode = relief_heightfield` 可配置。
- [ ] `materialChannel = V` 可配置。
- [ ] 光油浮雕模型区域 V=0。
- [ ] 空白区域所有通道为 255。
- [ ] 默认不生成 S 支撑。
- [ ] 输出 `relief_report.json`。
- [ ] Manifest 记录 `slicing.mode`。
- [ ] `rip_reader_test` 仍通过。
- [ ] 原 `closed_mesh_scanline` 样例仍可运行。
- [ ] Preview 可以显示 V 通道打印区域。
