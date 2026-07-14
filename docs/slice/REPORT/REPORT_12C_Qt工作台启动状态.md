# REPORT_12C Qt 工作台启动状态

> 文档状态：Stage Entry Report / 12C
> 日期：2026-07-14

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
12C-R1-02：COMPLETE；
12C-R1-03：COMPLETE；
12C-R1-04：COMPLETE；
12C-R1：COMPLETE；
12C-R2：READY TO START；
文档与上下文准备：COMPLETE；
```

## 2. 当前可复用能力

```text
ScenarioRegistry + visibility；
ConfigDocument + QuickConfigPanel；
SliceSettingsModel + EffectiveConfigGenerator；
HelpTextProvider + SettingHelpPanel；
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

R1-01 只冻结 Profile 目录和元数据。白墨/光油两个彩色 Profile 继续复用同一只读基础模板，二者的默认模型填充差异现已由 R1-02/R1-03 的 `SliceSettingsModel + generated effective config` 落地。

R1-02 增量验证：

```text
PASS slice-settings-model profiles=4 legacy-default=true openvdb=candidate-only
```

当前四个稳定 Profile 已在 `SliceSettingsModel` 中形成白墨/光油等不同默认状态。

R1-03 增量验证：

```text
PASS generated-effective-config differences=21 template-readonly=true
```

当前运行链路已变为：

```text
Profile template + 内存 UI override + SliceSettingsState
-> session/slice_config.generated.json
-> SliceSettingsModel/ConfigValidator 校验
-> slicer_cli
```

“运行切片”和“一键导入模型并切片”不再忽略未保存 UI 设置，也不覆盖原 template/fixture。配置页“生效配置”可查看实际 CLI 配置摘要、告警、错误和逐字段差异。模型内部填充、支撑 placement、内部镂空、表面/外侧光油及 preview 已进入同一生成链路。

OpenVDB 保持 utility/candidate 边界：普通切片强制 legacy；实验诊断生成 `writeProductionRgbwsv=false` 的诊断配置；既有显式候选入口不被提升为默认生产路径。

R1-04 增量验证：

```text
PASS setting-help-metadata entries=21 required=10 tooltips=7
```

配置页已新增“设置说明”页签。21 个设置项通过 `HelpTextProvider` 集中提供中文标题、描述、影响范围、默认值、生产安全和文档路径；常用设置 tooltip 与详细说明面板使用同一数据源。模型填充、支撑、内部镂空、表面/外侧光油、preview、Legacy 和 OpenVDB 候选均已有一致说明。

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
ai_workspace/context_handoff/2026-07-13_12C-R1-03_GeneratedEffectiveConfig.md
ai_workspace/context_handoff/2026-07-14_12C-R1-04_设置项中文帮助元数据.md
```

## 5. 下一任务

```text
12C-R2-01 PreviewWorkspace 与共享层状态
```

本报告不表示 12C 功能已全部实现。当前表示 R0 与完整 R1 设置管线已完成；R2 预览与诊断工作区仍待执行。

初始审查中的 Profile 数量、dirty config 行为、诊断区域位置和 12D 接入方式，已由 `DOC_DECISION_12C_UI产品默认值与交互冻结.md` 关闭。R0 当前没有未决产品问题。
