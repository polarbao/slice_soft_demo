# REPORT 12E-09B Qt 双模式生产入口当前状态

> 状态：COMPLETE / GO
> 日期：2026-07-24
> 生产默认：Legacy
> Global：显式 opt-in，仅开放已准入 Profile

## 1. 阶段结论

12E-09B 已完成 Legacy / Global Surface Shell 两种产品模式的 Qt 生产入口闭环：

```text
Legacy 保持默认；
Global 必须显式选择；
OpenVDB 不作为第三种产品模式；
两种模式共用模型预检、生产准入、ProcessRunner 和 p0.rgbwsv.2 输出合同；
失败、模式不一致、旧包、跨 session 包或 fallback 包均不会加载；
当前 session 的 preview/report/timing 与 package identity 同源；
Global 高时间和内存开销在 UI 中明确披露。
```

本阶段不改变 `R G B W S V`、`uint8`、`black_is_print`，不放宽 strict topology，
也不把 Global 改为默认。

## 2. 已完成任务

| 任务 | 结果 |
|---|---|
| 09B-01 | 建立 `ProductionModeCatalog`、Global Profile 能力目录和 fail-closed UI DTO |
| 09B-02 | 建立原子 session Effective Config、capability lock、requested/effective 审计 |
| 09B-03 | 增加中文 Legacy/Global 选择器、Global Profile 选择器、能力锁定和资源提示 |
| 09B-04 | 接通一键切片双模式路由、共享 preflight/process、session identity 和 no-fallback |
| 09B-05 | 校验当前 package identity、manifest/slice report 模式、生产输出、同源 preview/report 和实测资源 |
| 09B-06 | 完成 Debug/Release、UI smoke、Quick CI 和 xiao_ma/yecan 六 case Release/TIFF/RIP 矩阵 |

## 3. 09B-05 生产结果闭环

新增 `ProductionPackageResultValidator`，只接受本次
`ProductionSliceRunSession` 返回的精确 request：

```text
sessionId；
effective config path；
package path；
requested product mode；
requested Global Profile。
```

加载前必须同时满足：

```text
package 路径与当前 session 完全一致；
manifest schema = p0.rgbwsv.2；
manifest requestedPipelineMode/effectivePipelineMode 与请求一致；
slice_report 与 manifest 模式一致；
productionOutputWritten = true；
fallbackApplied = false；
manifest source.configPath 与本次 Effective Config 一致；
preview 和 reports 全部位于当前 package 内；
slice_report.json 与 preview_report.json 存在。
```

任一条件失败，UI 会显示生产结果校验失败，不加载旧预览、旧报告或其他 session 的输出包。

Legacy manifest/slice report 已补齐以下审计字段，因而两种模式使用同一结果校验口径：

```text
requestedPipelineMode = legacy；
effectivePipelineMode = legacy；
productionAcceptance = legacy_production；
productionOutputWritten；
fallbackApplied = false；
source.engine = legacy。
```

## 4. UI 当前行为

配置页“生产切片模式”区域显示：

```text
切片模式；
全局 Profile；
能力范围；
准入状态；
阻断信息；
资源提示；
本次 requested/effective 模式与 session；
TIFF 是否写入、fallback 状态和 package；
本次总耗时与峰值工作集。
```

`slicer_cli` 的 `SLICE_TIMING` 增加实际进程内存字段：

```text
memoryAvailable；
workingSetBytes；
peakWorkingSetBytes。
```

平台无法采集时显示“未提供”，不会使用固定倍数或历史预算冒充本次实测值。

## 5. 真实模型 Release 矩阵

验证入口：

```powershell
.\scripts\run_12e_09b_06_production_ui_gate.ps1 -BuildDir build -Config Release -SkipBuild
```

证据：

```text
output/benchmarks/12e_09b_06_production_ui/release_matrix_summary.json
output/benchmarks/12e_09b_06_production_ui/qt_production_entry_summary.json
```

| 模型族 | 模式/Profile | 总耗时 ms | 峰值内存 MiB | 结果 |
|---|---|---:|---:|---|
| xiao_ma | Legacy | 10113.748 | 685.2 | TIFF/preview/report/RIP PASS |
| yecan | Legacy | 15847.282 | 868.7 | TIFF/preview/report/RIP PASS |
| xiao_ma | Global restricted | 48248.640 | 5610.9 | TIFF/preview/report/RIP PASS |
| yecan | Global restricted | 64868.492 | 7470.0 | TIFF/preview/report/RIP PASS |
| xiao_ma | Global material parity | 56753.335 | 5715.4 | TIFF/preview/report/RIP PASS |
| yecan | Global material parity | 93864.852 | 7595.5 | TIFF/preview/report/RIP PASS |

本次矩阵中，Global 总耗时约为 Legacy 的 `4.09x-5.92x`，峰值内存约为
`8.19x-8.74x`。这些值是本机本次实测证据，不是产品 SLA。

结论保持：

```text
Legacy production：GO / 默认；
Global restricted：GO / 显式 opt-in；
Global material parity：GO / 显式 opt-in；
Global 默认替换 Legacy：NO-GO；
fallbackApplied：false。
```

## 6. 验证结果

2026-07-24 实际执行：

```text
Debug 全量 build：PASS；
Debug CTest：54/54 PASS；
Release 全量 build：PASS；
production_package_result_unit_tests：PASS；
production_slice_route_process_tests：PASS；
production-mode-selector UI smoke：PASS（1280x720、1440x900、1920x1080）；
slice-progress-timing UI smoke：PASS；
model-preflight-one-click-gate：PASS；
self-test：PASS；
六 case Release/TIFF/preview/report/RIP strict：PASS；
scripts/run_ci_quick.ps1：PASS。
```

## 7. 当前边界与剩余工作

```text
复杂浮雕 aishen/meigui/titian 仍是 0/3 strict coverage gap；
Global 仍是高开销显式候选，不替代 Legacy 默认；
09A-02..06 是独立 Diagnostic UI 支线，不因 09B 完成而自动完成；
09C X/Y DPI 已完成准备，09B-06 完成后可按独立授权从 09C-01 开始；
12E-10 仍等待 09A-05、09C 和最终矩阵依赖。
```

## 8. 回滚边界

若后续生产入口回归，可保持：

```text
默认模式回到 Legacy；
隐藏 Global 选择但不删除核心模式字段；
保留 ProductionPackageResultValidator 和 no-fallback；
不得回滚固定 RGBWSV 协议或 strict topology fail-fast。
```
