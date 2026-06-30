# CODEX_PROMPT_04_彩色纹理基础实现指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_03_RGBWSV协议固化当前实现状态.md
docs/slicer/DOC_DECISION_04_REPORT03后进入彩色纹理基础阶段.md
docs/slicer/PRD_04_彩色纹理模型切片基础版.md
docs/slicer/DEV_04_OBJ_MTL_Texture采样与RGB输出设计.md
docs/slicer/DEMO_04_彩色纹理切片验证方案.md
docs/slicer/TASKS_04_彩色纹理切片任务清单.md
```

当前阶段是：

```text
04：彩色纹理模型切片基础版
```

目标：

```text
OBJ + MTL + Texture
→ UV 采样
→ RGB 通道真实颜色输出
→ RGBWSV TIFF package
```

不要做：

```text
RIP 半色调
ICC / 色彩管理
CMYK
3MF
OpenVDB
Qt UI
完整光油覆盖策略
纹理驱动白墨 / 光油
```

必须保持：

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

优先实现：

```text
1. OBJ vt / face v/vt/vn 解析
2. MTL map_Kd 解析
3. texture image loader
4. UV sampler
5. relief top surface texture sampling
6. sampled RGB 写入 RGB 通道
7. texture_report.json
8. textured relief 样例
9. run_regression.ps1 加入 texture 正向样例
10. REPORT_04_彩色纹理切片当前实现状态.md
```
