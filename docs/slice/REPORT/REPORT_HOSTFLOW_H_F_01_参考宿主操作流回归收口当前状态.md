# REPORT_HOSTFLOW_H_F_01 参考宿主操作流回归收口当前状态

> 状态：COMPLETE
> 日期：2026-08-10
> 范围：`apps/slicer_ui_host_sim`，不占阶段编号

## 1. 问题与结论

本次修复新封装宿主与旧版日常操作流之间的四项回归：

| 问题 | 根因 | 当前结果 |
|---|---|---|
| 导入对话框不在模型目录 | 新宿主固定使用 `QDir::homePath()` | 优先复用本次会话目录；首次启动优先 `<exe>/model`，开发构建回退 `<cwd>/model` |
| 工艺配置说明过短 | Profile 目录只展示名称、安全级别和能力名 | 展示适用场景、默认工艺、输出合同、使用限制、Profile ID 和能力要求 |
| 导入后未自动排版 | 批量原子导入完成后未调用 `applyGridLayout` | 场景超过一个实例时，按面板当前 11×2、列/行 10 mm 参数执行一次权威排版 |
| 切片错误不易发现 | 错误已进入作业面板，但失败时没有切换到该页 | 提交失败、取消和 Worker 失败自动打开“切片作业”，显示中文错误码、说明与详细信息 |

自动排版失败不会撤销已经成功导入的模型；UI 会保留场景并提示操作者到“变换与排版”页调整参数后重试。该行为与旧版批量导入后的容错边界一致。

## 2. 实现边界

- 未修改 `PM_SPI_VERSION=1`、11 个 `pm_*` 导出和 15 项能力。
- 未修改 RGBWSV、`p0.rgbwsv.2`、TIFF 位深、极性或通道顺序。
- 未读取内部 `slicer_scenarios.json`，Profile 仍由宿主目录与 ABI 能力求交。
- 未修改 `apps/slicer_debug_ui`；旧版仅作为交互行为参照。
- 场景保存/加载仍归 PrintApp，本次只记住当前进程内最近一次模型目录。

## 3. 验证结果

Debug 与 Release 均执行以下 7 项定向测试并通过：

```text
hostflow_hb01_model_import
hostflow_he02_batch_import
hostflow_hb01_import_ui_smoke
hostflow_hb03_transform_layout
hostflow_hb04_profile_catalog
hostflow_hb04_profile_ui_smoke
hostflow_hb06_job_ui_smoke
```

覆盖内容包括运行目录/工作目录模型路径策略、默认排版参数、Profile 详细说明与 Tooltip、中文切片错误详情。实际文件对话框和系统窗口交互仍需操作者在发布 Runtime 中做一次人工确认。
