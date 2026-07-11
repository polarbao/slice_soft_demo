# DOC_AUDIT_12C 现有 Qt UI 能力与收口缺口审查

> 文档状态：Current Code Audit / Stage 12C
> 日期：2026-07-10

## 1. 审查目标

12C 不能按 2026-07-05 的初始设计从零实现。11、11B、12A 已经落地了一批 Profile、配置、生产层预览和材料叠加能力。本审查用于区分：

```text
A：当前代码已实现，可直接复用；
B：正式目标已定义，但尚未完整实现；
C：历史实验或 fixture，只作背景；
D：与当前代码冲突，不再作为实现依据。
```

## 2. Current State

### 2.1 场景与 Profile

当前 A 级能力：

```text
ScenarioRegistry 读取 samples/scenarios/slicer_scenarios.json；
场景已有中文 name/category/description；
visibility 支持 default/advanced/fixture；
默认隐藏高级和 fixture，可通过“显示全部场景”打开；
下拉框有动态宽度和 tooltip；
scenario-registry smoke 已存在。
```

缺口：

```text
Scenario 仍以 configPath 为中心，不是稳定产品 Profile；
没有独立 Profile schema/version/default override contract；
普通场景数量仍偏多，生产 Profile 与真实模型样例混在一起；
没有材料能力、输入格式、production safety 的结构化筛选。
```

判断：`12C-01` 为 `PARTIAL`，不得重新实现一套与 ScenarioRegistry 平行的目录系统。

### 2.2 配置与设置

当前 A 级能力：

```text
ConfigDocument 支持 load/save/saveAs/revert/validate/dirty；
QuickConfigPanel 已覆盖模型、输出、层高、纹理、非表面 RGB、白墨、光油、支撑、preview 和 OpenVDB；
高级页已有工艺 Profile、材料策略、材料角色、支撑和配置差异；
12A surfaceVarnish/outerVarnish 已有中文控件和 tooltip。
```

关键缺口：

```text
QuickConfigPanel 直接修改当前 ConfigDocument；
运行切片仍使用磁盘配置，dirty config 只提示警告；
尚无“模板 + UI override -> generated effective config”的正式链路；
普通用户仍需理解配置文件保存和磁盘状态；
缺少统一 SliceSettingsModel 和 effectiveConfig 摘要。
```

判断：`12C-02` 为 `PARTIAL`，R1 重点不是继续加控件，而是固化 generated config 工作流。

### 2.3 预览

当前 A 级能力：

```text
LayerPreviewPanel 已支持真实 layerIndex、production RGB、W/S/V、occupancy、diagnostic 和 RGBWSV 像素探针；
PreviewOverlayPanel 已支持同层 RGB/W/S/V 伪彩叠加；
PreviewPanel 可浏览原始 preview 文件；
12A semantic/sourcePolicy 已进入状态栏和像素解释；
已有 layer-preview-load 和 overlay-load-real smoke。
```

缺口：

```text
三种预览仍是三个顶级 tab；
LayerPreviewPanel 与 PreviewOverlayPanel 各自维护 layer slider；
没有共享 PreviewWorkspaceState；
模式切换时 layerIndex、zoom、探针上下文不能统一保留；
图例和像素探针没有固定在统一工作区。
```

判断：`12C-03` 的底层视图已存在，但工作区整合 `NOT_STARTED`。

### 2.4 报告、曲线与日志

当前 A 级能力：ReportPanel、ChannelChartPanel、LogPanel 均已存在。

缺口：报告和曲线仍占用中央顶级 tab，日志通过垂直 splitter 常驻；没有统一 DiagnosticsDock，也没有小窗口折叠策略。

判断：`12C-04` 为 `NOT_STARTED`，应复用现有 panel，不重写报告解析。

### 2.5 中文说明

当前 A 级能力：多数常用控件已有中文标签和 tooltip，用户手册已说明配置页和预览差异。

缺口：说明分散在 C++ 字符串和 Markdown；没有 HelpTextProvider、影响通道、默认值、生产安全级别和文档链接的统一数据源。

判断：`12C-05` 为 `PARTIAL`。

### 2.6 OpenVDB 状态表达

当前 A 级能力：按钮和场景已标注实验/候选，失败可加载 09P experimental report，UI self-test 覆盖 experimental report summary。

缺口：UI 尚未读取 `slicesoft.openvdb_sdf_utility.12b_r2.1`；没有展示 R2 的 utility role、promoteDecision 和 `productionReplacementAllowed=false`；benchmark 与 production action 的视觉层级仍需分开。

判断：`12C-06` 为 `PARTIAL`。

## 3. 构建准入 Blocker

2026-07-10 当前事实：

```text
canonical build 缓存引用已不存在的 MSVC 14.50.35717；
MSVC 19.51/14.51 可 fresh build slicer_cli；
Qt 5.15.2 fresh UI build 因 stdext::make_checked_array_iterator 被新 STL 移除而失败；
既有 UI binary --self-test 仍通过。
```

结论：12C 代码实现前必须先完成 `12C-R0-01 Qt/MSVC build lane`。不得用旧 binary self-test 代替 fresh UI build 准入。

## 4. Target State

```text
ScenarioRegistry 演进为稳定 ProfileCatalog 数据源，而非平行重写；
SliceSettingsModel 负责模板 + override + generated config；
PreviewWorkspace 复用三个现有 preview panel 并共享 layer state；
DiagnosticsDock 复用报告、曲线和日志 panel；
Help metadata 成为 UI tooltip 与 user guide 的统一来源；
OpenVDB 只显示为 utility/candidate，不成为生产默认引擎。
```

## 5. Historical State

07/07A/07B、11/11B 的 UI 文档和实现是当前能力的来源，但旧阶段中“调试配置执行器”的交互不再是 12C 目标。历史 fixture 继续保留，默认不向普通用户展示。

## 6. Pending Confirmation

初始审查中的四个开放问题已由 `DOC_DECISION_12C_UI产品默认值与交互冻结.md` 关闭：

```text
1. 普通用户默认四类稳定 Profile；
2. 运行时自动生成 session effective config，不覆盖原始 fixture；
3. DiagnosticsDock 默认位于底部并折叠；
4. 12C 不实现 12D 算法，仅允许后续只读显示已有报告。
```

R0 当前无产品需求待确认。Qt/MSVC 兼容路线必须由 R0-01 的实测证据决定，不属于产品开放项。

## 7. 准入结论

```text
12C 文档方向正确，但 v0.1 任务不能直接按“从零实现”执行；
必须先完成 build lane，再按增量收口路线推进；
12C 准备已经完成，可以进入 R0-01，不可直接跳到 PreviewWorkspace 大改。
```
