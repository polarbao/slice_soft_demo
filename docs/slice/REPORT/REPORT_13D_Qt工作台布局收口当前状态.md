# REPORT 13D Qt 工作台布局收口当前状态

> 状态：13D-01..04 COMPLETE / PASS
> 日期：2026-07-29

## 1. 阶段目标

在不修改切片业务、生产协议和输出包的前提下，把调试工具形态的 Qt 界面收敛为“准备模型 -> 切片
-> 检查结果”的稳定工作台。

## 2. 完成内容

| 原子任务 | 状态 | 结果 |
|---|---|---|
| 13D-01 | COMPLETE | 固定顶部导入、保存、模式/Profile、切片、取消和状态摘要 |
| 13D-02 | COMPLETE | 右侧收敛为场景、变换、排版、切片设置和预检五页 Context Inspector |
| 13D-03 | COMPLETE | 项目/兼容工具移入默认收起左 Dock；所有报告和诊断统一到底部 Dock |
| 13D-04 | COMPLETE | 版本化布局恢复、安全默认值、响应式、快捷键、用户说明和阶段证据收口 |

## 3. 布局状态

```text
QSettings schema = ui.layout.version=13d.1；
保存 geometry、QMainWindow dock state、main splitter sizes、检查器页和检查器可见性；
旧 schema、损坏数据或屏幕外 geometry 自动删除并回退 1440x900 安全默认布局；
UI Smoke 和 self-test 禁用真实用户设置读写；
布局状态不进入 slice JSON、Effective Config 或 Package。
```

## 4. 最终信息架构

```text
顶部：SceneActionBar；
中央：模型 / 预览 / 配置；
右侧：唯一 ContextInspector；
左侧：默认收起的 ProjectToolsDock；
底部：默认收起的 DiagnosticsDock；
视图菜单：项目与高级工具 / 上下文检查器 / 诊断区域。
```

## 5. 验证结果

```text
Debug slicer_debug_ui build：PASS；
workbench-job-action-bar：PASS；
workbench-context-inspector：PASS；
workbench-project-diagnostics：PASS；
workbench-layout-restore：PASS；
workbench-1280x720：PASS（100%/150% 字体尺度）；
diagnostics-collapse：PASS；
workspace-layout-sizes：PASS；
multi-model-list / scene-grid-layout：PASS；
scene-batch-import-three / scene-slice-current：PASS；
UI self-test：PASS；
scripts/run_ci_quick.ps1：PASS；
git diff --check：PASS。
```

## 6. 边界与下一步

13D 不修改 `SceneDocument`、切片算法、RGBWSV TIFF、Package、RIP、材料策略或生产准入。Stage 13
功能开发与工作台收口已经完成；设备 buildVolume/轴向和 22 实例预算仍是独立 production 输入。

根据跨阶段执行顺序，下一任务为 `12E-09A-03` 中文参数控件与状态区。
