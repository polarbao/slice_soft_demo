# REPORT_12B_R0_Benchmark契约与真实Release对比当前状态

生成日期：2026-07-08

## 1. 阶段目标

12B-R0 用于把切片引擎性能比较从 UI 体感、TIFF 写盘、preview 生成和报告写盘中剥离出来，先建立稳定的 core-only benchmark 契约。

本阶段只判断：

```text
1. legacy production path 在 Release 下真实模型核心切片耗时；
2. OpenVDB candidate 当前是否可运行；
3. legacy 与 OpenVDB 是否具备同姿态、同分辨率、同语义的替代比较资格；
4. 后续是否进入 legacy/heightfield 优化或 OpenVDB 替代验证。
```

本阶段不改变 RGBWSV 协议，不改变 12A/12D 材料语义，也不把 OpenVDB 设为默认生产引擎。

## 2. 本阶段新增内容

新增阶段拆分和契约文档：

```text
docs/slice/DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md
docs/slice/DOC/DOC_SCHEMA_12B_CoreBenchmarkReport.md
docs/slice/ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md
docs/codex_task/current/TASKS_12B_R0_Benchmark契约与真实Release对比任务清单.md
```

新增 benchmark 脚本：

```text
scripts/run_12b_core_benchmark.ps1
```

脚本输出 schema：

```text
slicesoft.benchmark.12b.1
```

脚本约束：

```text
writeTiff=false
writePreview=false
writeReports=benchmark_stdout_only
publishPackage=false
```

因此 `coreComputeMs` 不包含 TIFF、preview、manifest、report 等写盘时间。

## 3. Release legacy core-only 基线

本次使用 `model/obj` 下 3 个真实模型派生临时 benchmark 配置。

临时配置目录：

```text
output/benchmarks/12b_r0_configs
```

结果：

| case | coreComputeMs | grid | modelPixels | supportPixels | peakWorkingSetBytes | 报告 |
|---|---:|---|---:|---:|---:|---|
| `nai_you_new` | 4862.987 | 229 x 455 x 498 | 8367116 | 25746243 | 504160256 | `output/benchmarks/12b_r0_nai_you_new_legacy_release.json` |
| `aishen_fudiao` | 6564.161 | 226 x 425 x 573 | 7055867 | 20915992 | 528232448 | `output/benchmarks/12b_r0_aishen_fudiao_legacy_release.json` |
| `meigui_fudiao` | 6409.744 | 227 x 574 x 552 | 9448201 | 32033789 | 664842240 | `output/benchmarks/12b_r0_meigui_fudiao_legacy_release.json` |

结论：

```text
真实 OBJ 模型的 legacy Release core-only 目前是 4.9s 到 6.6s 级别。
该耗时不含生产 TIFF、preview 图片和报告写盘。
后续性能优化应优先分析核心几何采样、层循环、材料合成和支撑填充路径。
```

## 4. OpenVDB candidate 当前状态

Release 可用性报告：

```text
output/benchmarks/12b_r0_openvdb_release_availability.json
```

结果：

```text
legacy available=true
openvdb-candidate available=false
reason=cli_not_found:build-openvdb-09p\Release\slicer_cli.exe
```

Debug candidate smoke：

```text
output/benchmarks/12b_r0_openvdb_candidate_debug.json
```

结果：

```text
openvdb-candidate available=true
coreComputeMs=1274.057
outputSemanticsComparable=false
reason=openvdb-candidate_output_semantics_not_comparable; output semantics are not comparable
```

结论：

```text
当前 OpenVDB candidate 路径可在 Debug 产物上运行；
当前 OpenVDB Release 产物缺失；
当前 OpenVDB candidate 输出语义仍不可与 legacy production path 等价；
因此 OpenVDB 当前不能替代 legacy 生产切片引擎。
```

## 5. same-pose / same-resolution 检查

脚本已新增同姿态和同分辨率检查。

当前 Release 可用性报告结果：

```text
samePose=false
samePoseReason=model_path_differs;scale_differs;rotation_differs;translation_differs;auto_orient_differs
sameResolution=true
sameResolutionReason=same_dpi_and_layer_thickness
performanceComparable=false
```

结论：

```text
当前默认 legacy config 与 OpenVDB candidate config 不是同一个模型和姿态；
即使 OpenVDB Debug candidate 有耗时数据，也不能直接与 legacy Release 真实模型基线比较；
后续若要比较 OpenVDB 替代能力，必须先生成同模型、同 transform、同 dpi、同 layerThickness 的成对配置。
```

## 6. replacement gate 结论

当前 replacement gate：

```text
replacementPass=false
```

原因：

```text
1. OpenVDB Release CLI 缺失；
2. 当前 legacy/openvdb 默认配置 samePose=false；
3. 当前 OpenVDB candidate outputSemanticsComparable=false；
4. 当前 OpenVDB candidate 仍不覆盖 12A/12D 的 production RGBWSV 材料语义。
```

因此，OpenVDB 在当前阶段只能保持：

```text
sdf_utility_candidate
```

不能进入：

```text
production_default_engine
```

## 7. 验证记录

已运行：

```powershell
cmake --build build --config Release --target slicer_cli
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite -Output output\benchmarks\core_benchmark_12b_legacy.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName nai_you_new_legacy_release -LegacyConfig output\benchmarks\12b_r0_configs\nai_you_new.legacy.json -NoImageWrite -Output output\benchmarks\12b_r0_nai_you_new_legacy_release.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName aishen_fudiao_legacy_release -LegacyConfig output\benchmarks\12b_r0_configs\aishen_fudiao.legacy.json -NoImageWrite -Output output\benchmarks\12b_r0_aishen_fudiao_legacy_release.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -CaseName meigui_fudiao_legacy_release -LegacyConfig output\benchmarks\12b_r0_configs\meigui_fudiao.legacy.json -NoImageWrite -Output output\benchmarks\12b_r0_meigui_fudiao_legacy_release.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine all -BuildType Release -CaseName openvdb_candidate_release_availability -NoImageWrite -Output output\benchmarks\12b_r0_openvdb_release_availability.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine openvdb -BuildType Debug -CaseName openvdb_candidate_debug_smoke -NoImageWrite -Output output\benchmarks\12b_r0_openvdb_candidate_debug.json
git diff --check
占位标记扫描：无命中
```

验证说明：

```text
Release 构建命令在工具等待 120s 时超时，但 build/Release/slicer_cli.exe 随后已存在并可执行；
git diff --check 通过，仅存在 CRLF 换行提示；
占位标记扫描无命中。
```

## 8. 构建环境注意事项

本次验证中观察到 VS/MSBuild/CL 进程存在长时间不退出情况。

这不影响已生成的 Release `slicer_cli.exe` 执行 core-only benchmark，但会影响后续自动化稳定性。建议后续单独处理：

```text
1. 检查 VS generator / HostX86-x64 路径是否被错误选中；
2. 优先建立稳定的 Release CLI 构建 preset；
3. OpenVDB ON 轨道也需要补齐 Release 构建产物；
4. benchmark 报告中继续记录 buildType 和 CLI 路径，避免 Debug/Release 混用。
```

## 9. 下一阶段建议

建议进入 12B-R1，但进入前需保持以下边界：

```text
1. legacy 仍是默认 production path；
2. OpenVDB 不作为默认生产切片引擎；
3. 性能优化优先从 legacy/heightfield fast path 入手；
4. OpenVDB 后续只在同姿态、同分辨率、同语义配置下重新评估；
5. 所有耗时比较必须区分 core-only 与 end-to-end。
```

12B-R1 推荐任务：

```text
1. 分析 legacy core-only 热点：层循环、三角面采样、纹理采样、支撑填充、材料合成；
2. 设计 2.5D heightfield fast path 原型；
3. 建立同一真实模型的 legacy baseline before/after 对比；
4. 保持 12A/12D 材料语义不回退。
```

## 10. 阶段结论

12B-R0 已完成 benchmark 契约、真实 Release legacy baseline、OpenVDB candidate 可用性检查、同姿态/同分辨率检查和 replacement gate 结论。

当前最重要结论：

```text
OpenVDB 当前不能替代 legacy production slicer。
真实模型 legacy core-only 已确认为秒级耗时，后续优化方向应进入 legacy/heightfield fast path，而不是直接把 OpenVDB 设为默认。
```
