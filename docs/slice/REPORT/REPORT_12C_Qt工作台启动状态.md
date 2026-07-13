# REPORT_12C Qt 工作台启动状态

> 文档状态：Stage Entry Report / 12C
> 日期：2026-07-13

## 1. 准入状态

```text
12A P0/P1 语义：基本完成；
12B R0/R1/R2：完成；
Profile/配置/预览基础组件：已存在；
fresh Qt UI build：PASS；
12C-R0-01：COMPLETE；
12C-R0-02：COMPLETE；
12C-R0-03：COMPLETE；
12C-R0：COMPLETE；
12C-R1-01：COMPLETE；
12C-R1-02：READY TO START；
12C-R2：需等待 R1 设置管线完成。
文档与上下文准备：COMPLETE；
```

## 2. 当前可复用能力

```text
ScenarioRegistry + visibility；
ConfigDocument + QuickConfigPanel；
LayerPreviewPanel + RGBWSV probe；
PreviewOverlayPanel；
PreviewPanel；
ReportPanel / ChannelChartPanel / LogPanel；
UiSmokeTestRunner。
```

R0 fresh binary 基线验证：

```powershell
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scenario-registry
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case save-as-config --config samples\configs\material_process\nail_rgb_white_varnish_top2.json --output output\ui_smoke_generated_top3_12c_r0_02.json --yes
```

结果：

```text
PASS startup
PASS experimental-report-summary
PASS scenario-registry default=11 fixture=7 advanced=6
PASS layer-preview-load layers=25 channels=production_rgb,rgb,white,support,varnish,occupancy,diagnostic
PASS overlay-load-real images=47 channels=rgb,support,varnish,white modes=RGB + W 白墨,RGB + V 光油,RGB + S 支撑
PASS save-as-config
```

上述结果均来自 `build-12c-ui` fresh lane，R0-02 smoke 基线已闭环。

R1-01 增量验证：

```text
PASS scenario-registry default=4 fixture=7 advanced=17
场景索引 schema=slice_soft.scenarios.2；
默认 Profile=textured_nail_rgb_white_lower_support；
四个普通 Profile 均具有 displayName/inputFormats/materialCapabilities/productionSafety/docPath；
历史样例仍保留在 advanced/fixture，不删除回归配置。
```

R1-01 只冻结 Profile 目录和元数据。白墨/光油两个彩色 Profile 当前复用同一基础模板，二者的默认模型填充差异将在 R1-02/R1-03 由 `SliceSettingsModel + generated effective config` 落地；在该设置管线完成前，不应把“选择 Profile 后直接运行模板”理解为最终产品行为。

## 3. Build Blocker 处理结果

Qt 5.15.2 与 MSVC 19.51 的 `stdext` 不兼容已通过项目内 target-scoped shim 解决。未修改 Qt 安装目录，未升级依赖。决策和验证见 `DOC_DECISION_12C_R0_01_QtMSVCFreshBuildLane.md`。

## 4. 执行入口

```text
docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_12C_Qt工作台收口执行指令.md
docs/slice/DOC/DOC_CHECKLIST_12C_阶段准入与上下文完整性.md
ai_workspace/context_handoff/2026-07-12_12C-R0-02_UI_Smoke基线.md
docs/slice/DOC/DOC_AUDIT_12C_R0_03_现有Qt布局与组件复用基线.md
ai_workspace/context_handoff/2026-07-12_12C-R0-03_布局组件基线.md
```

## 5. 下一任务

```text
12C-R1-02 SliceSettingsModel
```

本报告不表示 12C 功能已全部实现。当前只表示 R0 构建、Smoke、布局与组件复用基线已完成，R1/R2 产品化改造仍待执行。

初始审查中的 Profile 数量、dirty config 行为、诊断区域位置和 12D 接入方式，已由 `DOC_DECISION_12C_UI产品默认值与交互冻结.md` 关闭。R0 当前没有未决产品问题。
