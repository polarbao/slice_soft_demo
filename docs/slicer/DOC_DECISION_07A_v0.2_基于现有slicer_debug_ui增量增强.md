# DOC_DECISION_07A_v0.2_基于现有slicer_debug_ui增量增强

> 文档版本：v0.2  
> 文档状态：Decision / 修订版  
> 适用阶段：REPORT_07 之后  
> 建议提交目录：`docs/slicer/`

---

## 1. 阶段结论

07 阶段当前代码已经完成基础调试 UI。

因此 07A 不再定义为“新建 Qt 调试 UI”，而定义为：

```text
基于现有 slicer_debug_ui 的参数编辑与 profile 可视化增强阶段
```

---

## 2. 当前保留结构

07A 必须保留当前已实现结构：

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

不得为了 07A 重写上述基础功能。

---

## 3. 07A 增量新增内容

新增 services：

```text
ConfigDocument
ConfigValidator
ConfigWriter
ProfileReportModel
PreviewOverlayComposer
```

新增 widgets：

```text
ConfigEditorPanel
MaterialProcessProfileEditor
MaterialPolicyEditor
MaterialRoleMappingEditor
SupportEditor
ChannelChartPanel
PreviewOverlayPanel
```

---

## 4. MainWindow 集成建议

当前 MainWindow 已有：

```text
left project/run panel
center Preview/Reports tabs
right Material/Warnings/Compare tabs
bottom LogPanel
```

07A 建议增量集成：

```text
center:
  Preview
  Reports
  Config
  Charts
  Overlay

right:
  Material
  Profile Editor
  Warnings
  Compare
```

---

## 5. 执行原则

```text
1. 优先 Save As，避免覆盖已有 sample config；
2. 不修改 slicer_core 输出协议；
3. 不改变 MaterialPolicy / MaterialProcessProfile 执行语义；
4. 编辑器只修改 config JSON；
5. 图表和 overlay 只读取已生成 reports/preview；
6. 运行仍通过 QProcess 调用现有 CLI 和脚本。
```

---

## 6. 非目标

07A 不做：

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
