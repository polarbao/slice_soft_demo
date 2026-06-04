# DEMO_00B_8bit输出与黑色打印极性实施方案

> 文档版本：v0.1  
> 文档状态：Draft / P0 输出修正版实施方案  
> 适用阶段：P0 Demo 00B

---

## 1. 实施目标

将当前 Demo 从：

```text
uint16
白色表示打印
黑色表示空白
```

修正为：

```text
uint8
黑色表示打印
白色表示空白
```

---

## 2. 实施顺序

### Step 1：更新配置结构

修改 `samples/configs/slice_config.json`：

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
    "value": 0
  }
}
```

### Step 2：更新配置解析

实现：

```text
read_u8
whiteValue
varnishValue
support.value
background.value
```

短期可兼容旧字段：

```text
whiteStrength
varnishStrength
support.strength
```

但新文档和样例配置统一使用 `Value`。

### Step 3：更新通道合成

默认背景从：

```text
0
```

改为：

```text
255
```

模型光油填充：

```text
V = 0
```

支撑填充：

```text
S = 0
```

### Step 4：更新 TIFF Writer

```text
BitsPerSample = 8
buffer type = uint8
```

### Step 5：更新 Manifest

增加：

```json
{
  "polarity": "black_is_print",
  "printValue": 0,
  "emptyValue": 255
}
```

### Step 6：更新 RIP Reader

校验：

```text
bitDepth == 8
polarity == black_is_print
```

### Step 7：更新 Preview

Preview 显示时做反相或伪彩色：

```text
visible = 255 - productionValue
```

支撑显示绿色，光油显示紫色，白墨显示灰白色。

---

## 3. 验收 Checklist

- [ ] `output.bitDepth` 为 8。
- [ ] TIFF BitsPerSample 为 8。
- [ ] 空白区域所有通道为 255。
- [ ] 光油模型区域 V = 0。
- [ ] 白墨模型区域 W = 0。
- [ ] 支撑区域 S = 0。
- [ ] Manifest 写入 `polarity = black_is_print`。
- [ ] RIP Reader 校验 bitDepth 和 polarity。
- [ ] Preview 可以正确显示打印区域。
- [ ] 旧的 16-bit 默认路径被移除或明确标记 deprecated。

---

## 4. Codex 推荐指令

```text
请阅读 docs/slicer/DOC_DECISION_00B_8bit输出与黑色打印极性修正.md、
PRD_00B_P0输出位深与打印极性修正.md、
DEV_00B_8bit_TIFF与黑色打印极性实现设计.md、
DEMO_00B_8bit输出与黑色打印极性实施方案.md。

根据 00B 文档修改代码：
1. 将 TIFF 输出从 uint16 改为 uint8；
2. 将打印极性改为 0=打印，255=不打印；
3. 更新配置字段 whiteValue、varnishValue、support.value；
4. 更新 manifest；
5. 更新 rip_reader_test；
6. 更新 preview 显示逻辑；
7. 保持通道顺序 R G B W S V 不变。
完成后更新 REPORT_00_P0_Demo当前实现状态.md。
```

---

## 5. 结论

00B 是 P0 Demo 必须尽快完成的协议修正。

不建议继续在 16-bit / 白色表示打印 的旧逻辑上扩展 PRD_01 或彩色纹理功能。
