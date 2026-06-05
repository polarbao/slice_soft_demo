# DOC_REVIEW_00C_基于当前代码的浮雕专项判断

> 文档版本：v0.1  
> 文档状态：Draft / 代码现状评审  
> 适用阶段：00C 前置评审  
> 建议提交目录：`docs/slicer/`

---

## 1. 当前代码确认结论

根据当前 GitHub 主线代码，00B 已经基本进入主线：

```text
bitDepth = 8
background.value = 255
modelMaterial.rgb / whiteValue / varnishValue 已支持 uint8 配置
support.value 已支持 uint8 配置
manifest 已写入 black_is_print
rip_reader_test 已校验 bitDepth=8、polarity=black_is_print
```

当前单材料输出确实可以通过配置文件控制，但仍然是“隐式材料配置”：通过 `rgb / whiteValue / varnishValue` 的数值决定写入哪个通道，而不是通过显式 `materialChannel` 和 `applyMode` 描述材料模式。

---

## 2. 当前单材料输出模式的本质

当前光油单材料配置为：

```json
{
  "modelMaterial": {
    "rgb": [255, 255, 255],
    "whiteValue": 255,
    "varnishValue": 0
  }
}
```

在 00B 协议下：

```text
0   = 打印
255 = 不打印
```

所以该配置含义为：

```text
RGB 不打印
W 不打印
V 打印
```

这已经能够表达“单材料模型整体使用光油通道打印”。

---

## 3. 当前浮雕问题不是材料配置无法表达

当前异常不是因为不能配置光油输出，而是因为模型区域 `modelMask` 的生成方式不适合浮雕/薄壳/复杂开口模型。

当前主路径是：

```text
Z 平面切三角面
→ 得到 segment
→ scanline fill
→ 生成每层 model mask
```

这条路线适合普通闭合实体模型，不适合高频浮雕模型。

---

## 4. 当前支撑算法边界

当前支撑逻辑基于每个 XY 位置的第一层模型：

```text
compute_first_model_layers
compose_layer:
  if model mask:
      写模型材料
  else if support.enabled && first_model_layer > current_layer:
      写 S 支撑
```

该策略适合普通 bottom projection 支撑，但不能自然表达浮雕模型的高度场、基底、局部凸起和中高层断续结构。

---

## 5. 00C 的必要性

既然浮雕模型是后续高频模型类型，不能继续当作普通 closed mesh 的边界样例处理。

需要新增：

```text
00C：单材料浮雕模型切片修正
```

00C 的目标不是完整光油覆盖策略，也不是彩色纹理，而是：

```text
在 00B 协议基础上，为单材料浮雕模型提供稳定的 relief_heightfield 切片路径。
```

---

## 6. 00C 与当前代码的关系

00C 不推翻当前代码，而是新增模式：

```text
closed_mesh_scanline     当前默认路径
relief_heightfield       新增浮雕路径
```

短期保持默认：

```text
slicingMode = closed_mesh_scanline
```

新增浮雕样例配置：

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

## 7. 优先级结论

推荐后续顺序调整为：

```text
00B：8-bit / 黑色打印极性修正
00C：单材料浮雕模型切片修正
00A：P0 Demo 稳定化增强
PRD_01：2.5D / 浮雕正式路线
PRD_02：彩色纹理模型切片
PRD_03：RGBWSV TIFF / RIP 协议固化
```
