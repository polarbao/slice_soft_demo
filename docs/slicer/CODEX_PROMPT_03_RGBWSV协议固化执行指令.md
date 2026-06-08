# CODEX_PROMPT_03_RGBWSV协议固化执行指令

> 文档版本：v0.4  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

## 指令

请先阅读：

```text
docs/slicer/REPORT_02_支撑与孤岛检测当前实现状态.md
docs/slicer/DOC_DECISION_03_REPORT02后进入协议固化阶段.md
docs/slicer/PRD_03_v0.4_RGBWSV协议固化与负向测试.md
docs/slicer/DEV_03_v0.4_TIFFWriter_RIPReader协议固化设计.md
docs/slicer/TASKS_03_v0.4_RGBWSV协议固化任务清单.md
```

当前阶段是：

```text
03：RGBWSV TIFF / manifest / RIP Reader 输入协议固化与负向测试
```

不要做：

```text
彩色纹理
UV / MTL / Texture
RIP 半色调
CMYK 分色
喷头 bitstream
OpenVDB
Qt UI
复杂支撑树
```

必须保持：

```text
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
SamplesPerPixel = 6
PlanarConfig = contiguous
Model > Support > Empty
SupportType 不进入 TIFF 通道
```

优先实现：

```text
1. manifest schema = p0.rgbwsv.1
2. reader 校验 schema / tiff / grid / layer list
3. TIFF metadata 校验
4. ValidationErrorCode 或等价错误码
5. bad package 负向测试
6. printPixels / emptyPixels 统计字段
7. scripts/run_regression.ps1
8. REPORT_03_RGBWSV协议固化当前实现状态.md
```

完成后请输出：

```text
1. 修改了哪些文件；
2. schema 如何写入；
3. reader 现在校验哪些字段；
4. 负向测试覆盖哪些场景；
5. 回归脚本如何运行；
6. REPORT_03 的结论。
```
