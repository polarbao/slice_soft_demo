# TASKS_08_支撑形态与工艺优化任务清单

> 文档版本：v0.1  
> 阶段：08  
> 建议目录：`docs/slicer/`

## Milestone 08-0：阅读确认

- [x] 阅读 `REPORT_R2_配置报告测试CI工程化当前状态.md`
- [x] 确认不修改 p0.rgbwsv.2
- [x] 确认不实现 surface_shell_texture / compensated_varnish
- [x] 确认不引入 OpenVDB / 设备通信

## Milestone 08-1：SupportShapePolicy

- [x] 新增 `SupportShapePolicy.*`
- [x] 解析 `support.shape.enabled`
- [x] 解析 `minComponentAreaPx`
- [x] 解析 `xyDilationPx`
- [x] 解析 `closingRadiusPx`
- [x] 解析 `bridgeGapPx`
- [x] 兼容已有 support 字段

## Milestone 08-2：Component Analysis

- [x] 新增 `SupportComponentAnalysis.*`
- [x] 支持 4/8 connectivity
- [x] 输出 component count
- [x] 输出 largest area
- [x] 输出 small/tiny component count
- [x] 输出 bbox

## Milestone 08-3：Shape Optimizer

- [x] 新增 `SupportShapeOptimizer.*`
- [x] 支持小组件过滤
- [x] 支持 dilation
- [x] 支持简化 closing
- [x] 支持短 gap bridge
- [x] 不覆盖 model pixels
- [x] 限制 maxAddedSupportRatio

## Milestone 08-4：Report

- [x] 新增 `SupportShapeReport.*`
- [x] 输出 `reports/support_shape_report.json`
- [x] schema = `p0.support_shape_report.1`
- [x] 写入 policy / pre / post / filtered / bridged / warnings

## Milestone 08-5：Sample 与 Tests

- [x] 新增 `samples/configs/support/support_shape_smoke.json`
- [x] 新增 `scripts/run_support_shape_tests.ps1`
- [x] 接入 `run_schema_tests.ps1`
- [x] 接入 `run_golden_tests.ps1`
- [x] 接入 `run_ci_quick.ps1`

## Milestone 08-6：UI / Preview

- [x] 确认 RGB+S overlay 可展示变化
- [x] 必要时让 UI report viewer 识别 support_shape_report
- [x] 不新增生产级 UI

## Milestone 08-7：最终验证

- [x] `cmake --build build --config Debug`
- [x] `slicer_cli --config support_shape_smoke.json`
- [x] `rip_reader_test --package output/SupportShapeSmoke`
- [x] `run_ci_quick.ps1`
- [x] 生成 `REPORT_08_支撑形态与工艺优化当前状态.md`
