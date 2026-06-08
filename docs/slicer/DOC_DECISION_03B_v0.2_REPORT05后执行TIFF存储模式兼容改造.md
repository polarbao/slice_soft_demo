# DOC_DECISION_03B_v0.2_REPORT05后执行TIFF存储模式兼容改造

> 文档版本：v0.2  
> 文档状态：Decision / 基于 REPORT_05 修订  
> 适用阶段：05 材料策略完成后 / 03B  
> 建议提交目录：`docs/slicer/`  
> 主题：05 完成后执行 TIFF StorageMode 兼容改造，默认输出 Stripped，保留 Tiled 兼容

---

## 1. 修订背景

根据 `REPORT_05_材料策略当前实现状态.md`，05 阶段已经完成 MaterialPolicy 基础实现，并通过完整回归。

05 已实现：

```text
RGB texture only
RGB texture + W white underbase
RGB texture + V varnish top_n_layers
RGB texture + W underbase + V top_n_layers
V varnish only
W white only
```

05 仍保持当前协议：

```text
schema = p0.rgbwsv.1
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF 通道
```

05 已新增并验证 6 个 MaterialPolicy 样例：

```text
MaterialPolicyRgbOnly
MaterialPolicyRgbWhiteUnderbase
MaterialPolicyRgbVarnishTop2
MaterialPolicyRgbWhiteVarnish
MaterialPolicyVarnishOnly
MaterialPolicyWhiteOnly
```

因此，之前规划的 03B 阶段现在可以正式执行。

---

## 2. 是否需要修改上一版 03B 文档

需要做小幅修订，但不需要推翻上一版 03B。

上一版 03B 的核心决策仍然有效：

```text
Writer 支持 stripped / tiled
Reader 支持 stripped / tiled
默认输出 stripped
保留 tiled 兼容
Manifest 显式记录 storageMode
Regression 覆盖双模式
```

本次 v0.2 主要补充：

```text
1. 明确 05 已完成，03B 现在可执行；
2. 将 MaterialPolicy 六个样例纳入 03B 回归基线；
3. 明确 p0.rgbwsv.1 → p0.rgbwsv.2 的 schema 迁移边界；
4. 明确 03B 不能破坏 MaterialPolicy 的 RGB/W/V 语义校验；
5. 明确 03B 完成后再决定是否进入 05A 或 06。
```

---

## 3. 03B 阶段定位

03B 是：

```text
TIFF 物理存储模式兼容改造
```

不是：

```text
材料策略修改
纹理采样修改
RIP 半色调
ICC / CMYK
3MF
OpenVDB
Qt UI
支撑形态修复
```

03B 只处理：

```text
TIFF Writer storage mode
RIP Reader storage mode
Manifest storageMode
Schema 兼容
Bad package
Regression
```

---

## 4. 默认输出策略

03B 后默认输出：

```text
schema = p0.rgbwsv.2
storageMode = stripped
rowsPerStrip = 64
```

保留可配置：

```text
storageMode = tiled
tileSize = [256, 256]
```

原因：

```text
stripped 更适合传统 RIP scanline 读取；
tiled 更适合大图块处理、局部读取和后续工业扩展；
项目应同时支持两者，但默认先偏向 RIP 兼容。
```

---

## 5. Schema 迁移策略

### 5.1 当前基线

05 完成时仍是：

```text
schema = p0.rgbwsv.1
tiled = true
```

### 5.2 03B 后 Writer 默认

03B 后 Writer 默认输出：

```text
schema = p0.rgbwsv.2
tiff.storageMode = stripped
tiff.tiled = false
tiff.rowsPerStrip = 64
```

### 5.3 Reader 兼容

Reader 必须兼容：

```text
p0.rgbwsv.1 legacy tiled package
p0.rgbwsv.2 stripped package
p0.rgbwsv.2 tiled package
```

不能只读取 p0.rgbwsv.2。

---

## 6. 05 MaterialPolicy 回归基线

03B 不能破坏以下语义：

| Package | RGB | W | V | Support | 关键要求 |
|---|---:|---:|---:|---:|---|
| MaterialPolicyRgbOnly | >0 | 0 | 0 | >0 | RGB only |
| MaterialPolicyRgbWhiteUnderbase | >0 | >0 | 0 | >0 | W underbase |
| MaterialPolicyRgbVarnishTop2 | >0 | 0 | >0 | >0 | V 只在 top 2 layers |
| MaterialPolicyRgbWhiteVarnish | >0 | >0 | >0 | >0 | RGB/W/V 组合 |
| MaterialPolicyVarnishOnly | 0 | 0 | >0 | >0 | V only |
| MaterialPolicyWhiteOnly | 0 | >0 | 0 | >0 | W only |

其中 `MaterialPolicyRgbVarnishTop2` 和 `MaterialPolicyRgbWhiteVarnish` 必须继续验证：

```text
V 只出现在模型顶部 topLayers 范围
```

---

## 7. 执行顺序

推荐执行顺序：

```text
1. 以 REPORT_05 为冻结基线；
2. 执行 03B storageMode 改造；
3. 默认输出切换为 stripped；
4. Reader 支持 stripped/tiled 双路径；
5. run_regression.ps1 同时验证 05 MaterialPolicy 与 storage mode；
6. 生成 REPORT_03B；
7. 再决定进入 05A 真实模型材料参数验证或 06 3MF/多材料输入。
```

---

## 8. 结论

03B 文档需要基于 REPORT_05 做 v0.2 修订。

修订重点不是改变方向，而是把 05 的 MaterialPolicy 成果纳入 03B 的强制回归基线。
