# DEV_07B_UI自动化与配置编辑器收口设计

> 文档版本：v0.1  
> 建议目录：`docs/slicer/`

## 1. 新增代码

```text
apps/slicer_debug_ui/services/UiSmokeTestRunner.*
apps/slicer_debug_ui/services/PreviewReportIndex.*
apps/slicer_debug_ui/services/ConfigDiffModel.*
apps/slicer_debug_ui/widgets/ConfigDiffPanel.*
```

## 2. UiSmokeTestRunner

职责：

```text
构造 QApplication / MainWindow
加载 config/package
触发指定 UI 操作
检查 widget 状态
返回 0 / 非 0
```

支持：

```text
startup
load-package
save-as-config
chart-load
overlay-load
compare-profiles
```

## 3. main.cpp 参数

新增：

```text
--ui-smoke-test
--case
--config
--package
--package-a
--package-b
--output
--yes
```

## 4. Save 覆盖确认

建议接口：

```cpp
struct SaveOptions {
    bool allowOverwriteWithoutPrompt{false};
};

bool ConfigDocument::save(QWidget* parent, SaveOptions options);
bool ConfigDocument::saveAs(const QString& path, QWidget* parent, SaveOptions options);
```

交互模式下弹 `QMessageBox::question`，smoke test 用 `--yes` 自动确认。

## 5. ConfigDiffModel

比较 original/current `QJsonDocument`，输出：

```text
path
oldValue
newValue
```

第一版不需要完整 JSON Patch。

## 6. PreviewReportIndex

新增 parser：

```text
PreviewReportIndex::load(packageDir)
```

优先支持：

```text
schema = p0.preview_report.1
files[].path
files[].channel
files[].layerIndex
files[].kind
```

并兼容旧字段：

```text
files / generated / previewFiles
```

`PreviewOverlayPanel` 改为优先使用 `PreviewReportIndex`，再 fallback 到目录扫描。

## 7. 实施顺序

```text
UiSmokeTestRunner skeleton
main.cpp 参数解析
Save overwrite confirm
ConfigDiffModel / ConfigDiffPanel
enum combobox hardening
PreviewReportIndex
PreviewOverlayPanel 接入
smoke-test cases
quick regression
REPORT_07B
```
