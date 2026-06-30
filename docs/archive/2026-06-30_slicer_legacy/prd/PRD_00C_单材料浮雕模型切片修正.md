# PRD_00C_单材料浮雕模型切片修正

> 文档版本：v0.1  
> 文档状态：Draft / P0+ 浮雕专项需求  
> 文档类型：PRD 增量文档  
> 所属模块：切片软件 / Slicer  
> 建议提交目录：`docs/slicer/`

---

## 1. 产品目标

00C 阶段目标是让单材料浮雕模型能够稳定完成切片输出。

当前目标模型类型：

```text
浮雕模型
2.5D 模型
薄壳/浅浮雕 OBJ/STL
高度起伏明显但材料单一的模型
```

00C 输出仍然遵守 00B 协议：

```text
RGBWSV
uint8
0 = 打印
255 = 不打印
R G B W S V
```

---

## 2. 用户价值

浮雕模型是 UV 打印常见业务场景。

典型使用方式：

```text
单材料光油浮雕
单材料白墨浮雕
单材料 RGB 灰度/黑色浮雕
```

00C 需要解决：

```text
普通 closed mesh scanline 对浮雕模型不稳定的问题
```

并为后续正式 2.5D / Relief 路线打基础。

---

## 3. 需求范围

### 3.1 必须支持

```text
1. 新增 slicingMode = relief_heightfield
2. 支持单材料浮雕模型
3. 支持模型主材料通道选择：V / W / RGB / auto
4. 默认支持光油 V 通道输出
5. 生成 relief_report.json
6. 不破坏原 closed_mesh_scanline 模式
```

### 3.2 00C 默认配置

```json
{
  "slicingMode": "relief_heightfield",
  "modelMaterial": {
    "materialChannel": "V",
    "applyMode": "solid_volume",
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  },
  "support": {
    "enabled": false
  }
}
```

---

## 4. 单材料输出语义

### 4.1 光油材料输出

当：

```json
{
  "modelMaterial": {
    "materialChannel": "V",
    "varnishValue": 0
  }
}
```

模型占据区域输出：

```text
R = 255
G = 255
B = 255
W = 255
S = 255
V = 0
```

空白区域输出：

```text
R = 255
G = 255
B = 255
W = 255
S = 255
V = 255
```

### 4.2 白墨材料输出

```json
{
  "modelMaterial": {
    "materialChannel": "W",
    "whiteValue": 0,
    "varnishValue": 255
  }
}
```

模型区域：

```text
W = 0
其他未使用通道 = 255
```

### 4.3 RGB 材料输出

```json
{
  "modelMaterial": {
    "materialChannel": "RGB",
    "rgb": [0, 0, 0],
    "whiteValue": 255,
    "varnishValue": 255
  }
}
```

模型区域：

```text
R/G/B = 配置值
W/S/V = 255
```

---

## 5. 支撑与浮雕基底边界

00C 默认不启用 S 支撑通道。

原因：

```text
浮雕模型中的“基底/铺底/实体底层”不等同于 3D 打印支撑。
```

因此：

```json
{
  "support": {
    "enabled": false
  }
}
```

后续如需要浮雕基底，应新增专门字段，例如：

```text
reliefBase
baseLayer
foundationMaterial
```

而不是直接复用 S 通道。

---

## 6. 验收标准

00C 验收标准：

1. 支持 `slicingMode = relief_heightfield`。
2. 支持 `modelMaterial.materialChannel = V`。
3. 使用光油材料时，模型区域 V 通道为 0。
4. 空白区域所有通道为 255。
5. 浮雕模型中高层不应因为 scanline open segment 直接丢失主要材料区域。
6. 输出 `relief_report.json`。
7. `closed_mesh_scanline` 原行为不被破坏。
8. `rip_reader_test` 仍通过。
9. Manifest 中记录当前 `slicingMode`。
10. 00C 不实现完整光油覆盖策略。

---

## 7. 非目标

00C 不处理：

```text
彩色纹理
OBJ UV 贴图采样
MTL 材质映射
局部光油
上表面光油策略
完整高度图输入
复杂支撑
OpenVDB 正式内核
```
