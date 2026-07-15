# DOC_CHECKLIST_12C-R2-05 阶段封口准入

> 文档状态：Readiness Complete / Stage 12C-R2-05
> 日期：2026-07-15
> 目标：冻结 12C 多尺寸布局、最终 Smoke、用户手册、状态报告和 12D 交接契约

## 1. 准入结论

```text
12C-R0 fresh Qt build：COMPLETE；
12C-R1 Profile/Settings/generated config/help：COMPLETE；
12C-R2-01 至 R2-04：COMPLETE；
R2-05 多尺寸布局和阶段封口契约：FROZEN；
R2-05：READY TO IMPLEMENT。
```

R2-05 只收口 Qt 工作台布局、自动化验证和文档，不修改切片算法、材料策略、OpenVDB 计算、生产 package 或 RGBWSV 协议。

## 2. 当前代码事实

`MainWindow` 当前三列最小宽度为：

```text
左侧项目区 >= 320 px；
中央工作区 >= 620 px；
右侧参数区 >= 360 px；
合计未包含 splitter handle 和 layout margin 已达到 1300 px。
```

因此现有代码不能真实满足 1024x768。R2-05 不能只补 Smoke 文案，必须先做最小响应式调整，再由几何 Smoke 证明目标尺寸可用。

## 3. 冻结布局方案

### 3.1 三列边界

```text
左侧 projectPanel：最小 280 px，最大 440 px；
中央 mainWorkspaceTabs：最小 400 px，保持最高 stretch factor；
右侧 rightDiagnosticsPanel：最小 240 px，最大 420 px；
mainSplitter：稳定 objectName，供 Smoke 查询实际几何。
```

目标不是让三列等宽，而是在 1024px 宽度下优先保证中央预览，同时保留左右入口。长路径继续通过 tooltip 查看完整值，不强制完整铺开。

### 3.2 左侧收口

场景/Profile 行拆成两行：

```text
第一行：场景/Profile + 下拉框；
第二行：显示全部场景 + 刷新。
```

路径输入保持省略显示和 tooltip，不新增横向滚动主页面，不删除任何运行入口。

### 3.3 预览图例

材料图例从单行水平布局调整为紧凑两行网格：

```text
第一行：RGB、W、S；
第二行：V、真实空白。
```

图例文字、伪彩和生产协议语义不变，不写回 TIFF。

### 3.4 诊断区域

DiagnosticsDock 继续默认隐藏、bottom-only。多尺寸 Smoke 必须同时检查：

```text
默认隐藏时不挤压中央预览；
展开时位于中央工作区下方而非覆盖其内容；
收起后恢复中央空间；
展开/收起不改变共享真实 layerIndex。
```

## 4. workspace-layout-sizes Smoke

新增单一 case：

```text
workspace-layout-sizes
```

目标尺寸：

```text
1440x900；
1280x720；
1024x768。
```

每个尺寸的稳定断言：

```text
窗口能够缩放到请求尺寸，不被旧 minimumSizeHint 强制放大；
projectPanel、mainWorkspaceTabs、rightDiagnosticsPanel 均可见且宽度为正；
三列全局几何不相互重叠，并位于 mainSplitter 范围内；
中央工作区宽度不低于 400 px；
PreviewWorkspace 当前 layerIndex 在 resize 和 dock toggle 前后不变；
DiagnosticsDock 默认隐藏，展开后与中央工作区不重叠；
菜单 action 与 dock 可见状态一致。
```

自动化几何检查是 R2-05 的可重复门禁。人工截图只作为补充，不替代 Smoke，也不因截图缺失阻断代码级验收。

## 5. 最终 Smoke 集合

新增统一脚本：

```text
scripts/Run12CUiClosure.ps1
```

脚本必须使用指定 build dir 中的 fresh `slicer_cli` 和 `slicer_debug_ui`：

```text
构建 slicer_cli + slicer_debug_ui；
用 ui_layer_preview.json 生成 output/UiSmokeLayerPreview；
用 ui_overlay_rgbwv_preview.json 生成 output/UiSmokeOverlayRgbwv；
运行 --self-test；
运行 scenario-registry；
运行 slice-settings-model；
运行 generated-effective-config；
运行 setting-help-metadata；
运行 preview-workspace-shared-layer；
运行 preview-legend-probe-context；
运行 diagnostics-collapse；
运行 openvdb-utility-summary；
运行 workspace-layout-sizes；
运行 layer-preview-load；
运行 overlay-load-real。
```

脚本遇到任一非零退出码立即失败，不允许继续并输出伪 PASS。

## 6. Fresh Build 与测试门禁

R2-05 使用新的 build 目录进行最终准入：

```powershell
.\scripts\Configure12CQtUi.ps1 -BuildDir build-12c-ui-r2-final -Config Debug
.\scripts\Run12CUiClosure.ps1 -BuildDir build-12c-ui-r2-final -Config Debug
ctest --test-dir build-12c-ui-r2-final -C Debug --output-on-failure
git diff --check
```

历史 `build-12c-ui` 可用于开发增量验证，但不能替代最终 fresh lane 证据。

## 7. 用户手册验收点

手册必须覆盖：

```text
统一预览三种模式和共享真实 layerIndex；
RGB/W/S/V/真实空白图例和生产值边界；
六通道像素探针；
视图 -> 诊断区域；
独立 OpenVDB Utility 报告加载；
1024x768 下三列仍可用、长路径通过 tooltip 查看；
OpenVDB 仍是 utility/candidate，Legacy 仍是默认生产路径；
当前不支持的生产功能。
```

## 8. 最终报告结构

生成：

```text
docs/slice/REPORT/REPORT_12C_Qt工作台当前状态.md
```

固定章节：

```text
1. 阶段结论；
2. R0/R1/R2 完成矩阵；
3. 当前 UI 功能；
4. 配置生成与生产安全边界；
5. 预览与诊断能力；
6. 构建和 Smoke 证据；
7. 已知限制；
8. 12D 交接条件；
9. 下一阶段建议。
```

R2-05 完成后，历史 `REPORT_12C_Qt工作台启动状态.md` 保留，不删除。

## 9. 12D 交接条件

R2-05 完成后检查：

```text
REPORT_12C_Qt工作台当前状态.md 已生成；
12C final fresh lane 和 Smoke 全通过；
12D-R0 PRD/DEV/DEMO/schema/fixture matrix/TASKS/CODEX_PROMPT 已存在；
12D 代码从 12D-02 MaterialClosureConfig 开始；
12D 不回写或重构 12C PreviewWorkspace/DiagnosticsDock；
12D-R3 UI 展示复用 ReportPanel/DiagnosticsDock，不自行重新判定闭环。
```

满足以上条件时，12D-R1 可以开始；不需要在 R2-05 提前实现 MaterialClosureConfig 或闭环算法。

## 10. 预计影响文件

```text
apps/slicer_debug_ui/MainWindow.cpp
apps/slicer_debug_ui/widgets/PreviewWorkspace.cpp
apps/slicer_debug_ui/services/UiSmokeTestRunner.h/.cpp
scripts/Run12CUiClosure.ps1
docs/user_guides/QT_DEBUG_UI_操作手册.md
docs/slice/REPORT/REPORT_12C_Qt工作台当前状态.md
12C TASKS/DEV/DEMO/checklist/context pointers
12D REPORT/context pointers（仅在 12C 完成后更新准入状态）
```

## 11. 最终判断

R2-05 的响应式修改边界、三尺寸自动化断言、最终 Smoke 集合、fresh lane、报告结构和 12D 交接条件均已冻结，可以进入代码实施。
