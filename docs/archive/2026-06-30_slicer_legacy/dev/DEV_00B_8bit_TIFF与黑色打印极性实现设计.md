# DEV_00B_8bit_TIFF与黑色打印极性实现设计

> 文档版本：v0.1  
> 文档状态：Draft / P0 输出协议修正设计  
> 文档类型：DEV 增量设计  
> 所属模块：切片软件 / Slicer

---

## 1. 技术目标

将当前 P0 Demo 输出从：

```text
uint16
0 = 空白
65535 = 打印
```

修正为：

```text
uint8
0 = 打印
255 = 空白
```

通道顺序保持：

```text
R G B W S V
```

---

## 2. 代码影响范围

需要修改：

```text
src/slicer_core/config.*
src/slicer_core/slicer.*
src/slicer_core/tiff_io.*
src/slicer_core/rip_reader.*
samples/configs/slice_config.json
docs/slicer/*
```

---

## 3. 数据结构修改

### 3.1 Layer buffer

旧：

```cpp
std::vector<std::uint16_t> layer;
```

新：

```cpp
std::vector<std::uint8_t> layer;
```

### 3.2 配置字段

推荐新结构：

```cpp
struct MaterialConfig {
    std::array<std::uint8_t, 3> rgb{255, 255, 255};
    std::uint8_t white_value{255};
    std::uint8_t varnish_value{255};
};

struct SupportConfig {
    bool enabled{true};
    std::string mode{"bottom_projection"};
    std::uint8_t value{0};
    double offset_mm{0.0};
    int min_area_px{0};
};

struct BackgroundConfig {
    std::uint8_t value{255};
};
```

如果需要兼容旧字段，可以短期支持：

```text
whiteStrength → whiteValue
varnishStrength → varnishValue
support.strength → support.value
```

但文档中应标记旧字段 deprecated。

---

## 4. 通道合成逻辑

### 4.1 新默认值

所有像素初始值应为：

```text
255
```

而不是 0。

即：

```cpp
std::vector<std::uint8_t> pixels(width * height * 6, background_value);
```

### 4.2 模型像素

```cpp
if (model_mask[pixel] != 0) {
    R = config.material.rgb[0];
    G = config.material.rgb[1];
    B = config.material.rgb[2];
    W = config.material.white_value;
    S = 255;
    V = config.material.varnish_value;
}
```

### 4.3 支撑像素

```cpp
else if (support_enabled && first_model_layer[pixel] > layer_index) {
    R = 255;
    G = 255;
    B = 255;
    W = 255;
    S = config.support.value;  // 默认 0
    V = 255;
}
```

### 4.4 优先级

保持：

```text
Model > Support
```

---

## 5. TIFF Writer 修改

旧：

```text
BitsPerSample = 16
SampleFormat = uint
uint16 buffer
```

新：

```text
BitsPerSample = 8
SampleFormat = uint
uint8 buffer
```

保持：

```text
SamplesPerPixel = 6
PlanarConfig = contiguous
Tiled = true
ChannelOrder = R G B W S V
```

---

## 6. Manifest 修改

Manifest 中应增加或修改：

```json
{
  "tiff": {
    "channelOrder": ["R", "G", "B", "W", "S", "V"],
    "channelCount": 6,
    "bitDepth": 8,
    "sampleFormat": "uint",
    "planarConfig": "contiguous",
    "storage": "tiled",
    "polarity": "black_is_print",
    "printValue": 0,
    "emptyValue": 255
  }
}
```

---

## 7. RIP Reader 修改

RIP reader 校验：

```text
bitDepth == 8
polarity == black_is_print
printValue == 0
emptyValue == 255
channelOrder == R G B W S V
```

RIP reader checksum 建议统计：

```text
printPixelsPerChannel = count(value < 255)
fullPrintPixelsPerChannel = count(value == 0)
emptyPixelsPerChannel = count(value == 255)
```

不要再假设 “值越大打印越强”。

---

## 8. Preview 修改

Preview 不能直接把生产值映射为灰度，因为生产值 0 表示打印。

建议：

### 8.1 单通道 preview

```cpp
preview_intensity = 255 - production_value;
```

这样：

```text
production 0   → preview 255，可见
production 255 → preview 0，不可见
```

### 8.2 伪彩色 preview

支撑：

```cpp
support_preview = {0, 255 - S, 0};
```

光油：

```cpp
varnish_preview = {255 - V, 0, 255 - V};
```

白墨：

```cpp
white_preview = {255 - W, 255 - W, 255 - W};
```

---

## 9. 样例配置

### 9.1 整模型光油

```json
{
  "output": {
    "bitDepth": 8
  },
  "background": {
    "value": 255
  },
  "modelMaterial": {
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  },
  "support": {
    "enabled": true,
    "mode": "bottom_projection",
    "value": 0
  }
}
```

### 9.2 整模型白墨

```json
{
  "modelMaterial": {
    "rgb": [255, 255, 255],
    "whiteValue": 0,
    "varnishValue": 255
  }
}
```

---

## 10. 推荐迁移步骤

1. 增加 uint8 读取函数 `read_u8`。
2. 修改 MaterialConfig / SupportConfig。
3. 修改配置文件字段。
4. 修改 layer buffer 类型。
5. 修改 compose_layer 默认背景为 255。
6. 修改 TIFF writer BitsPerSample 为 8。
7. 修改 manifest bitDepth 和 polarity。
8. 修改 rip_reader_test 校验。
9. 修改 preview 反相和伪彩色显示。
10. 更新 REPORT_00。

---

## 11. 测试用例

### Test 1：全空层

所有通道均应为：

```text
255
```

### Test 2：光油模型

模型区域：

```text
V = 0
其他未使用通道 = 255
```

### Test 3：白墨模型

模型区域：

```text
W = 0
其他未使用通道 = 255
```

### Test 4：支撑区域

支撑区域：

```text
S = 0
其他通道 = 255
```

### Test 5：RIP Reader

应通过：

```text
bitDepth == 8
polarity == black_is_print
```

---

## 12. 结论

00B 实现完成后，P0 Demo 才与真实打印行业图像语义一致：

```text
uint8
0 = 打印
255 = 不打印
黑色打印
白色空白
```
