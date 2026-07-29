# DOC PREP 13D-03 项目区与诊断 Dock 准备

> 文档状态：PREPARED / WAIT 13D-02 PASS
> 版本：v1.0
> 日期：2026-07-29

## 1. 目标

将仓库路径、输出路径、构建、旧单模型兼容入口和回归工具收入口径明确的可折叠项目区；把右侧
诊断和工艺对比迁入现有底部 `DiagnosticsDock`，删除验证确认后的重复壳层。

## 2. 能力迁移矩阵

```text
配置/输出/对比路径 -> 项目与高级工具；
构建、旧单模型、OpenVDB 诊断、快速回归、RIP -> 项目与高级工具；
报告、材料闭环、曲线、工艺对比、日志 -> DiagnosticsDock；
模型预检 -> ContextInspector，不迁入 DiagnosticsDock；
顶部导入/保存/切片/取消 -> 保持 13D-01 位置。
```

## 3. 安全规则

迁移期间先 reparent/包装，再删除重复容器。所有能力必须能通过 objectName 和 Smoke 找到；不得删除
旧生产兼容入口，不得改变命令参数或输出路径。

## 4. 验收

新增 `workbench-project-diagnostics` Smoke，覆盖项目区折叠、所有高级入口可达、DiagnosticsDock
五类内容可达、右侧不再保留重复诊断页。完成后才解锁 13D-04。
