# TASKS_12B_R0 Benchmark 契约与真实 Release 对比任务清单

> 文档状态：Current Task Plan
> 日期：2026-07-08
> 阶段：12B-R0
> 对应文档：
> - docs/slice/DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md
> - docs/slice/DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md
> - docs/slice/ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md

## 边界

R0 只做 benchmark 契约、脚本、Release 基线和 OpenVDB replacement gate 结论。

R0 不做：

```text
1. 不优化 legacy core；
2. 不实现 heightfield fast path；
3. 不把 OpenVDB 设为默认；
4. 不改变 RGBWSV 协议；
5. 不修改 12A/12D 材料语义。
```

## Task 12B-R0-01 阶段拆分与文档补齐

状态：DONE

内容：

```text
拆分 12B 为 R0/R1/R2；
新增 benchmark schema；
新增 12B 分阶段路线；
更新 12B 总任务入口。
```

验证：

```powershell
git diff --check
rg -n "<未完成标记正则>" docs/slice/DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md docs/slice/DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md docs/slice/ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md docs/codex_task/current/TASKS_12B_R0_Benchmark契约与真实Release对比任务清单.md
```

## Task 12B-R0-02 Benchmark report schema 落地

状态：DONE

内容：

```text
新增 run_12b_core_benchmark.ps1 聚合报告；
输出 schema=slicesoft.benchmark.12b.1；
兼容读取 11B 单引擎 benchmark stdout JSON。
```

验证：

```powershell
.\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite
```

完成记录：

```text
脚本：scripts/run_12b_core_benchmark.ps1
验证输出：output/benchmarks/core_benchmark_12b_legacy.json
schema：slicesoft.benchmark.12b.1
engine：legacy
available：true
writeTiff/writePreview：false/false
```

## Task 12B-R0-03 Release legacy baseline

状态：DONE

内容：

```text
对至少 3 个 model/obj 真实模型运行 Release legacy core-only benchmark。
```

建议模型：

```text
model/obj/nai_you_new
model/obj/aishen_fudiao
model/obj/meigui_fudiao
```

验证：

```text
每个 case 输出 coreComputeMs、grid、layerCount、modelPixels、supportPixels、memory。
```

完成记录：

```text
临时配置目录：output/benchmarks/12b_r0_configs

nai_you_new：
- 输出：output/benchmarks/12b_r0_nai_you_new_legacy_release.json
- coreComputeMs：4862.987
- grid：229 x 455 x 498
- modelPixels：8367116
- supportPixels：25746243
- peakWorkingSetBytes：504160256

aishen_fudiao：
- 输出：output/benchmarks/12b_r0_aishen_fudiao_legacy_release.json
- coreComputeMs：6564.161
- grid：226 x 425 x 573
- modelPixels：7055867
- supportPixels：20915992
- peakWorkingSetBytes：528232448

meigui_fudiao：
- 输出：output/benchmarks/12b_r0_meigui_fudiao_legacy_release.json
- coreComputeMs：6409.744
- grid：227 x 574 x 552
- modelPixels：9448201
- supportPixels：32033789
- peakWorkingSetBytes：664842240
```

## Task 12B-R0-04 OpenVDB candidate baseline

状态：DONE

内容：

```text
在 OpenVDB ON 构建存在时运行 candidate benchmark；
OpenVDB 不可用时输出 available=false 和 reason。
```

验证：

```text
OpenVDB case 不可用不会导致整个 legacy benchmark 失败；
报告记录 failureReasons。
```

完成记录：

```text
Release 可用性报告：output/benchmarks/12b_r0_openvdb_release_availability.json
- legacy：available=true
- openvdb-candidate：available=false
- reason：cli_not_found:build-openvdb-09p\Release\slicer_cli.exe

Debug candidate smoke：output/benchmarks/12b_r0_openvdb_candidate_debug.json
- openvdb-candidate：available=true
- coreComputeMs：1237.124
- outputSemanticsComparable：false
- reason：openvdb-candidate_output_semantics_not_comparable; output semantics are not comparable

结论：
- 当前 OpenVDB ON 只有 Debug 产物可运行；
- 当前 OpenVDB candidate 仍不能进入 replacementPass；
- Release 同口径替代评估需要先补 OpenVDB Release 构建。
```

## Task 12B-R0-05 same-pose / same-resolution 检查

状态：DONE

内容：

```text
确认 legacy/openvdb 使用同模型、同 transform、同 dpi、同 layerThickness。
```

验证：

```text
benchmark JSON 中 samePose=true / sameResolution=true；
如果 false，必须输出原因。
```

完成记录：

```text
脚本已新增 samePose/sameResolution 检查。

验证报告：output/benchmarks/12b_r0_openvdb_release_availability.json
- samePose：false
- samePoseReason：model_path_differs;scale_differs;rotation_differs;translation_differs;auto_orient_differs
- sameResolution：true
- sameResolutionReason：same_dpi_and_layer_thickness
- performanceComparable：false
```

## Task 12B-R0-06 outputSemanticsComparable 原因展开

状态：DONE

内容：

```text
输出不可比较原因，例如 support_semantics_not_implemented、texture_semantics_not_comparable、non_production_admission。
```

验证：

```text
comparison.failureReasons 不是空泛的 "failed"。
```

完成记录：

```text
验证报告：output/benchmarks/12b_r0_openvdb_candidate_debug.json
- openvdb-candidate available：true
- outputSemanticsComparable：false
- failureReasons：
  - openvdb-candidate_output_semantics_not_comparable
  - output semantics are not comparable

验证报告：output/benchmarks/12b_r0_openvdb_release_availability.json
- comparison.failureReasons：
  - openvdb_unavailable
  - cli_not_found:build-openvdb-09p\Release\slicer_cli.exe
  - same_pose_false:model_path_differs;scale_differs;rotation_differs;translation_differs;auto_orient_differs
```

## Task 12B-R0-07 R0 当前状态报告

状态：DONE

内容：

```text
生成 docs/slice/REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md。
```

报告必须包含：

```text
1. 运行了哪些模型；
2. Release legacy core-only 基线；
3. OpenVDB 是否可运行；
4. outputSemanticsComparable 结果；
5. replacementPass 结论；
6. 下一步进入 R1 还是只保留 R0 结论。
```

完成记录：

```text
报告：docs/slice/REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md
结论：
- legacy Release 真实模型 core-only 基线已建立；
- OpenVDB Release CLI 缺失；
- OpenVDB Debug candidate 可运行但 outputSemanticsComparable=false；
- replacementPass=false；
- 下一步建议进入 12B-R1 legacy/heightfield fast path 优化。
```
