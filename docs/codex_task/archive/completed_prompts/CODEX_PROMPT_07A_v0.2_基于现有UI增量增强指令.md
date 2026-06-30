# CODEX_PROMPT_07A_v0.2_基于现有UI增量增强指令

> 文档版本：v0.2  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_07_Qt调试UI当前实现状态.md
docs/slicer/DOC_REVIEW_07A_基于当前代码的文档修订判断.md
docs/slicer/DOC_DECISION_07A_v0.2_基于现有slicer_debug_ui增量增强.md
docs/slicer/TASKS_07A_v0.2_基于当前代码的修订任务清单.md
```

当前阶段：

```text
07A：Qt 参数编辑与 Profile 可视化增强
```

重要前提：

```text
当前 apps/slicer_debug_ui 已经存在，不要重建工程，不要替换 MainWindow。
```

必须保留：

```text
MainWindow
ProcessRunner
PackageLoader
ReportLoader
ToolPaths
LogPanel
MaterialProcessPanel
PreviewPanel
ReportPanel
```

增量新增：

```text
ConfigDocument
ConfigValidator
ConfigEditorPanel
MaterialProcessProfileEditor
MaterialPolicyEditor
MaterialRoleMappingEditor
SupportEditor
ChannelChartPanel
PreviewOverlayPanel
```

必须保持：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
black_is_print
Model > Support > Empty
MaterialRoleMapping 语义不变
MaterialPolicy 语义不变
MaterialProcessProfile 语义不变
S support 仍由 Support pipeline 独立生成
```

不要做：

```text
设备通信
喷头 bitstream
RIP 半色调
ICC / CMYK
OpenVDB
新的切片算法
生产级任务系统
完整 3D viewport
```

完成后生成：

```text
docs/slicer/REPORT_07A_Qt参数编辑与Profile可视化当前实现状态.md
```
