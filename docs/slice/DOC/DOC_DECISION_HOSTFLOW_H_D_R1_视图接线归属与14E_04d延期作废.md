# DOC_DECISION_HOSTFLOW_H-D-R1 视图接线归属与 14E-04d 延期作废

> 文档状态：**ACTIVE / DECISION RECORD**
> 版本：v1.0 ｜ 日期：2026-08-08
> 定位：记录一次**决策反转** —— 把「渲染策略接线」从打印侧收回切片侧
> 关联：`REPORT_14E_04d`（被反转方）、`TASKS_HOSTFLOW` H-D 组、`TASKS_RENDER` R-A-01
> 证据等级：A=已核实代码事实，B=目标设计，P=判断

---

## 1. 被反转的原决定（A）

`REPORT_14E_04d` §5 原文：

> 「参考宿主的中央画布目前是双视图交互入口；真实场景载入与渲染策略接线
> **由打印侧集成时**按本任务类边界复用。」

同类表述亦见 `REPORT_14E_04c` §5：「尚未把 top/three_d 切换控件……接入可见 UI」。

## 2. 代码事实（A）

| 事实 | 位置 |
|---|---|
| 俯视/3D 画布是 `QLabel` + 静态文字，零像素输出 | `ViewWorkspaceWidget.cpp:13-27` |
| `HostMainWindow` 对画布仅三处调用：`SetMode` / `ShowViewError` / `SetSelectedInstances` | `HostMainWindow.cpp:61,97,403,482` |
| 七个渲染/交互类在 app 内的唯一引用者是批处理自检 | `CapabilityCoverageWorkflow.cpp` |

涉及的七个类：

```text
SceneRenderPolicy · TopViewRenderPolicy · CpuRasterBackend · CameraController
SceneInteractionController · TransformCommitPolicy · MoveOptimizationPolicy
```

**这些类本身是合格交付物** —— UI-M1..M13 门禁全绿、UI-M7=0 次 DLL 调用、
UI-M8 P5=51.4168 FPS 均为实测。问题不在实现质量，只在**没有接到可见画布上**。

## 3. 决定

```text
✅ 视图接线归【切片侧】，在 TASKS_HOSTFLOW 的 H-D 组内完成
❌ 作废「由打印侧集成时复用」的延期安排
```

### 3.1 三条理由（P）

**① 与「最少改动移植」目标直接冲突。**
Stage 14 的核心目标是让打印软件参照参考宿主、以最少改动完成移植。
把接线（本次工作中技术含量最高的一段之一）留给移植方，等于把最难的部分外包出去，
参考实现反而在最需要示范的地方缺席。

**② 违反本专项已冻结的验收口径。**
`TASKS_HOSTFLOW` §4：「一个没读过本项目文档的操作员，能在 `slicer_ui_host_sim` 中
独立完成……」。模型不可见时，排版是否重叠、实例是否越界、变换是否符合预期
**都无法用眼判断**，该口径不可能达成。

**③ 缺陷会被永久掩盖。**
`SceneViewMeshBuilder.cpp:143-146` 的 LOD 均匀跳采样缺陷在画布不显示时**无法被发现**。
接线是让这类显示缺陷暴露出来的唯一途径。

## 4. 根因与流程修正

**根因：验收方式与交付目标不匹配。**
14E-04c/04d 的门禁**全部是离屏渲染**（`stage14e04d_view_switch_tests` 使用 offscreen Qt）。
离屏测试能证明「渲染策略正确」，**不能证明「用户看得见」**。
`H-C-03` 的 A/B 对照 8 个维度中**也没有「显示」维度**，因此复查同样未能发现。

**修正（写入 `CODEX_PROMPT_HOSTFLOW` 坑 7）：**

```text
凡涉及【可见 UI】的卡片，验收必须把 app 真的跑起来并归档截图。
「离屏门禁全绿」不得作为 UI 可用性的证据。
```

## 5. 不改变的边界

```text
✅ PM_SPI_VERSION = 1 不变        ✅ 11 个 pm_* 导出不变
✅ 15 项能力不变                  ✅ p0.rgbwsv.2 / 通道序 / 位深 / 极性不变
✅ 生产 TIFF 与 Package 不变      ✅ 主干 apps/slicer_debug_ui 不改
```

H-D 组**不新增任何 ABI 面**：所有渲染数据仍来自既有 `scene.get_viewdata`，
接线全部发生在宿主本地。**UI-M1 / UI-M7 的零跨 DLL 调用在接线后必须重新实测**，
不得沿用接线前的数据。

## 6. 派生任务

| 卡号 | 内容 | 状态 |
|---|---|---|
| H-D-01 | 俯视画布接线 | PROPOSED |
| H-D-02 | 3D 画布 + 相机接线 | PROPOSED |
| H-D-03 | 三车道拖拽接线 | PROPOSED |
| H-D-04 | 视图刷新事件接线 | PROPOSED |
| H-D-05 | 打开包目录 | PROPOSED |
| H-D-06 | 端到端人工可操作性验收 + 回填 H-C-03「显示」维度 | PROPOSED |
| R-A-01 | 实测甲片三角面数（`TASKS_RENDER`，建议与 H-D-01 同批）| PROPOSED |

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-08 | v1.0 | 首版。记录 `REPORT_14E_04d` §5 接线延期决定的作废、A 级代码证据、三条理由、根因（离屏门禁不能证明 UI 可用）与流程修正，并登记 H-D 组 6 张派生卡 |
