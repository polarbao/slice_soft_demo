# DEMO_12B_R2 OpenVDB SDF Utility 验证方案

> 文档版本：v0.1
> 文档状态：DEMO / Stage 12B-R2
> 生成日期：2026-07-08

## 1. 验证目标

R2 验证目标不是证明 OpenVDB 可以替代 legacy，而是证明：

```text
1. 默认 OpenVDB OFF 构建不受影响；
2. OpenVDB ON 构建可以作为可选 utility lane；
3. SDF utility 只能输出 report/diagnostic，不写 production RGBWSV；
4. outer varnish / clearance / topology / material closure assist 四类能力有可判断结果；
5. 失败时能解释 blocker，而不是静默退化。
```

## 2. 验证矩阵

| Case | Build | Utility | 输入 | 预期 |
|---|---|---|---|---|
| R2-01 | OFF | legacy guard | `samples/configs/slice_config.json` | legacy benchmark PASS |
| R2-02 | OFF | openvdb unavailable | OpenVDB utility config | report unavailable，不阻断 legacy |
| R2-03 | ON | smoke | generated smoke fixture | activeVoxels > 0 |
| R2-04 | ON | outer varnish shell | closed mesh fixture | shell metrics 输出 |
| R2-05 | ON | clearance | closed mesh fixture | distance metrics 输出 |
| R2-06 | ON/OFF | topology | bad topology fixtures | blockers/warnings 输出 |
| R2-07 | ON | material closure assist | semantic gap fixture | assist metrics 输出，不单独判 production PASS |

## 3. 默认 OFF 验证

命令：

```powershell
cmake --build build --config Debug --target slicer_cli slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite -Output output\benchmarks\core_benchmark_12b_r2_off_guard.json
```

通过标准：

```text
slicer_cli Debug target build PASS；
slicer_debug_ui Debug target build PASS；
UI self-test PASS；
benchmark schema=slicesoft.benchmark.12b.1；
engine=legacy available=true；
outputPolicy.writeTiff=false；
outputPolicy.writePreview=false。
```

## 4. OpenVDB ON Smoke

命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
```

通过标准：

```text
report.openvdb.enabled=true；
report.openvdb.available=true；
report.openvdb.activeVoxels > 0。
```

说明：

```text
如果本机没有 OpenVDB ON build，R2 不能声称 ON lane 通过；
应记录 blocker=openvdb_on_build_missing 或 equivalent。
```

## 5. Utility Report 验证

R2 建议最终输出：

```text
output/benchmarks/12b_r2_openvdb_sdf_utility_report.json
schema=slicesoft.openvdb_sdf_utility.12b_r2.1
```

schema 契约：

```text
docs/slice/DOC/DOC_SCHEMA_12B_R2_OpenVDBSdfUtilityReport.md
```

必须包含：

```text
build.useOpenVdb；
build.openVdbAvailable；
utilities.outerVarnishShell；
utilities.clearance；
utilities.topology；
utilities.materialClosureAssist；
decision.openVdbRole；
decision.productionReplacementAllowed=false。
```

## 6. 通过/失败判定

R2 可通过的最低条件：

```text
1. OFF lane PASS；
2. R2 文档和任务完整；
3. ON lane 如不可用，有明确 blocker；
4. capability matrix 完成；
5. OpenVDB 仍未替代 legacy production path。
```

R2 不能通过的情况：

```text
1. OFF lane 因 OpenVDB 改动失败；
2. R2 改动使 legacy production package 改变；
3. diagnostic path 写 production TIFF；
4. report 无法解释 unavailable / blocked 原因；
5. 未输出 capability matrix 就给出 promote 结论。
```
