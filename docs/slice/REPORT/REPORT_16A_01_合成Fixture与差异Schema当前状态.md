# REPORT_16A-01 合成 Fixture 与差异 Schema 当前状态

> 状态：**COMPLETE**
> 日期：2026-08-12

## 1. 完成内容

新增六组与真实资产解耦的合成 Fixture，并冻结 `slicesoft.stage16.layer_channel_diff.1` 工程差异报告合同。测试会根据半开层区间和 1/4、2/4 固定覆盖阈值重新计算期望，不只是验证 JSON 能被解析。

## 2. 文件

```text
tests/stage16/fixtures/geometry_sampling_fixtures.json
tests/stage16/contracts/layer_channel_diff_schema.json
tests/stage16/fixtures/layer_channel_diff_example.json
tests/stage16/GeometrySamplingFixtureTests.cpp
```

## 3. 验收结果

| 验收项 | 结果 |
|---|---|
| 平底首末层半开边界 | PASS |
| 上升/下降斜楔层像素序列 | PASS |
| 圆弧边 1/4 与 2/4 候选差异 | PASS |
| 亚像素薄片候选分层 | PASS |
| 多区间负向 fail-closed 合同 | PASS |
| R/G/B/W/S/V 顺序 | PASS |
| layer/channel/component/dimension schema | PASS |
| Debug 定向 CTest | 1/1 PASS |

## 4. 当前边界

本任务没有实现或启用 Layer Slab、2x2 生产采样，也没有改变任何 Package、TIFF、材料、支撑或 RIP 行为。下一张依赖卡是 `16A-02 GeometryOccupancyPolicy 和 Provider 合同`，其状态仍需按任务清单单独推进。
