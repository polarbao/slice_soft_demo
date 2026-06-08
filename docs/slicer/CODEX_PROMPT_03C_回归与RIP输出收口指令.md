# CODEX_PROMPT_03C_回归与RIP输出收口指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_03B_TIFF存储模式兼容当前实现状态.md
docs/slicer/DOC_DECISION_03C_REPORT03B后执行回归与RIP输出收口.md
docs/slicer/PRD_03C_回归脚本拆分与RIPReader输出收口.md
docs/slicer/DEV_03C_回归脚本与RIPReader摘要输出设计.md
docs/slicer/DEMO_03C_回归分层与RIP摘要验证方案.md
docs/slicer/TASKS_03C_回归与RIP输出收口任务清单.md
```

当前阶段是：

```text
03C：回归脚本拆分与 RIP Reader 输出收口
```

目标：

```text
1. run_regression.ps1 支持 quick / full / heavy；
2. rip_reader_test 支持 --summary / --quiet；
3. heavy relief / heavy texture 可独立运行；
4. 生成 RIP compatibility checklist；
5. 不破坏 03B storageMode 和 05 MaterialPolicy 基线。
```

不要做：

```text
TIFF storage 再次改造
MaterialPolicy 修改
Texture sampling 修改
3MF
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
```

必须保持：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
black_is_print
Model > Support > Empty
MaterialPolicy RGB/W/V 语义不变
```

完成后生成：

```text
docs/slicer/REPORT_03C_回归与RIP输出收口当前实现状态.md
```
