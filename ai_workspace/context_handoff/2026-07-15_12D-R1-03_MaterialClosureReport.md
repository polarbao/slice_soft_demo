# 12D-R1-03 MaterialClosureReport 交接

> 日期：2026-07-15
> 状态：CURRENT / 12D-R1 IN PROGRESS

## 1. 已完成事实

```text
新增 MaterialClosureReport 独立报告构建模块；
legacy package 输出 reports/material_closure_report.json；
slice_report.totals.materialClosure 输出稳定摘要；
manifest.reports.materialClosure 指向规范路径；
detector 未接入时输出 unavailable/not_available，不伪造 pass；
报告单元测试、sample.stl CLI 生成、RIP Reader 和 CTest 已通过。
```

## 2. 当前报告边界

```text
source=unavailable；
confidence=unavailable；
closureStatus=not_available；
productionAcceptance=not_evaluated；
repair.attempted=false；
worstLayerIndex=null。
```

## 3. 保持不变

```text
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print 不变；
本任务没有实现 TIFF inferred detector、semantic mask exact detector 或 repair；
OpenVDB 默认关闭且不进入生产路径。
```

## 4. 下一原子任务

```text
12D-04 TIFF 反推候选诊断
```

只允许输出 `source=rgbwsv_tiff_inferred`、`confidence=candidate`、`productionAcceptance=not_evaluated`，`closureStatus` 不得为 `pass`，并保持 `repair.attempted=false`。
