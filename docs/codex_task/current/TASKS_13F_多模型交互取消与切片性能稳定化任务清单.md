# TASKS 13F 多模型交互、取消与切片性能稳定化任务清单

> 状态：R0 COMPLETE / R1 IN PROGRESS
> 日期：2026-07-30

## 13F-R0

| Task | 内容 | 状态 | 验证 |
|---|---|---|---|
| 13F-R0-01 | 批量导入增量展示，不冻结已完成实例 | COMPLETE | `scene-batch-import-three` |
| 13F-R0-02 | surface preview alpha 命中与冗余深拷贝消除 | COMPLETE | `multi-model-list`、`scene_document_unit_tests` |
| 13F-R0-03 | Cancelling 状态与 Windows 强制退出兜底 | COMPLETE | `production_slice_route_process_tests`、`scene-slice-cancel` |
| 13F-R0-04 | 爱神与 Reality 静态复杂度分析 | COMPLETE | 文件统计与代码路径审查，不运行 Reality 切片 |

## 13F-R1

| Task | 内容 | 状态 | Gate |
|---|---|---|---|
| 13F-R1-01 | 单实例核心切片与合成分项计时 | READY | 报告含 modelId/grid/layer/coreSliceMs |
| 13F-R1-02 | 导入 parse/texture/preview/hash 分项计时 | READY | 五模型导入耗时可定位 |
| 13F-R1-03 | 自适应 surface preview 精度 | PENDING | 显示质量不回归，峰值等待降低 |
| 13F-R1-04 | 平移重复实例 raster 复用原型 | PENDING | 与基线逐像素一致 |
| 13F-R1-05 | 有内存预算的有限并行评估 | PENDING | 不超过显式内存上限 |
| 13F-R1-06 | Reality 单模型 Z 基准修正与同配置 Release 基准 | COMPLETE | 当前纵向 `303x614x184`、核心 `2366.3419 ms`、完整写包 `6516.322 ms`、RIP PASS |

## 固定约束

```text
Reality 模型每次最多验证一个；
核心性能基准关闭 TIFF、preview、report 文件保存；
不得改变 p0.rgbwsv.2、uint8、black_is_print 与 R G B W S V 顺序；
不得用并行掩盖错误配置或模型准入失败；
不得把取消视为进程已经退出。
```

## 当前结论

```text
13F 尚未全部完成；
R0 四项均完成；
R1-06 已完成并解除 Reality 单模型约一分钟的异常耗时；
R1-01..05 仍需继续，重点是分实例计时、导入计时、自适应预览、平移复用和有限并行。
```
