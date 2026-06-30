# DEV_07A_Qt参数编辑与Profile可视化设计

> 文档版本：v0.1  
> 建议目录：`docs/slicer/`

## 1. 技术目标

在现有 `apps/slicer_debug_ui` 上新增：

```text
ConfigDocument
ConfigValidator
ConfigEditorPanel
MaterialProcessProfileEditor
MaterialPolicyEditor
MaterialRoleMappingEditor
ChannelChartPanel
PreviewOverlayPanel
```

## 2. 推荐新增结构

```text
apps/slicer_debug_ui/widgets/
  ConfigEditorPanel.*
  MaterialProcessProfileEditor.*
  MaterialPolicyEditor.*
  MaterialRoleMappingEditor.*
  ChannelChartPanel.*
  PreviewOverlayPanel.*

apps/slicer_debug_ui/services/
  ConfigDocument.*
  ConfigValidator.*
  ConfigWriter.*
  ProfileReportModel.*
  PreviewOverlayComposer.*
```

## 3. ConfigDocument

职责：

- 加载 JSON config。
- 提供 get/set helper。
- 保留未知字段。
- 标记 dirty。
- Save / Save As。
- Revert。

## 4. ConfigValidator

轻量校验：

```text
input.modelPath 不为空
output.packageDir 不为空
storageMode = stripped / tiled
varnish.topLayers >= 0
role 合法
support.mode 合法
preview.interval > 0
```

## 5. Editor Panels

### MaterialProcessProfileEditor

覆盖：

```text
enabled / name / target / rgb / white / varnish / support / validation
```

### MaterialPolicyEditor

覆盖：

```text
enabled / rgb / white / varnish / conflictPolicy
```

### MaterialRoleMappingEditor

使用 `QTableWidget` 或 `QTableView` 编辑 rules。

## 6. ChannelChartPanel

输入：

```text
reports/material_process_report.json
```

显示：

```text
RGB/W/V/S per-layer printPixels
```

第一版使用 QPainter 自绘。

## 7. PreviewOverlayPanel

复用已有 preview image，使用 QPainter alpha blend 生成 overlay。

## 8. Self-test

`slicer_debug_ui --self-test` 增强：

- 构造 MainWindow。
- 加载默认 config。
- 加载默认 package。
- 初始化 editor/chart/overlay。
- 不进入 event loop。
- 返回 0。

## 9. 实施顺序

```text
ConfigDocument
MaterialProcessProfileEditor
MaterialPolicyEditor
MaterialRoleMappingEditor
Save / Save As
ChannelChartPanel
PreviewOverlayPanel
self-test
REPORT_07A
```
