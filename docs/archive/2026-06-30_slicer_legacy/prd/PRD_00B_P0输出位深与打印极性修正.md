# PRD_00B_P0输出位深与打印极性修正

> 文档版本：v0.1  
> 文档状态：Draft / P0 输出协议修正  
> 文档类型：PRD 增量文档  
> 所属模块：切片软件 / Slicer  
> 适用阶段：P0 Demo 修正

---

## 1. 修正目标

针对 P0 Demo 测试中发现的输出问题，对切片输出协议进行修正：

1. 输出 TIFF 位深由 16-bit 调整为 8-bit。
2. 输出数值范围由 `0-65535` 调整为 `0-255`。
3. 生产数据极性调整为：

```text
0   = 打印
255 = 不打印
```

4. 保持通道顺序不变：

```text
R G B W S V
```

---

## 2. 产品语义

### 2.1 打印行业图像语义

P0 之后，切片输出必须遵循：

```text
黑色表示打印
白色表示不打印
```

即：

```text
0   = 满材料输出
255 = 空白无输出
```

### 2.2 空白区域

空白区域所有通道均为：

```text
255
```

### 2.3 模型区域

模型区域根据配置决定写入哪些通道。

例如整模型使用光油：

```text
R = 255
G = 255
B = 255
W = 255
S = 255
V = 0
```

例如整模型使用白墨：

```text
R = 255
G = 255
B = 255
W = 0
S = 255
V = 255
```

例如整模型使用 RGB 黑色打印：

```text
R = 0
G = 0
B = 0
W = 255
S = 255
V = 255
```

### 2.4 支撑区域

支撑区域写入 S 通道：

```text
S = 0
```

其他通道默认：

```text
255
```

---

## 3. 配置需求

### 3.1 推荐新配置字段

旧配置中的 `whiteStrength`、`varnishStrength`、`support.strength` 应逐步替换为：

```text
whiteValue
varnishValue
support.value
background.value
```

原因：

```text
新协议下数值越小越表示打印，继续使用 Strength 会造成歧义。
```

推荐配置：

```json
{
  "output": {
    "bitDepth": 8,
    "channelOrder": ["R", "G", "B", "W", "S", "V"]
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

### 3.2 光油填充配置

整模型使用光油填充：

```json
{
  "modelMaterial": {
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  }
}
```

### 3.3 白墨填充配置

整模型使用白墨填充：

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

## 4. Preview 需求

Preview 不能再简单按生产值直接显示。

原因：

```text
生产值 0 表示打印，但直接显示会是黑色。
如果所有打印区域都是黑色，难以区分支撑、光油、白墨、RGB。
```

因此 Preview 应使用伪彩色：

| 通道 | 生产打印值 | Preview 建议 |
|---|---:|---|
| S | 0 | 绿色或蓝色 |
| V | 0 | 紫色 |
| W | 0 | 白色/灰色 |
| RGB | 0-255 | 按 RGB 或反相显示 |

Preview 仅用于调试，不影响 TIFF 生产数据。

---

## 5. 验收标准更新

P0 00B 验收标准：

1. TIFF bitDepth 为 8。
2. TIFF 每通道值范围为 `0-255`。
3. 空白区域所有通道为 255。
4. 模型光油填充时，模型区域 V 通道为 0。
5. 模型白墨填充时，模型区域 W 通道为 0。
6. 支撑区域 S 通道为 0。
7. RIP reader 校验 bitDepth == 8。
8. Preview 能以伪彩色显示支撑和光油区域。
9. Manifest 中记录 `polarity = black_is_print` 或等价字段。
10. 旧的 uint16 输出不再作为 P0 默认输出。

---

## 6. 非目标

本次修正不处理：

```text
全彩纹理
多材料
3MF
复杂支撑
RIP 半色调
真实喷头 bitstream
```

---

## 7. 结论

P0 Demo 输出协议应修正为：

```text
RGBWSV
uint8
0 = 打印
255 = 不打印
black_is_print
```
