# DOC_PREP 12E-10C Release 性能与内存准备

> 文档状态：COMPLETE / PASS
> 版本：v1.1
> 日期：2026-08-03
> 前置：`12E-10B COMPLETE`

## 1. 任务目标

12E-10C 只形成当前参考机上的 Legacy 与 Global Surface Shell Release 性能、分段耗时和峰值内存结论，
不修改生产算法、Profile 默认值或 RGBWSV 协议。

固定输出：

```text
output/benchmarks/12e_10c/release_performance_matrix.json
docs/slice/REPORT/REPORT_12E_10C_Release性能与内存当前状态.md
```

## 2. 固定比较口径

### 2.1 参考资产

```text
xiao_ma：model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj
yecan：model/obj/yecan/3.obj
```

两个模型已经在 12E-10B 的 Legacy/Global 生产矩阵中通过。`aishen/meigui/titian` 继续作为 strict blocked
披露项，不进入性能比较，避免把预检失败时间混入生产引擎时间。3MF checker 只承担 10B 格式控制，不参与
10C 默认引擎性能结论。

### 2.2 参数与输出策略

每个模型固定三组请求宽度：

| widthPoint | requestedWidthMm | Legacy 表达 | Global 表达 |
|---|---:|---|---|
| `minimum` | 0.40 | `topSurfaceLayers=2` | `partial_shell widthMm=0.40` |
| `intermediate` | 0.80 | `topSurfaceLayers=4` | `partial_shell widthMm=0.80` |
| `all_texture` | 全纹理 | `solid_volume_from_top_surface` | `mode=all_texture` |

共同设置：

```text
dpiX/dpiY = 600/600
layerThicknessMm = 0.20
storageMode = stripped
tiffCompression = none
preview.enabled = false
modelFill = white
support = disabled
surfaceVarnish/outerVarnish = disabled
materialClosure = enabled, failOnGap=true
autoOrient = enabled, maxHeightMm=9
```

Legacy 的“顶面层数”与 Global 的“三维表面距离”几何语义并不相同。10C 只保证请求物理宽度、输入、栅格和
输出工艺一致，并在结果中同时记录 effective width；不得把两种算法描述成逐体素等价。

两个引擎可按各自生产语义生成不同的首末空层或边界层，因此 `layerCount` 必须分别记录但不要求相等；
报告同时给出 `coreMs` 和 `corePerLayerMs`，总耗时比较必须披露层数差异。

## 3. 测量合同

```text
Release 构建；
warm-up 1 次，不计入统计；
measurement 至少 3 次；
同一轮内按奇偶轮换 Legacy/Global 顺序；
每次运行使用独立 package 目录；
每次生产包均执行 RIP strict；
结果保留全部原始样本，并以中位数作为主结论；
峰值工作集同时记录 median 和 max；
referenceMachine 只代表当前机器，不外推设备 SLA。
```

## 4. 分段定义

```text
coreMs = SLICE_TIMING.layerComputeMs
composeMs = SLICE_TIMING.layerComposeMs
tiffSaveMs = SLICE_TIMING.tiffWriteMs
previewReportSaveMs = previewWriteMs + reportBuildMs + reportWriteMs
totalMs = SLICE_TIMING.totalMs
wallClockMs = 外部 runner 进程墙钟
peakWorkingSetBytes = 外部 runner 采样值与进程 PeakWorkingSet64 的较大值
```

`coreMs` 和 `peakWorkingSetBytes` 是引擎比较主指标；`tiffSaveMs`、`previewReportSaveMs` 和完整 `totalMs`
必须披露，但不得用 I/O 差异替代核心算法结论。

## 5. 完成 Gate

```text
12 个 case（2 模型 x 3 宽度 x 2 引擎）每个至少 3 个有效样本；
全部 package 为 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
RIP strict 36/36 PASS（按默认 3 次）；
requestedPipelineMode == effectivePipelineMode；
fallbackApplied == false；
每一对 Legacy/Global 的模型 hash、DPI、层厚、存储、压缩、材料策略一致；
输出 schema 通过 runner 自检；
报告明确 Legacy 默认和 Global 候选结论。
```

## 6. 风险与边界

```text
Windows 文件扫描可能造成 package staging rename 瞬时失败；发生时应记录失败，不静默吞掉；
单机三次中位数是工程基线，不是硬件打印 SLA；
Global effective width 可能受模型厚度上限钳制，必须诚实记录；
不修改 12G-TCWS；
不修改 p0.rgbwsv.2、通道顺序、位深、极性；
不把 OpenVDB 设为默认；
不以 10C 自动切换默认引擎。
```

## 7. 实际完成结果

```text
计量样本：36/36 PASS；
RIP strict：36/36 PASS；
fallback：0；
失败：0；
Global/Legacy core：1.826x..2.562x；
Global/Legacy total：2.244x..3.161x；
Global/Legacy peak memory：3.079x..4.304x；
决策：PASS_LEGACY_DEFAULT_GLOBAL_EXPLICIT_CANDIDATE。
```

详细结果见 `../REPORT/REPORT_12E_10C_Release性能与内存当前状态.md`。
