# REPORT_12B_R1 Legacy 与 Heightfield 优化当前状态

生成日期：2026-07-08

## 1. 阶段目标

12B-R1 用于在 R0 core-only benchmark 契约基础上，完成 legacy production path 的热点可观测化、真实模型 Release profile baseline、首个低风险优化，并判断 2.5D heightfield fast path 是否值得继续进入正式实现。

本阶段未改变：

```text
1. p0.rgbwsv.2 协议；
2. RGBWSV 通道顺序；
3. uint8 / black_is_print；
4. 12A/12D 材料语义；
5. legacy slicer_cli production path 的默认地位；
6. OpenVDB optional / disabled-by-default 边界。
```

## 2. 新增文档与入口

新增：

```text
docs/slice/DEV/DEV_12B_R1_LegacyHeightfield优化原型设计.md
docs/codex_task/current/TASKS_12B_R1_LegacyHeightfield优化任务清单.md
docs/slice/DOC/DOC_ANALYSIS_12B_R1_2_5DHeightfieldFastPath可行性评估.md
```

更新：

```text
docs/codex_task/README.md
docs/codex_task/current/TASKS_12B_切片引擎性能与OpenVDB替代任务清单.md
docs/slice/README.md
docs/slice/PRD/PRD_12B_切片引擎性能与OpenVDB替代评估.md
docs/slice/DEV/DEV_12B_切片引擎性能与OpenVDB替代评估设计.md
docs/slice/DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md
```

## 3. Profile 字段落地情况

新增 `SliceRunProfile`，作为 legacy slicer 的诊断字段。

位置：

```text
src/slicer_core/slicer.h
src/slicer_core/slicer.cpp
apps/slicer_cli/main.cpp
scripts/run_12b_core_benchmark.ps1
```

输出字段：

```text
profile.available
profile.profileLevel
profile.configLoadMs
profile.modelLoadMs
profile.gridSetupMs
profile.maskSamplingMs
profile.texturePrepareMs
profile.supportGenerationMs
profile.layerComposeMs
profile.reportBuildMs
profile.reportWriteMs
profile.totalMs
```

说明：

```text
profile 是诊断字段，不属于生产 RGBWSV package 协议。
当前 profileLevel=coarse，只拆到稳定的大阶段。
```

## 4. 三真实模型 Release profile baseline

R1-02 运行了 `model/obj` 下三个真实模型。

| case | coreComputeMs | modelLoadMs | maskSamplingMs | texturePrepareMs | supportGenerationMs | layerComposeMs | reportBuildMs | grid |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `nai_you_new` | 4701.410 | 554.044 | 80.436 | 42.698 | 2555.337 | 1302.770 | 64.456 | 229 x 455 x 498 |
| `aishen_fudiao` | 4351.583 | 369.346 | 107.150 | 51.363 | 2324.204 | 1269.507 | 95.097 | 226 x 425 x 573 |
| `meigui_fudiao` | 6958.878 | 1798.448 | 113.577 | 40.042 | 3278.544 | 1520.933 | 75.667 | 227 x 574 x 552 |

数据来源：

```text
output/benchmarks/12b_r1_nai_you_new_legacy_profile.json
output/benchmarks/12b_r1_aishen_fudiao_legacy_profile.json
output/benchmarks/12b_r1_meigui_fudiao_legacy_profile.json
```

## 5. 优化候选选择

选择候选：

```text
support generation path
```

选择原因：

```text
1. 三个真实模型 supportGenerationMs 分别为 2555.337 / 2324.204 / 3278.544；
2. supportGenerationMs 是最大热点，明显高于 maskSamplingMs 与 texturePrepareMs；
3. 支撑路径可局部优化，不需要改变协议和材料语义。
```

暂不优先选择：

```text
z-bucket / active triangle filter：maskSamplingMs 只有 80-114ms；
texture sampling cache：texturePrepareMs 只有 40-52ms；
tile/layer parallel：影响面较大，需后续单独设计；
heightfield fast path：当前 relief_heightfield 已经是 column z_min/z_max 路径，且 mask sampling 不是瓶颈。
```

## 6. 首个低风险优化

优化点：

```text
support.shape.enabled=false 时，不再复制整份 support_generation.support_masks；
support.shape.enabled=false 时，不再调用 support shape pipeline；
support.shape.enabled=true 时仍保留原始 support mask 副本，并同步 support type map。
```

优化文件：

```text
src/slicer_core/slicer.cpp
```

该优化不改变 production 输出语义。

## 7. Before / After 数据

| case | before coreMs | after coreMs | improvement | before supportMs | after supportMs | same grid/model/support |
|---|---:|---:|---:|---:|---:|---|
| `nai_you_new` | 4701.410 | 3358.141 | 28.57% | 2555.337 | 1788.073 | true / true / true |
| `aishen_fudiao` | 4351.583 | 3324.407 | 23.60% | 2324.204 | 1805.311 | true / true / true |
| `meigui_fudiao` | 6958.878 | 6158.632 | 11.50% | 3278.544 | 2801.870 | true / true / true |

After 输出：

```text
output/benchmarks/12b_r1_nai_you_new_support_shape_fastpath.json
output/benchmarks/12b_r1_aishen_fudiao_support_shape_fastpath.json
output/benchmarks/12b_r1_meigui_fudiao_support_shape_fastpath.json
```

额外 guard：

```text
samples/configs/support/support_shape_smoke.json
output/benchmarks/12b_r1_support_shape_smoke_fastpath_guard.json
```

结果：

```text
support_shape_smoke available=true
profile.available=true
coreComputeMs=8.487
```

## 8. Heightfield Fast Path 评估

评估文档：

```text
docs/slice/DOC/DOC_ANALYSIS_12B_R1_2_5DHeightfieldFastPath可行性评估.md
```

当前代码事实：

```text
relief_heightfield 已经通过 z_min/z_max column range 生成 lower_layer / upper_layer；
compute_relief_column_ranges 已经把列范围传给后续材料、支撑和纹理逻辑；
当前路径已经是 heightfield-like 的生产路径。
```

R1-04 后 profile：

| case | coreMs | maskSamplingMs | mask 占比 | supportGenerationMs | support 占比 | layerComposeMs | compose 占比 |
|---|---:|---:|---:|---:|---:|---:|---:|
| `nai_you_new` | 3358.141 | 75.644 | 2.25% | 1788.073 | 53.25% | 882.743 | 26.29% |
| `aishen_fudiao` | 3324.407 | 75.524 | 2.27% | 1805.311 | 54.30% | 924.601 | 27.81% |
| `meigui_fudiao` | 6158.632 | 92.076 | 1.50% | 2801.870 | 45.50% | 1595.716 | 25.91% |

结论：

```text
R1 阶段不建议继续实现新的 2.5D heightfield fast path。
```

原因：

```text
1. 当前 relief_heightfield 已经使用 column z_min/z_max；
2. maskSamplingMs 占比只有 1.50% 到 2.27%；
3. 新 fast path 需要 admission、独立 mask candidate、mask diff 和回退机制；
4. 当前更大的优化空间仍在 supportGenerationMs 与 layerComposeMs；
5. 若未来面向非 relief 模型推广 fast path，应另开正式 admission 阶段。
```

## 9. 验证记录

已运行：

```powershell
cmake --build build --config Release --target slicer_cli
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite -Output output\benchmarks\core_benchmark_12b_r1_profile.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName nai_you_new_legacy_release_r1_profile -LegacyConfig output\benchmarks\12b_r0_configs\nai_you_new.legacy.json -NoImageWrite -Output output\benchmarks\12b_r1_nai_you_new_legacy_profile.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName aishen_fudiao_legacy_release_r1_profile -LegacyConfig output\benchmarks\12b_r0_configs\aishen_fudiao.legacy.json -NoImageWrite -Output output\benchmarks\12b_r1_aishen_fudiao_legacy_profile.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName meigui_fudiao_legacy_release_r1_profile -LegacyConfig output\benchmarks\12b_r0_configs\meigui_fudiao.legacy.json -NoImageWrite -Output output\benchmarks\12b_r1_meigui_fudiao_legacy_profile.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName nai_you_new_legacy_release_r1_support_shape_fastpath -LegacyConfig output\benchmarks\12b_r0_configs\nai_you_new.legacy.json -NoImageWrite -Output output\benchmarks\12b_r1_nai_you_new_support_shape_fastpath.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName aishen_fudiao_legacy_release_r1_support_shape_fastpath -LegacyConfig output\benchmarks\12b_r0_configs\aishen_fudiao.legacy.json -NoImageWrite -Output output\benchmarks\12b_r1_aishen_fudiao_support_shape_fastpath.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName meigui_fudiao_legacy_release_r1_support_shape_fastpath -LegacyConfig output\benchmarks\12b_r0_configs\meigui_fudiao.legacy.json -NoImageWrite -Output output\benchmarks\12b_r1_meigui_fudiao_support_shape_fastpath.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName support_shape_smoke_r1_fastpath_guard -LegacyConfig samples\configs\support\support_shape_smoke.json -NoImageWrite -Output output\benchmarks\12b_r1_support_shape_smoke_fastpath_guard.json
git diff --check
占位标记扫描：无命中
```

说明：

```text
git diff --check 通过，仅存在 CRLF 提示。
```

## 10. 是否建议进入 12B-R2

可以进入 12B-R2，但目的应明确为：

```text
OpenVDB hybrid / SDF utility 定位。
```

不应把 R2 解读为：

```text
OpenVDB 替代 legacy production slicer。
```

建议：

```text
1. 性能优化主线继续关注 support generation 和 layer compose；
2. R2 仅判断 OpenVDB 是否适合作为 outer varnish shell / clearance / topology diagnostics / material closure gap analysis 工具；
3. OpenVDB 不应默认启用，不应替代 legacy production path；
4. 如要继续性能优化，可另开 R1-R2 或 12B-R1-followup，聚焦支撑生成和 layer compose。
```

## 11. 阶段结论

12B-R1 已完成：

```text
1. legacy coarse profile；
2. 三个真实模型 Release profile baseline；
3. 首个低风险支撑路径优化；
4. before/after 数据验证；
5. support shape enabled guard；
6. 2.5D heightfield fast path 可行性评估。
```

关键结论：

```text
当前最有效的 R1 优化来自 support generation path；
新的 2.5D heightfield fast path 不适合作为 R1 后续重点；
OpenVDB 仍不应替代 legacy，R2 只能讨论 SDF utility 定位。
```
