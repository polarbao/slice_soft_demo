# CODEX_PROMPT_05_材料策略基础实现指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_04A_纹理阶段收口修复当前实现状态.md
docs/slicer/DOC_DECISION_05_REPORT04A后进入材料策略基础阶段.md
docs/slicer/PRD_05_材料策略与白墨光油控制基础版.md
docs/slicer/DEV_05_MaterialPolicy白墨光油策略设计.md
docs/slicer/DEMO_05_材料策略组合验证方案.md
docs/slicer/TASKS_05_材料策略任务清单.md
```

当前阶段是：

```text
05：材料策略与白墨 / 光油控制基础版
```

目标：

```text
在现有 RGB texture 基础上，增加 W white underbase 和 V varnish top_n_layers / all_model 策略。
```

不要做：

```text
ICC / CMYK
RIP 半色调
3MF
OpenVDB
Qt UI
texture-driven varnish mask
texture-driven white mask
support morphology
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
1. materialPolicy 配置
2. RGB / W / V 策略组合
3. W underbase
4. V all_model
5. V top_n_layers
6. material_policy_report.json
7. material_policy 样例配置
8. run_regression.ps1 纳入 material policy
9. REPORT_05_材料策略当前实现状态.md
```
