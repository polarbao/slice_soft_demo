# REPORT_16C-01 分项 Telemetry 收口当前状态

> 状态：**COMPLETE**
> 日期：2026-08-12

## 1. 实现范围

新增 unique model import 与 visible instance 两级诊断 telemetry。Multi-model production
现在保留 `load_model_report`、resource hash、单实例 core/compose/total 和实例 grid 身份；
Legacy adapter 将无文件输出 producer 的真实 `SliceRunProfile` 返回给场景服务。

Worker `timing` 新增 `imports[]` 和 `instances[]`。当前没有独立计时边界的 texture decode
与 surface preview 明确输出 `null`，没有用 0 或按比例拆分伪造。

## 2. 保持不变

```text
Stage 14 SPI v1 与导出函数不变；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print 不变；
Legacy/Global 默认与采样候选不变；
材料、支撑、TIFF Writer 和 Package manifest 不变；
既有作业级 timing 字段保持兼容。
```

## 3. 验证状态

| 验收项 | 结果 |
|---|---|
| Debug `multi_model_production_service_unit_tests` 构建 | PASS |
| Debug `slicer_worker` 构建 | PASS |
| Debug `stage14d08_r2_slice_executor_tests` 构建 | PASS |
| 场景生产与 Worker 定向 CTest | 2/2 PASS |
| parse/hash 真实非负计时 | PASS |
| 未独立执行 texture/preview 输出 null | PASS |
| visible instance core/compose/grid 完整 | PASS |
| PackBits RGBWSV strict fixture | PASS（由既有场景生产单测覆盖） |

## 4. 后续

`16C-02` 仍依赖 `16A-05` 候选矩阵冻结；本卡不提前生成新的 Release 性能基线。
