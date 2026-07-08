# TASKS_12B_R1 Legacy 与 Heightfield 优化任务清单

> 文档状态：Current Task Plan
> 日期：2026-07-08
> 阶段：12B-R1
> 对应文档：
> - docs/slice/REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md
> - docs/slice/DEV/DEV_12B_R1_LegacyHeightfield优化原型设计.md
> - docs/slice/ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md

## 边界

R1 只做 legacy production path 的可观测化、低风险优化和 2.5D heightfield 小原型判断。

R1 不做：

```text
1. 不替换 legacy slicer_cli production path；
2. 不默认启用 OpenVDB；
3. 不修改 RGBWSV 协议；
4. 不改变 12A/12D 材料语义；
5. 不把 Debug benchmark 当性能结论。
```

## Task 12B-R1-01 legacy core hotspot profile

状态：DONE

内容：

```text
扩展 12B benchmark 输出，给 legacy engine 增加 profile 字段；
先允许 coarse profile，无法细分的字段必须为 null；
不得伪造 geometry/material/support 细分耗时。
```

验证：

```powershell
cmake --build build --config Release --target slicer_cli
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite -Output output\benchmarks\core_benchmark_12b_r1_profile.json
```

验收：

```text
schema=slicesoft.benchmark.12b.1
engines[0].profile.available=true
engines[0].profile.profileLevel=coarse
writeTiff=false
writePreview=false
```

完成记录：

```text
构建：
cmake --build build --config Release --target slicer_cli

验证：
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite -Output output\benchmarks\core_benchmark_12b_r1_profile.json

结果：
- schema：slicesoft.benchmark.12b.1
- engine：legacy
- available：true
- profile.available：true
- profile.profileLevel：coarse
- writeTiff/writePreview：false/false

最近一次轻量验证输出：
- 输出：output/benchmarks/core_benchmark_12b_r1_profile.json
- coreComputeMs：以 engines[0].timingsMs.coreCompute 为准
- profile：以 engines[0].profile 为准
```

## Task 12B-R1-02 真实模型 Release profile baseline

状态：DONE

内容：

```text
基于 R0 三个真实模型重复跑 Release profile baseline。
```

模型：

```text
model/obj/nai_you_new
model/obj/aishen_fudiao
model/obj/meigui_fudiao
```

验证：

```text
每个 case 输出 coreComputeMs、profile、grid、modelPixels、supportPixels、memory。
```

完成记录：

```text
nai_you_new：
- 输出：output/benchmarks/12b_r1_nai_you_new_legacy_profile.json
- coreComputeMs：4701.410
- modelLoadMs：554.044
- maskSamplingMs：80.436
- texturePrepareMs：42.698
- supportGenerationMs：2555.337
- layerComposeMs：1302.770
- reportBuildMs：64.456
- reportWriteMs：0.001
- grid：229 x 455 x 498
- modelPixels：8367116
- supportPixels：25746243
- peakWorkingSetBytes：502935552

aishen_fudiao：
- 输出：output/benchmarks/12b_r1_aishen_fudiao_legacy_profile.json
- coreComputeMs：4351.583
- modelLoadMs：369.346
- maskSamplingMs：107.150
- texturePrepareMs：51.363
- supportGenerationMs：2324.204
- layerComposeMs：1269.507
- reportBuildMs：95.097
- reportWriteMs：0.001
- grid：226 x 425 x 573
- modelPixels：7055867
- supportPixels：20915992
- peakWorkingSetBytes：527347712

meigui_fudiao：
- 输出：output/benchmarks/12b_r1_meigui_fudiao_legacy_profile.json
- coreComputeMs：6958.878
- modelLoadMs：1798.448
- maskSamplingMs：113.577
- texturePrepareMs：40.042
- supportGenerationMs：3278.544
- layerComposeMs：1520.933
- reportBuildMs：75.667
- reportWriteMs：0.001
- grid：227 x 574 x 552
- modelPixels：9448201
- supportPixels：32033789
- peakWorkingSetBytes：665665536
```

## Task 12B-R1-03 legacy 优化候选选择

状态：DONE

内容：

```text
根据 R1-01/R1-02 的 profile 判断首个低风险优化候选。
```

候选：

```text
support projection cache
z-bucket / active triangle filter
texture sampling cache
tile/layer parallel
```

验证：

```text
输出候选选择原因和不选择其他候选的原因。
```

完成记录：

```text
选择候选：support projection / support generation path 优化。

选择原因：
- 三个真实模型中 supportGenerationMs 分别为 2555.337 / 2324.204 / 3278.544；
- 该阶段是当前最大热点，明显高于 maskSamplingMs 和 texturePrepareMs；
- 支撑路径可做局部低风险优化，不需要改变 RGBWSV 协议或 OpenVDB 状态。

暂不优先选择：
- z-bucket / active triangle filter：maskSamplingMs 约 80-114ms，不是当前最大热点；
- texture sampling cache：texturePrepareMs 约 40-52ms，不是当前最大热点；
- tile/layer parallel：影响面较大，需先完成单线程局部优化；
- heightfield fast path：属于 R1-05 可行性评估，不应早于当前热点修复。
```

## Task 12B-R1-04 首个低风险优化原型

状态：DONE

内容：

```text
实现一个可开关、可回滚的低风险优化原型。
```

验证：

```text
同一 Release benchmark case 对比 before/after；
输出语义不回退；
coreComputeMs 变化可解释。
```

完成记录：

```text
优化点：
- support.shape.enabled=false 时，不再复制整份 support_generation.support_masks；
- support.shape.enabled=false 时，不再调用 support shape pipeline；
- support.shape.enabled=true 时仍保留原始 support mask 副本，并同步 support type map。

验证：
cmake --build build --config Release --target slicer_cli

before/after：

nai_you_new：
- before：output/benchmarks/12b_r1_nai_you_new_legacy_profile.json
- after：output/benchmarks/12b_r1_nai_you_new_support_shape_fastpath.json
- coreComputeMs：4701.410 -> 3358.141
- improvement：28.57%
- supportGenerationMs：2555.337 -> 1788.073
- sameGrid/modelPixels/supportPixels：true/true/true

aishen_fudiao：
- before：output/benchmarks/12b_r1_aishen_fudiao_legacy_profile.json
- after：output/benchmarks/12b_r1_aishen_fudiao_support_shape_fastpath.json
- coreComputeMs：4351.583 -> 3324.407
- improvement：23.60%
- supportGenerationMs：2324.204 -> 1805.311
- sameGrid/modelPixels/supportPixels：true/true/true

meigui_fudiao：
- before：output/benchmarks/12b_r1_meigui_fudiao_legacy_profile.json
- after：output/benchmarks/12b_r1_meigui_fudiao_support_shape_fastpath.json
- coreComputeMs：6958.878 -> 6158.632
- improvement：11.50%
- supportGenerationMs：3278.544 -> 2801.870
- sameGrid/modelPixels/supportPixels：true/true/true

support shape enabled guard：
- 配置：samples/configs/support/support_shape_smoke.json
- 输出：output/benchmarks/12b_r1_support_shape_smoke_fastpath_guard.json
- result：available=true, profile.available=true, coreComputeMs=8.487
```

## Task 12B-R1-05 2.5D heightfield fast path 可行性评估

状态：DONE

内容：

```text
不替换 production path；
只评估 2.5D admission、topHeight/bottomHeight、mask 差异统计。
```

验证：

```text
输出 heightfield 是否值得进入正式实现；
如果 mask 差异不可接受，保留为研究结论。
```

完成记录：

```text
评估文档：
docs/slice/DOC/DOC_ANALYSIS_12B_R1_2_5DHeightfieldFastPath可行性评估.md

代码证据：
- src/slicer_core/slicer.cpp::sample_relief_heightfield_masks(...)
- src/slicer_core/slicer.cpp::compute_relief_lower_layers(...)
- src/slicer_core/slicer.cpp::compute_relief_column_ranges(...)

结论：
- 当前 relief_heightfield 已经使用 z_min/z_max column range；
- R1-04 后 maskSamplingMs 只占 coreComputeMs 的 1.50% 到 2.27%；
- 当前主要瓶颈仍是 supportGenerationMs 和 layerComposeMs；
- R1 阶段不建议继续实现新的 2.5D heightfield fast path；
- 后续若要正式实现，需要新增 admission、独立 mask candidate、mask diff 和回退机制。

mask diff 状态：
- 未执行独立 mask diff；
- 原因：当前没有第二套独立 fast path，且实现它会扩大 R1 范围；
- 当前可用一致性证据来自 R1-04：before/after 的 grid/modelPixels/supportPixels 均一致。
```

## Task 12B-R1-06 R1 当前状态报告

状态：DONE

内容：

```text
生成 docs/slice/REPORT/REPORT_12B_R1_LegacyHeightfield优化当前状态.md。
```

报告必须包含：

```text
1. profile 字段落地情况；
2. 三个真实模型 baseline；
3. 选择了哪个优化候选；
4. before/after 数据；
5. 2.5D heightfield 是否进入后续实现；
6. 是否建议进入 12B-R2。
```

完成记录：

```text
报告：
docs/slice/REPORT/REPORT_12B_R1_LegacyHeightfield优化当前状态.md

结论：
- profile 字段已落地；
- 三个真实模型 Release profile baseline 已完成；
- 首个优化候选为 support generation path；
- support.shape disabled fast path 已完成 before/after；
- 2.5D heightfield fast path 不建议在 R1 继续实现；
- 可进入 R2，但 R2 应定位为 OpenVDB SDF utility，而不是 production replacement。
```
