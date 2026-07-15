# 12D-R1-02 MaterialClosureConfig 交接

> 日期：2026-07-15
> 状态：CURRENT / 12D-R1 IN PROGRESS

## 1. 已完成事实

```text
SliceConfig 已新增 MaterialClosureConfig 与 MaterialClosureRepairConfig；
legacy 配置和 slicer.config.1 迁移均保留 materialClosure；
默认值、显式诊断配置和负向校验已有单元测试；
repair_then_report 与 repair.enabled=true 在 R3 前会显式拒绝；
experimental_config_unit_tests 已通过。
```

## 2. 冻结默认值

```text
enabled=true；
mode=diagnostic；
connectivity=8；
maxGapPx=1；
repair.enabled=false；
failOnGap=true；
writeGapPreview=false。
```

## 3. 保持不变

```text
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print 不变；
本任务未新增 report writer、gap detector 或 repair；
OpenVDB 默认关闭且不进入生产路径。
```

## 4. 下一原子任务

```text
12D-03 MaterialClosureReport
```

只建立 `reports/material_closure_report.json` 和 `slice_report.totals.materialClosure` 报告骨架，不提前实现 TIFF candidate detector 或 semantic mask repair。
