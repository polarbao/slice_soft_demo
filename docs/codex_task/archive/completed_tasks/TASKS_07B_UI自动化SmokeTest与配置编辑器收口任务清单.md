# TASKS_07B_UI自动化SmokeTest与配置编辑器收口任务清单

> 文档版本：v0.1  
> 建议目录：`docs/slicer/`

## Milestone 07B-0：阅读确认

- [x] 阅读 `REPORT_07A_Qt参数编辑与Profile可视化当前实现状态.md`
- [x] 确认不修改 slicer_core 输出协议
- [x] 确认不做生产 UI / 设备通信

## Milestone 07B-1：UiSmokeTestRunner

- [x] startup
- [x] load-package
- [x] save-as-config
- [x] chart-load
- [x] overlay-load
- [x] compare-profiles
- [x] --yes 自动确认

## Milestone 07B-2：main.cpp 参数

- [x] --ui-smoke-test
- [x] --case
- [x] --config
- [x] --package
- [x] --package-a
- [x] --package-b
- [x] --output
- [x] --yes

## Milestone 07B-3：Save 覆盖确认

- [x] Save 覆盖当前文件前确认
- [x] Save As 覆盖已有文件前确认
- [x] smoke-test mode 自动确认
- [x] 非法 JSON 仍禁止保存

## Milestone 07B-4：ConfigDiff

- [x] ConfigDiffModel
- [x] ConfigDiffPanel
- [x] path / old / new
- [x] editor 变更后刷新
- [x] Save / Revert 后刷新

## Milestone 07B-5：PreviewReportIndex

- [x] schema = p0.preview_report.1
- [x] files[].path/channel/layerIndex/kind
- [x] 兼容 files / generated / previewFiles
- [x] PreviewOverlayPanel 接入
- [x] fallback 到目录扫描

## Milestone 07B-6：验证与报告

- [x] 执行所有 UI smoke test
- [x] 执行 quick regression
- [x] 生成 `REPORT_07B_UI自动化SmokeTest与配置编辑器收口当前实现状态.md`
