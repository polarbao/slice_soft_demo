# DOC_DECISION_00B_8bit输出与黑色打印极性修正

> 文档版本：v0.1  
> 文档状态：Draft / 协议修正决策  
> 适用阶段：P0 Demo 修正  
> 修正主题：输出位深从 uint16 调整为 uint8；生产数据极性调整为黑色表示打印、白色表示不打印

---

## 1. 问题背景

当前 P0 Demo 测试发现两个输出协议问题：

1. 当前 TIFF 数据通道为 16-bit，数值范围为 `0-65535`，但真实使用环境要求为 8-bit，数值范围为 `0-255`。
2. 当前 Demo 输出中，黑色表示空白，白色表示打印；而打印行业中通常采用“黑色表示打印，白色表示不打印”的图像语义。

因此需要对 PRD、DEV、DEMO 和代码实现进行协议级修正。

---

## 2. 修正结论

### 2.1 位深修正

P0 输出 TIFF 从：

```text
uint16
0 - 65535
```

调整为：

```text
uint8
0 - 255
```

### 2.2 打印极性修正

生产数据采用打印行业常用极性：

```text
0   = 打印 / 满材料 / 满输出
255 = 不打印 / 空白 / 无输出
```

即：

```text
黑色 = 打印
白色 = 不打印
```

### 2.3 通道顺序保持不变

通道顺序继续保持：

```text
R G B W S V
```

本次不调整通道顺序，只调整：

```text
bitDepth
数值范围
打印极性
配置字段语义
preview 显示逻辑
RIP reader 校验逻辑
```

---

## 3. 新旧协议对比

| 项 | 旧版本 | 新版本 |
|---|---|---|
| 位深 | uint16 | uint8 |
| 数值范围 | 0 - 65535 | 0 - 255 |
| 打印极性 | 65535 表示打印 | 0 表示打印 |
| 空白极性 | 0 表示空白 | 255 表示空白 |
| 通道顺序 | R G B W S V | R G B W S V |
| 支撑打印值 | 65535 | 0 |
| 支撑空白值 | 0 | 255 |
| 模型光油打印值 | V = 65535 | V = 0 |
| 模型白墨打印值 | W = 65535 | W = 0 |

---

## 4. 配置语义建议

旧字段名中 `whiteStrength`、`varnishStrength`、`support.strength` 容易让人理解为“越大越打印”。

但新极性下，真实输出值是：

```text
越小越打印
越大越不打印
```

因此建议后续将配置字段从 `Strength` 改为 `Value`，避免语义反转。

推荐新配置：

```json
{
  "modelMaterial": {
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  },
  "support": {
    "enabled": true,
    "mode": "bottom_projection",
    "value": 0
  },
  "background": {
    "value": 255
  }
}
```

如果为了兼容旧代码，可以短期保留旧字段，但文档应明确：

```text
旧字段 strength 在 00B 之后不再表示“强度”，而是实际输出灰度值。
```

更推荐直接升级字段名。

---

## 5. 常用材料配置示例

### 5.1 整模型使用光油填充

```json
{
  "modelMaterial": {
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  }
}
```

含义：

```text
RGB 不打印
W 不打印
V 打印
```

### 5.2 整模型使用白墨填充

```json
{
  "modelMaterial": {
    "rgb": [255, 255, 255],
    "whiteValue": 0,
    "varnishValue": 255
  }
}
```

含义：

```text
RGB 不打印
W 打印
V 不打印
```

### 5.3 整模型使用 RGB 黑色打印

```json
{
  "modelMaterial": {
    "rgb": [0, 0, 0],
    "whiteValue": 255,
    "varnishValue": 255
  }
}
```

含义：

```text
R 打印
G 打印
B 打印
W 不打印
V 不打印
```

### 5.4 空白区域

所有通道均为：

```text
255
```

---

## 6. Preview 与生产数据的区别

生产 TIFF 数据采用：

```text
0 = 打印
255 = 不打印
```

但是 UI/Preview 为了方便人眼查看，可以使用伪彩色显示。

例如：

```text
S 通道打印区域，生产值为 0，但 preview 可以显示为绿色或蓝色。
V 通道打印区域，生产值为 0，但 preview 可以显示为紫色。
W 通道打印区域，生产值为 0，但 preview 可以显示为白色或灰色。
```

因此必须区分：

```text
Production Channel Value
Display Preview Color
```

---

## 7. 对代码实现的影响

需要修改：

```text
SliceConfig
Config parser
LayerChannelComposer
TIFF Writer
Manifest Writer
RIP Reader
Preview Exporter
samples/configs/slice_config.json
docs/slicer/*
```

核心变更：

```text
std::uint16_t → std::uint8_t
BitsPerSample 16 → 8
bitDepth 16 → 8
empty/background 默认 255
model/support 打印区域默认 0
RIP reader 校验 bitDepth == 8
preview 显示时不要直接把 0 当黑色空白，而要按通道语义做伪彩色
```

---

## 8. 结论

00B 协议修正后，P0 Demo 的输出协议应冻结为：

```text
RGBWSV 六通道
uint8
0 = 打印
255 = 不打印
R G B W S V 通道顺序不变
```

这比当前 uint16 / 白色打印逻辑更符合真实打印行业环境。
