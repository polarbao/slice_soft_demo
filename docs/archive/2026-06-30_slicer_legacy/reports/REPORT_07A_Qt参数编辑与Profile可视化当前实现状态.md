# REPORT_07A_Qt参数编辑与Profile可视化当前实现状态

> 实现日期：2026-06-09  
> 阶段：07A Qt 参数编辑与 Profile 可视化增强  
> 状态：已完成第一版增量实现，命令级验证通过  

## 1. 阶段边界

07A 基于现有 `apps/slicer_debug_ui` 做增量增强，没有重建 UI 工程，没有新增第二个 UI target。

本阶段明确保持：

- 07A 未修改 `slicer_core` 输出协议。
- 07A 未修改 `MaterialPolicy` / `MaterialRoleMapping` / `MaterialProcessProfile` 执行语义。
- 07A 只是 UI 增强，新增能力只用于配置 JSON 编辑、报告读取和 preview 可视化。
- 仍保持 `schema=p0.rgbwsv.2`、`channelOrder=R G B W S V`、`bitDepth=8`、`polarity=black_is_print`。

## 2. 新增 services

- `apps/slicer_debug_ui/services/ConfigDocument.h`
- `apps/slicer_debug_ui/services/ConfigDocument.cpp`
- `apps/slicer_debug_ui/services/ConfigValidator.h`
- `apps/slicer_debug_ui/services/ConfigValidator.cpp`

当前支持：

- 加载 JSON config。
- 保留未知字段。
- 通过 path helper 读写嵌套字段。
- 维护 dirty 状态。
- 支持 Save / Save As / Revert。
- 保存前执行基础校验，错误阻止保存，warning 允许保存。

基础校验覆盖：

- `input.modelPath` 非空。
- `output.packageDir` 非空。
- `output.storageMode` 为 `stripped` 或 `tiled`。
- `materialRoleMapping.defaultRole` / `rules[].role` 合法。
- `materialPolicy.varnish.topLayers >= 0`。
- `materialProcessProfile.varnish.topLayers >= 0`。
- `support.mode` 为当前基础模式之一。
- `preview.interval > 0`。

## 3. 新增 widgets

- `ConfigEditorPanel`
- `MaterialProcessProfileEditor`
- `MaterialPolicyEditor`
- `MaterialRoleMappingEditor`
- `SupportEditor`
- `ChannelChartPanel`
- `PreviewOverlayPanel`

## 4. MainWindow 集成方式

现有功能保留：

- `Preview`
- `Reports`
- `Material`
- `Warnings`
- `Compare`
- `LogPanel`
- `Run Slicer`
- `Run RIP Summary`
- `Run Quick Regression`
- `Compare Profiles`

新增 center tabs：

- `配置`
- `曲线`
- `叠加预览`

`Load Package` 和 `Run Slicer` 成功后的自动加载会同步刷新：

- `ReportPanel`
- `PreviewPanel`
- `MaterialProcessPanel`
- `ChannelChartPanel`
- `PreviewOverlayPanel`

## 5. Config 编辑支持范围

`MaterialProcessProfileEditor` 当前支持：

- `materialProcessProfile.enabled`
- `materialProcessProfile.name`
- `materialProcessProfile.target`
- `materialProcessProfile.rgb.enabled`
- `materialProcessProfile.white.enabled`
- `materialProcessProfile.white.coverage`
- `materialProcessProfile.white.expandPx`
- `materialProcessProfile.white.shrinkPx`
- `materialProcessProfile.varnish.enabled`
- `materialProcessProfile.varnish.topLayers`
- `materialProcessProfile.support.expected`
- `materialProcessProfile.validation.requireRgbPixels`
- `materialProcessProfile.validation.requireWhitePixels`
- `materialProcessProfile.validation.requireVarnishPixels`
- `materialProcessProfile.validation.requireSupportPixels`

`MaterialPolicyEditor` 当前支持：

- `materialPolicy.enabled`
- `materialPolicy.rgb.enabled`
- `materialPolicy.rgb.source`
- `materialPolicy.white.enabled`
- `materialPolicy.white.mode`
- `materialPolicy.white.layers`
- `materialPolicy.white.value`
- `materialPolicy.varnish.enabled`
- `materialPolicy.varnish.mode`
- `materialPolicy.varnish.topLayers`
- `materialPolicy.varnish.value`
- `materialPolicy.conflictPolicy`

`MaterialRoleMappingEditor` 当前支持：

- `materialRoleMapping.enabled`
- `materialRoleMapping.defaultRole`
- `materialRoleMapping.allowInputSupportMaterial`
- `materialRoleMapping.rules`

`SupportEditor` 当前支持：

- `support.enabled`
- `support.mode`
- `support.minIslandAreaPx`
- `support.xyDilationPx`
- `support.connectivity`

## 6. Save / Save As / Validate 行为

- `Save`：保存当前路径配置。
- `Save As`：选择新路径并保存。
- `Revert`：重新从磁盘加载当前配置。
- `Validate`：展示 warning / error。
- 当前未实现：`Save` 覆盖前二次确认弹窗。

## 7. ChannelChartPanel 支持范围

`ChannelChartPanel` 读取 `reports/material_process_report.json` 的 `layers[]` 字段，并用 QPainter 绘制：

- RGB per-layer `rgbPrintPixels`
- W per-layer `whitePrintPixels`
- V per-layer `varnishPrintPixels`
- S per-layer `supportPrintPixels`

支持：

- RGB/W/V/S checkbox。
- 鼠标 hover 显示 layer index 与通道像素数。
- 缺少报告时显示 warning，不崩溃。

## 8. PreviewOverlayPanel 支持范围

`PreviewOverlayPanel` 当前支持：

- 单通道预览。
- RGB + W 白墨 overlay。
- RGB + V 光油 overlay。
- RGB + S 支撑 overlay。
- layer slider。
- zoom in / zoom out。
- fit to window。

数据源策略：

- 优先读取 `reports/preview_report.json` 中的 `files` / `generated` / `previewFiles`。
- 如果报告未列出文件，则 fallback 到 `preview` 目录扫描和文件名 token 分类。
- 不重新运行 slicer。

## 9. 文档整理

上一阶段的软件配置说明已从 `docs/slicer` 移出，当前路径：

- `docs/user_guides/QT_DEBUG_UI_软件配置说明.md`

这样避免与 PRD / DEV / DEMO / REPORT 类阶段文档混放。

## 10. 验证结果

已执行：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
```

结果：通过，生成：

```text
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe
```

已执行：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

结果：返回 0，通过。

已执行：

```powershell
.\scripts\run_regression.ps1 -Mode quick
```

结果：通过，输出 `Regression complete. mode=quick`。

补充执行：

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\nail_rgb_white_varnish_top2.json
.\build\Debug\rip_reader_test.exe --package output\NailRgbWhiteVarnishTop2 --summary
```

结果：

- `slicer_cli` 成功生成 `output/NailRgbWhiteVarnishTop2`。
- `rip_reader_test` PASS。
- summary 显示 `schema=p0.rgbwsv.2`、`storageMode=stripped`、`bitDepth=8`、`channelOrder=R G B W S V`。

## 11. 未实现项

- 未实现 `Save` 覆盖前二次确认弹窗。
- 未人工点击验证 GUI 中的 Save As / Overlay / Chart 交互，只完成了构建与 `--self-test` 初始化验证。
- `PreviewOverlayPanel` 的 preview metadata 解析为基础兼容实现，复杂 preview_report schema 后续仍可继续增强。
- 未在 right tabs 额外增加 `Profile Editor`，当前配置编辑入口统一放在 center `配置` tab。

## 12. 下一阶段建议

- 增加 UI 自动化或脚本化 smoke test，覆盖 Save As、打开 package、切换曲线、切换 overlay。
- 为 `PreviewOverlayPanel` 固化 preview_report schema，减少对文件名 token 的依赖。
- 为 Config 编辑器增加覆盖保存确认、字段枚举下拉、配置差异预览。
- 若后续进入生产 UI，再考虑独立 ViewModel、撤销/重做和更细粒度字段校验。
