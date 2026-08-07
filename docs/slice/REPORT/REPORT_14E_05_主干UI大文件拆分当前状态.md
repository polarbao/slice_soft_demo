# REPORT_14E-05 主干 UI 大文件拆分当前状态

> 状态：**COMPLETE / STRUCTURE GATE PASS**
> 日期：2026-08-07
> 任务：14E-05
> 依据：`docs/claude/INTEGRATION/INT_11_文件拆分与结构治理专项.md`、`REPORT_14B_06_CI行数与结构门禁.md`

## 1. 结论

14E-05 已按职责拆分主干 Qt 调试 UI 的两个历史大文件：

- `MainWindow.cpp`：由 4267 行降至 1218 行；
- `services/UiSmokeTestRunner.cpp`：由 7401 行降至 642 行；
- 新增实现文件均不超过 500 行；
- 两个历史文件已从 14B-06 临时白名单移除；
- Debug/Release 构建、自测、代表性 UI Smoke 与行数门禁均通过。

本任务只调整实现文件布局、内部辅助声明和 CMake 源文件清单，不改变 Qt 交互行为、切片算法、公开 ABI、RGBWSV TIFF 或生产 Package。

## 2. MainWindow 拆分

| 文件 | 职责 | 物理行数 |
|---|---|---:|
| `MainWindow.cpp` | 实现级公共辅助、构造与基础装配 | 1218 |
| `MainWindowLifecycle.cpp` | 初始化、窗口状态与生命周期 | 395 |
| `MainWindowPanels.cpp` | 工作区和面板装配 | 465 |
| `MainWindowConfigActions.cpp` | 配置文件动作与持久化 | 411 |
| `MainWindowProductionSettings.cpp` | 生产参数和控件同步 | 319 |
| `MainWindowDiagnostics.cpp` | 诊断请求、结果与状态呈现 | 457 |
| `MainWindowPreflight.cpp` | 模型预检与准入交互 | 427 |
| `MainWindowScenarios.cpp` | 场景/Profile 选择与默认值 | 355 |
| `MainWindowSceneSlice.cpp` | 当前场景切片、进度与取消 | 445 |
| `MainWindowInternal.h` | 仅供实现文件共享的内部声明 | 98 |

## 3. UiSmokeTestRunner 拆分

`UiSmokeTestRunner.cpp` 仅保留共享测试辅助、用例路由和路径解析。用例实现按工作台、配置、生产纹理、Package、Preview、预检、模型视图、批量导入、场景切片和报告等职责拆入 19 个文件。

拆分后的最大用例文件为 `UiSmokeWorkbenchLayoutCases.cpp` 485 行；其余新增实现文件均低于 500 行。`UiSmokeTestInternal.h` 只提供实现级 fixture 辅助声明，不形成新的产品 API。

## 4. 门禁变化

`scripts/SourceSizeGuardConfig.json` 的 `allowlist` 已清空：

```json
"allowlist": []
```

后续 `MainWindow` 与 `UiSmokeTestRunner` 相关新增文件直接受 G1/G3 约束，不允许重新加入无限期白名单。历史 G4/G5 告警仍作为后续结构治理输入，不阻断本卡。

## 5. 验证证据

已实际运行并通过：

```powershell
cmake --build --preset slicesoft-debug --target slicer_debug_ui
cmake --build --preset slicesoft-release --target slicer_debug_ui

$env:QT_QPA_PLATFORM='offscreen'
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test
build-slicesoft/main/apps/slicer_debug_ui/Release/slicer_debug_ui.exe --self-test

build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --ui-smoke-test --case workbench-job-action-bar --repo-root .
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --ui-smoke-test --case workbench-context-inspector --repo-root .
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --ui-smoke-test --case diagnostic-settings-controls --repo-root .
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --ui-smoke-test --case production-texture-controls --repo-root .
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --ui-smoke-test --case scene-grid-layout --repo-root .
build-slicesoft/main/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --ui-smoke-test --case experimental-report-summary --repo-root .

python scripts/ValidateSourceSizeGuard.py --self-test
python scripts/ValidateSourceSizeGuard.py --base-ref HEAD
```

行数门禁结果为 PASS；全树仍报告 34 项既有 G4/G5 非阻断告警，本卡拆分文件不在告警中。

## 6. 后续

14E-05 已完成。下一任务为 14E-06：输出打印侧可移植模块文件级清单，明确“可直接复制”和“需改写”边界。
