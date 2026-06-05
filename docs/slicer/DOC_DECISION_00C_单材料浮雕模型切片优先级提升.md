# DOC_DECISION_00C_单材料浮雕模型切片优先级提升

> 文档版本：v0.1  
> 文档状态：Draft / 阶段决策  
> 适用阶段：P0+ / 00C  
> 主题：将单材料浮雕模型切片修正提升为 00C 阶段，优先级高于彩色纹理阶段

---

## 1. 决策背景

当前 P0 Demo 已完成基础切片闭环，并已经进入 00B 输出协议修正阶段：

```text
uint8
0 = 打印
255 = 不打印
black_is_print
R G B W S V
```

在使用浮雕模型进行测试时，发现现有 `closed_mesh_scanline + bottom_projection` 路线对浮雕类模型存在明显局限：

```text
1. 中高层出现局部材料缺失或区域异常
2. bottom_projection 只基于 first_model_layer，无法表达浮雕高度场结构
3. 当前光油材料只是单材料 V 通道输出，不是完整光油覆盖策略
4. 浮雕模型后续是高频业务模型，不能长期作为边界测试处理
```

因此需要新增 00C 阶段：

```text
00C：单材料浮雕模型切片修正
```

---

## 2. 与光油策略的边界

当前需求不是完整“光油覆盖策略”。

当前需求是：

```text
模型材料 = 光油材料
模型占据区域写入 V 通道
```

在 00B 协议下：

```text
V = 0   表示光油打印
V = 255 表示光油不打印
```

完整光油覆盖策略，例如：

```text
top_surface_only
top_n_layers
local_varnish_mask
texture_driven_varnish
protective_varnish_layer
```

不进入 00C 实现范围。

00C 只解决：

```text
单材料浮雕模型如何稳定生成模型占据层，并将模型材料写入指定材料通道。
```

---

## 3. 00C 与彩色纹理阶段的优先级

00C 优先级高于彩色纹理。

原因：

```text
1. 浮雕模型是后续高频输入类型
2. 00C 仍是单材料，不涉及 UV/MTL/Texture/ColorShell
3. 00C 只需新增 relief_heightfield 模式，复杂度低于彩色纹理切片
4. 当前浮雕问题会影响真实业务 Demo 验证
```

阶段顺序调整为：

```text
00B：8-bit / 黑色打印极性修正
00C：单材料浮雕模型切片修正
00A：P0 Demo 稳定化增强
PRD_01：2.5D / 浮雕正式路线
PRD_02：彩色纹理模型切片
PRD_03：RGBWSV TIFF / RIP 协议固化
```

---

## 4. 00C 核心决策

### 4.1 新增 slicingMode

```json
{
  "slicingMode": "relief_heightfield"
}
```

已有普通实体模型继续使用：

```json
{
  "slicingMode": "closed_mesh_scanline"
}
```

若未设置，短期默认仍为 `closed_mesh_scanline`，避免破坏现有 Demo。

---

### 4.2 新增 materialChannel / applyMode

建议新增：

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

含义：

```text
materialChannel = V      模型主材料通道为光油
applyMode = solid_volume 模型占据体积全部写入该材料通道
```

注意：

```text
这不是光油覆盖策略，只是单材料模型的材料通道归属。
```

---

### 4.3 浮雕模式下支撑默认关闭

浮雕打印中的“基底/铺底”和 3D 打印“支撑”不是同一概念。

因此 00C 推荐默认：

```json
{
  "support": {
    "enabled": false
  }
}
```

如确实需要后续支持，可单独引入：

```text
reliefBase
baseLayer
foundationMaterial
```

不要直接把浮雕基底写入 S 通道。

---

## 5. 00C 非目标

00C 不做：

```text
彩色纹理
UV 采样
MTL 材质解析
局部光油
上表面光油覆盖策略
完整高度图产品功能
复杂支撑树
OpenVDB 正式体素内核
```

---

## 6. 结论

00C 是必要阶段。

它的目标是：

```text
在 00B 协议基础上，让单材料浮雕模型能够稳定生成 RGBWSV 层数据，尤其是 V 光油通道输出。
```

Codex 后续实现时必须遵守：

```text
1. 保持 00B：uint8，0=打印，255=不打印
2. 保持通道顺序：R G B W S V
3. 不引入完整光油覆盖策略
4. 不引入彩色纹理
5. 不把浮雕基底误写为 S 支撑通道
```
