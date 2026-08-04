# REPORT 12E-10C Release 性能与内存当前状态

> 状态：COMPLETE / PASS
> 日期：2026-08-03
> 证据范围：当前参考机 Release 工程基线，不是设备 SLA

## 1. 阶段结论

12E-10C 已完成同模型、同宽度请求、同 DPI、同层厚和同输出策略下的 Legacy 与
Global Surface Shell 性能矩阵。固定结论为：

```text
Legacy：继续作为默认生产模式；
Global Surface Shell：继续作为显式候选模式；
禁止 silent fallback；
本矩阵不授权自动切换默认引擎。
```

固定输出为：

```text
output/benchmarks/12e_10c/release_performance_matrix.json
schema = slicesoft.stage12e.release_performance.1
```

## 2. 测量合同

```text
参考机：LAP-COM / Windows NT 10.0.26200.0 / 18 logical processors；
构建：Release；
模型：xiao_ma、yecan；
宽度：minimum 0.40 mm、intermediate 0.80 mm、all_texture；
DPI：600 x 600；
层厚：0.20 mm；
TIFF：stripped / none；
Preview：关闭；
材料：white fill，support/varnish 关闭；
每 case：1 次预热 + 3 次计量；
顺序：每轮交替 Legacy/Global 先后顺序；
生产包：每个样本均执行 RIP strict。
```

Legacy 的顶面层深度与 Global 的三维表面距离共用相同请求宽度，但两者不是逐体素等价语义。本报告
比较相同产品请求下的工程成本，不把它描述为完全相同的几何算法输入。

## 3. 汇总结果

| 模型/宽度 | 层数 Legacy/Global | Core 比值 G/L | 每层 Core 比值 G/L | Total 比值 G/L | 峰值内存比值 G/L |
|---|---:|---:|---:|---:|---:|
| xiao_ma / minimum | 28 / 30 | 2.002 | 1.869 | 2.318 | 3.099 |
| xiao_ma / intermediate | 28 / 30 | 2.110 | 1.969 | 2.244 | 3.087 |
| xiao_ma / all_texture | 28 / 30 | 2.562 | 2.391 | 2.606 | 3.079 |
| yecan / minimum | 29 / 31 | 2.288 | 2.141 | 2.942 | 4.292 |
| yecan / intermediate | 29 / 31 | 2.162 | 2.022 | 3.161 | 4.304 |
| yecan / all_texture | 29 / 31 | 1.826 | 1.708 | 2.738 | 4.303 |

范围结论：

```text
Global / Legacy core：1.826x .. 2.562x；
Global / Legacy total：2.244x .. 3.161x；
Global / Legacy peak working set：3.079x .. 4.304x。
```

## 4. 分段中位数

单位为毫秒；内存为进程峰值工作集 MiB。

| Case | 模式 | Core | Compose | TIFF | Preview/Report | Total | 峰值 MiB |
|---|---|---:|---:|---:|---:|---:|---:|
| xiao_ma minimum | Legacy | 172.445 | 309.361 | 128.443 | 92.097 | 574.459 | 83.42 |
| xiao_ma minimum | Global | 345.280 | 279.564 | 172.268 | 31.228 | 1331.748 | 258.48 |
| xiao_ma intermediate | Legacy | 172.217 | 330.509 | 147.778 | 122.100 | 609.654 | 83.59 |
| xiao_ma intermediate | Global | 363.382 | 295.375 | 123.403 | 31.455 | 1367.955 | 258.77 |
| xiao_ma all_texture | Legacy | 143.249 | 315.703 | 169.392 | 72.220 | 537.061 | 83.05 |
| xiao_ma all_texture | Global | 367.005 | 270.492 | 213.790 | 32.979 | 1399.576 | 258.29 |
| yecan minimum | Legacy | 178.164 | 328.979 | 132.893 | 80.939 | 563.534 | 78.20 |
| yecan minimum | Global | 407.696 | 348.017 | 144.768 | 37.086 | 1658.040 | 336.15 |
| yecan intermediate | Legacy | 183.382 | 293.387 | 100.029 | 73.249 | 512.764 | 78.14 |
| yecan intermediate | Global | 396.433 | 348.205 | 146.685 | 35.895 | 1620.956 | 336.18 |
| yecan all_texture | Legacy | 218.878 | 370.036 | 120.715 | 77.318 | 582.492 | 78.19 |
| yecan all_texture | Global | 399.589 | 343.395 | 134.015 | 36.259 | 1594.640 | 336.14 |

Global 比 Legacy 多输出 2 层。矩阵因此同时记录总 Core 和每层 Core；即使按层归一化，Global 仍为
Legacy 的 1.708x..2.391x。本差异来自当前边界层生成语义，未被隐藏或强行裁平。

## 5. 本次工程补齐

```text
新增可重复执行的 12E-10C Release runner；
新增版本化性能矩阵 schema；
Global production profile 补齐 detailed phase timings；
分离 layer compute、material compose、TIFF、preview/report 和 package publish；
Windows package 原子发布对瞬时 rename 占用增加有界重试；
重试耗尽仍 fail-closed，不吞掉真实文件系统错误。
```

## 6. 实际验证

实际执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_12e_10c_release_performance.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -Iterations 3 `
  -WarmupIterations 1 `
  -SkipBuild
```

结果：

```text
material_closure_report_unit_tests：PASS；
rgbwsv_production_package_writer_unit_tests：PASS；
global_surface_shell_production_pipeline_unit_tests：PASS；
计量样本：36/36 PASS；
RIP strict：36/36 PASS；
fallback：0；
失败：0；
Global detailed timing 零值样本：0。
```

## 7. 剩余风险

```text
本结果只代表当前参考机，不是跨设备性能承诺；
复杂浮雕 aishen/meigui/titian 仍被 strict topology 阻断，未进入性能比较；
Global 当前仍明显更慢且峰值内存更高；
设备 buildVolume、坐标轴和 22 实例生产预算仍是 Stage 13 外部输入；
12G-TCWS 继续冻结；
OpenVDB 继续 optional/OFF。
```

12E-10C Gate 已满足，12E-10D 可以执行文档与阶段封口。
