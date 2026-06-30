# CODEX_PROMPT_03B_v0.2_TIFF存储模式兼容改造指令

> 文档版本：v0.2  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

请在 05 材料策略阶段完成后执行本阶段。

请先阅读：

```text
docs/slicer/REPORT_05_材料策略当前实现状态.md
docs/slicer/DOC_DECISION_03B_v0.2_REPORT05后执行TIFF存储模式兼容改造.md
docs/slicer/PRD_03B_v0.2_RGBWSV_TIFF存储模式兼容改造.md
docs/slicer/DEV_03B_v0.2_TIFFStorageModeWriterReader设计.md
docs/slicer/DEMO_03B_v0.2_TIFF存储模式兼容验证方案.md
docs/slicer/TASKS_03B_v0.2_TIFF存储模式兼容任务清单.md
```

当前阶段是：

```text
03B：TIFF StorageMode 兼容改造
```

目标：

```text
1. Writer 支持 stripped / tiled；
2. Reader 支持 stripped / tiled；
3. 默认输出 stripped；
4. 保留 tiled 兼容；
5. Manifest 显式记录 storageMode；
6. Regression 覆盖双模式；
7. MaterialPolicy 六个样例全部作为回归基线。
```

必须保持：

```text
channelOrder = R G B W S V
channelCount = 6
bitDepth = 8
sampleFormat = uint
polarity = black_is_print
printValue = 0
emptyValue = 255
PlanarConfig = contiguous
Model > Support > Empty
SupportType 不进入 TIFF 通道
MaterialPolicy RGB/W/V 语义不变
```

默认配置：

```text
output.storageMode = stripped
output.rowsPerStrip = 64
schema = p0.rgbwsv.2
```

兼容配置：

```text
output.storageMode = tiled
output.tileSize = [256, 256]
```

Schema 策略：

```text
Writer 默认输出 p0.rgbwsv.2
Reader 支持 p0.rgbwsv.1 legacy tiled
Reader 支持 p0.rgbwsv.2 stripped/tiled
```

不要做：

```text
材料策略修改
纹理采样修改
真实模型材料参数调优
RIP 半色调
ICC / CMYK
3MF
OpenVDB
Qt UI
支撑形态修复
```

完成后生成：

```text
docs/slicer/REPORT_03B_TIFF存储模式兼容当前实现状态.md
```

报告必须包含：

```text
1. 默认 storageMode；
2. schema 迁移策略；
3. p0.rgbwsv.1 兼容结果；
4. p0.rgbwsv.2 stripped/tiled 结果；
5. MaterialPolicy 六个样例回归结果；
6. bad storage package 结果。
```
