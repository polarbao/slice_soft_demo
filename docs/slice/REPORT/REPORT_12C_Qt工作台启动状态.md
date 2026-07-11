# REPORT_12C Qt 工作台启动状态

> 文档状态：Stage Entry Report / 12C
> 日期：2026-07-10

## 1. 准入状态

```text
12A P0/P1 语义：基本完成；
12B R0/R1/R2：完成；
Profile/配置/预览基础组件：已存在；
fresh Qt UI build：PASS；
12C-R0-01：COMPLETE；
12C-R0-02/R0-03：PENDING；
12C-R1/R2：需等待 R0 基线阶段完成。
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

现有 binary 基线验证：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scenario-registry
```

结果：

```text
PASS scenario-registry default=11 fixture=7 advanced=6
```

该结果证明当前 registry 分层可读取，但不能替代 R0 的 fresh UI build gate。

## 3. Build Blocker 处理结果

Qt 5.15.2 与 MSVC 19.51 的 `stdext` 不兼容已通过项目内 target-scoped shim 解决。未修改 Qt 安装目录，未升级依赖。决策和验证见 `DOC_DECISION_12C_R0_01_QtMSVCFreshBuildLane.md`。

## 4. 执行入口

```text
docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_12C_Qt工作台收口执行指令.md
docs/slice/DOC/DOC_CHECKLIST_12C_阶段准入与上下文完整性.md
ai_workspace/context_handoff/2026-07-12_12C-R0-01_Qt构建准入.md
```

## 5. 下一任务

```text
12C-R0-02 UI Self-Test 与 Smoke 基线
```

本报告不表示 12C 功能已实现。当前只表示文档准入和 R0-01 fresh build gate 已完成。

初始审查中的 Profile 数量、dirty config 行为、诊断区域位置和 12D 接入方式，已由 `DOC_DECISION_12C_UI产品默认值与交互冻结.md` 关闭。R0 当前没有未决产品问题。
