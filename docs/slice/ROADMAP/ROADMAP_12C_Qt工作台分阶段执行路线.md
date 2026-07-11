# ROADMAP_12C Qt 工作台分阶段执行路线

> 文档状态：ROADMAP / Stage 12C
> 日期：2026-07-10

## 1. Goal

把现有 Qt 调试 UI 增量收口为 Profile、设置、统一预览和诊断工作台，不重复实现 11/12A 已有能力。

## 2. Phase R0：可构建基线

```text
R0-01 Qt/MSVC compatibility 决策与修复；
R0-02 fresh binary self-test + scenario/layer/overlay/config smoke 基线；
R0-03 三种窗口尺寸布局证据与组件复用边界。
```

退出标准：新工作区可从干净 build dir 构建 UI，关键 smoke 全部通过。

## 3. Phase R1：配置产品化

```text
R1-01 Profile metadata schema 和默认 Profile 集；
R1-02 SliceSettingsModel；
R1-03 template + override + generated config，并完成 12A 材料/支撑/光油映射；
R1-04 Help metadata 和配置摘要。
```

退出标准：导入模型、选 Profile、修改设置、运行切片全程不要求用户保存原始 fixture JSON。

## 4. Phase R2：预览与诊断收口

```text
R2-01 PreviewWorkspace 与共享 layer state；
R2-02 常驻图例和 RGBWSV 像素探针；
R2-03 DiagnosticsDock；
R2-04 OpenVDB utility/candidate 摘要；
R2-05 smoke、布局验收、用户手册和 REPORT。
```

退出标准：生产层检查、材料叠加和原始调试预览在同一工作区切换；报告、曲线、日志不遮挡主预览。

## 5. Dependencies

```text
12A：提供材料、支撑、光油配置语义；
12B：提供 legacy/OpenVDB role 和 benchmark 边界；
12D：material closure 可后续接入，不能阻断 12C R0/R1；
Qt/MSVC fresh build：R0 硬准入条件。
```

## 6. Rollback

每个阶段保留现有 ScenarioRegistry、ConfigEditorPanel 和三个预览 panel。整合通过 wrapper/state coordinator 完成，失败时可回退到现有独立 tab，不删除原有 panel 和 fixture。
