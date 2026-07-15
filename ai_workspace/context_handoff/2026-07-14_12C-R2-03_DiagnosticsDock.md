# 12C-R2-03 DiagnosticsDock 交接

## 1. 已完成

```text
新增底部 DiagnosticsDock，默认隐藏且只允许 BottomDockWidgetArea；
视图菜单提供“诊断区域” toggleViewAction；
ReportPanel、ChannelChartPanel、LogPanel 由 dock 唯一拥有；
中央 mainWorkspaceTabs 只保留“预览”和“配置”；
输出包加载、warningsChanged 和 ProcessRunner 日志连接保持原有行为；
diagnostics-collapse 验证默认折叠、三页签、唯一实例、展开收起和共享层不变。
```

## 2. 未改变边界

```text
未修改 slicer_core、切片算法或生产 package；
未修改 p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print；
未解析 OpenVDB utility report；
未实现 12D 材料闭环判断；
未完成 R2-05 多尺寸最终截图验收。
```

## 3. R2-04 准备度审查

R2-04 的产品目标明确：只读显示 `slicesoft.openvdb_sdf_utility.12b_r2.1`，始终保持 `productionReplacementAllowed=false`，不得把 utility PASS 翻译为生产切片 PASS。

当前仍需在编码前冻结：

```text
utility report 的真实 on/off fixture 和字段必选集合；
报告位于 package/reports 还是 output/benchmarks 独立路径时的选择入口；
摘要放入 DiagnosticsDock 新页签还是复用 ReportPanel；
available=false、promoteDecision、blockerCodes 和 legacy fallback 的中文映射；
缺报告、schema 错误和 productionReplacementAllowed 非 false 的失败显示；
openvdb-utility-summary smoke 的固定 fixture 与断言。
```

结论：R2-04 的方向和前置组件已准备，但原子实施契约尚未完善。下一步应先完成 R2-04 准入准备，不在 R2-03 中提前解析 utility report。
