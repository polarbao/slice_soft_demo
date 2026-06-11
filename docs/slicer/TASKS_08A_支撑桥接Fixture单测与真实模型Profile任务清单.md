# TASKS_08A_支撑桥接Fixture单测与真实模型Profile任务清单

> 文档版本：v0.1
> 适用阶段：08A
> 建议提交目录：`docs/slicer/`

## Milestone 08A-0：阅读确认

- [x] 阅读 `REPORT_08_支撑形态与工艺优化当前状态.md`
- [x] 确认不修改 p0.rgbwsv.2
- [x] 确认不实现 surface_shell_texture / compensated_varnish
- [x] 确认不引入 OpenVDB / 设备通信

## Milestone 08A-1：Bridge Fixture

- [x] 新增 `samples/configs/support/support_bridge_gap_smoke.json`
- [x] 必要时新增 `samples/models/support/support_bridge_gap.obj`
- [x] 使 `support_shape_report.bridgedGaps.length > 0`
- [x] 使 `addedSupportPixels > 0`
- [x] 输出 `output/SupportBridgeGapSmoke`

## Milestone 08A-2：Unit Test Target

- [x] 新增 `support_shape_unit_tests`
- [x] 测试 component analysis
- [x] 测试 small component filter
- [x] 测试 dilation preserve model priority
- [x] 测试 horizontal bridge
- [x] 测试 vertical bridge
- [x] 测试 maxAddedSupportRatio rollback

## Milestone 08A-3：Pipeline/Support Wrapper

- [x] 新增或扩展 support shape facade
- [x] 减少 `slicer.cpp` 直接依赖 optimizer 细节
- [x] 不重写 support generation
- [x] quick regression 通过

## Milestone 08A-4：Schema / Golden / CI

- [x] `run_support_shape_tests.ps1` 接入 bridge fixture
- [x] `run_schema_tests.ps1` 检查 bridge report
- [x] `run_golden_tests.ps1` 新增 bridge case
- [x] `run_ci_quick.ps1` 执行 unit test 和 bridge fixture

## Milestone 08A-5：真实模型 Profile

- [x] 新增至少一个 `three_mf_real_01_support_shape.json`
- [x] 不替换原真实模型配置
- [x] package 可生成
- [x] rip_reader_test PASS
- [x] support_shape_report 有统计

## Milestone 08A-6：状态报告

- [x] 生成 `REPORT_08A_支撑桥接Fixture单测与真实模型Profile当前状态.md`
- [x] 说明是否可以进入 09
