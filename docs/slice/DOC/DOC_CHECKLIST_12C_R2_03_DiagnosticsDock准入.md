# DOC_CHECKLIST_12C_R2-03 DiagnosticsDock 准入

> 文档状态：READY TO IMPLEMENT
> 日期：2026-07-14
> 适用任务：12C-R2-03

## 1. 准入结论

`12C-R2-03 DiagnosticsDock` 可以开始代码实施。正式产品文档已经冻结底部、默认折叠和复用既有 panel 的方向；当前代码中的 `ReportPanel`、`ChannelChartPanel`、`LogPanel` 均可在不改业务逻辑的前提下迁移承载位置。

## 2. 容器决策

采用 Qt 原生 `QDockWidget`：

```text
停靠区域：Qt::BottomDockWidgetArea；
允许区域：仅 BottomDockWidgetArea；
默认状态：隐藏，避免永久挤压主预览；
发现入口：MainWindow “视图”菜单中的“诊断区域” toggleViewAction；
展开状态：底部显示报告/曲线/日志页签；
关闭行为：仅隐藏 dock，不销毁 panel 或清空日志；
浮动行为：禁用，避免调试 UI 多窗口状态复杂化。
```

不采用中央区内自定义折叠 splitter，原因是现有 `QMainWindow` 已提供成熟 dock 生命周期、菜单 action 和底部停靠能力，且隐藏时不占中央布局尺寸。

## 3. 组件所有权

```text
MainWindow
  DiagnosticsDock
    QTabWidget diagnosticsTabs
      ReportPanel
      ChannelChartPanel
      LogPanel
```

约束：

```text
DiagnosticsDock 唯一拥有三个 panel；
MainWindow 只保存非 owning 指针用于既有 signal、日志和包加载链路；
ReportPanel::warningsChanged 继续连接右侧 warnings_view；
ProcessRunner 输出继续连接同一个 LogPanel；
DiagnosticsDock::LoadPackage 只转发给 ReportPanel 和 ChannelChartPanel；
日志在 package 切换时不自动清空；
ConfigEditorPanel 继续保留中央顶级“配置”页签。
```

## 4. 布局与状态契约

```text
中央顶级页签只保留“预览”和“配置”；
报告、曲线、日志不得重复出现在中央页签；
DiagnosticsDock 默认显式 hidden；
展开建议高度 240 px，不设置会破坏 1024x768 的强制最小高度；
隐藏后中央工作区恢复全部可用高度；
dock title 和 tab 文案使用中文；
不在 R2-03 移动右侧材料工艺、警告或像素探针。
```

R2-03 只证明诊断区可折叠且不永久遮挡。1440x900、1280x720、1024x768 的最终截图和重叠检查仍由 R2-05 完成。

## 5. Smoke 契约

新增 `diagnostics-collapse`：

```text
MainWindow 中只存在一个 DiagnosticsDock；
objectName=diagnosticsDock；
内部 objectName=diagnosticsTabs；
默认 isHidden=true；
中央 mainWorkspaceTabs 只包含“预览”“配置”；
诊断 tabs 精确包含“报告”“曲线”“日志”；
三个既有 panel 各只有一个实例；
展开后 isHidden=false，收起后 isHidden=true；
加载 output/UiSmokeLayerPreview 后曲线层统计非空；
折叠操作不改变 PreviewWorkspace 的当前真实 layerIndex。
```

## 6. 文件影响面

```text
新增 apps/slicer_debug_ui/widgets/DiagnosticsDock.h/.cpp；
修改 apps/slicer_debug_ui/CMakeLists.txt；
修改 apps/slicer_debug_ui/MainWindow.h/.cpp；
修改 apps/slicer_debug_ui/services/UiSmokeTestRunner.h/.cpp；
更新 DEV、DEMO、TASKS、REPORT、用户手册和上下文交接。
```

## 7. 验证门禁

```powershell
cmake --build build-12c-ui --config Debug --target slicer_debug_ui
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case diagnostics-collapse --package output\UiSmokeLayerPreview
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case preview-legend-probe-context --package output\UiSmokeLayerPreview
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case preview-workspace-shared-layer --package output\UiSmokeOverlayRgbwv
ctest --test-dir build-12c-ui -C Debug --output-on-failure
git diff --check
```

## 8. 安全边界

```text
不修改 slicer_core 或切片算法；
不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
不读取或解释 OpenVDB utility report，留给 R2-04，并保持 productionReplacementAllowed=false；
不实现 12D 材料闭环判断；
不重写 ReportPanel、ChannelChartPanel 或 LogPanel；
不提前完成 R2-05 多尺寸最终验收。
```
