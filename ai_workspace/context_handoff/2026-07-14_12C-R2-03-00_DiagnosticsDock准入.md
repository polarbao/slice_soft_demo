# 12C-R2-03-00 DiagnosticsDock 准入交接

## 1. 结论

`12C-R2-03` 已达到代码实施准入。采用底部 `QDockWidget`，默认隐藏，通过“视图”菜单切换，不使用自定义中央折叠 splitter。

## 2. 固定边界

```text
DiagnosticsDock 唯一拥有 ReportPanel、ChannelChartPanel、LogPanel；
MainWindow 保留非 owning 指针以维持既有连接；
中央页签只保留预览和配置；
ConfigEditorPanel 不迁入诊断区；
OpenVDB 摘要等待 R2-04；
多尺寸最终验收等待 R2-05。
```

## 3. 验证入口

新增 `diagnostics-collapse` smoke，固定 objectName、默认隐藏、三页签、唯一 panel 实例、展开/收起、包加载和共享层不变契约。

详细文件：`docs/slice/DOC/DOC_CHECKLIST_12C_R2_03_DiagnosticsDock准入.md`。
