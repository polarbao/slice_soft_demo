# REPORT_12B_R2 OpenVDB SDF Utility 启动状态

> 文档状态：Stage Entry Report / 12B-R2
> 生成日期：2026-07-08

## 1. 当前状态

12B-R0 已完成：

```text
benchmark report schema = slicesoft.benchmark.12b.1；
真实模型 Release legacy core-only baseline；
OpenVDB replacement gate；
结论：OpenVDB 当前不能替代 legacy production slicer。
```

12B-R1 已完成：

```text
legacy SliceRunProfile coarse profile；
三个真实模型 Release profile baseline；
support.shape disabled fast path；
before/after benchmark；
2.5D heightfield fast path 可行性评估；
结论：R1 不继续实现新的 heightfield fast path。
```

当前工作树进入 12B-R2 的前置条件已满足，但 R2 必须按 SDF utility 定位执行。

## 2. R2 准入判断

| 条件 | 状态 | 证据 |
|---|---|---|
| R0 有真实模型 Release baseline | PASS | `REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md` |
| OpenVDB 不能直接替代 legacy | PASS | `replacementPass=false` |
| R1 已完成 legacy 优化和评估 | PASS | `REPORT_12B_R1_LegacyHeightfield优化当前状态.md` |
| 12A/12D 材料语义已定义 | PASS | 12A/12D PRD/DEV/DOC_DECISION |
| 需要 SDF utility 类能力 | PASS | outer varnish / clearance / topology / closure assist 均有需求入口 |
| OpenVDB OFF 必须保持默认 | PASS / 持续约束 | `.agents` 与 CMake `USE_OPENVDB=OFF` |

结论：

```text
可以开启 12B-R2。
```

但 R2 的含义是：

```text
OpenVDB hybrid / SDF utility 定位与验证。
```

不是：

```text
OpenVDB production slicer replacement。
```

## 3. 本次补齐文档

新增：

```text
docs/slice/PRD/PRD_12B_R2_OpenVDB_SDFUtility定位.md
docs/slice/DEV/DEV_12B_R2_OpenVDB_SDFUtility评估设计.md
docs/slice/DEMO/DEMO_12B_R2_OpenVDB_SDFUtility验证方案.md
docs/codex_task/current/TASKS_12B_R2_OpenVDB_SDFUtility定位任务清单.md
docs/codex_task/current/CODEX_PROMPT_12B_R2_OpenVDB_SDFUtility执行指令.md
docs/slice/REPORT/REPORT_12B_R2_OpenVDB_SDFUtility启动状态.md
docs/slice/DOC/DOC_SCHEMA_12B_R2_OpenVDBSdfUtilityReport.md
docs/slice/DOC/DOC_AUDIT_12B_任务覆盖与R2缺口审查.md
```

更新建议入口：

```text
docs/slice/README.md
docs/codex_task/README.md
docs/codex_task/current/TASKS_12B_切片引擎性能与OpenVDB替代任务清单.md
docs/slice/PRD/PRD_12B_切片引擎性能与OpenVDB替代评估.md
docs/slice/DEV/DEV_12B_切片引擎性能与OpenVDB替代评估设计.md
docs/slice/DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md
docs/slice/ROADMAP/ROADMAP_12B_切片引擎性能分阶段路线.md
```

## 4. R2 当前执行入口

```text
docs/codex_task/current/TASKS_12B_R2_OpenVDB_SDFUtility定位任务清单.md
```

当前任务状态：

```text
12B-R2-00 文档准入与阶段启动：DONE
12B-R2-01 当前 OpenVDB utility 代码盘点：DONE
12B-R2-02 Utility Report Schema：DONE
12B-R2-03 OpenVDB OFF 默认轨道保护：DONE
12B-R2-04 OpenVDB ON Smoke 与可用性报告：DONE
12B-R2-05 Utility Capability Matrix：DONE
12B-R2-06 最小 Utility Report 原型：PENDING
```

## 5. 下一步建议

下一步建议执行：

```text
Task 12B-R2-06 最小 Utility Report 原型
```

原因：

```text
R2 已确认当前 OpenVDB 代码成熟度；
并已固化 slicesoft.openvdb_sdf_utility.12b_r2.1；
并已验证 USE_OPENVDB=OFF 默认构建、UI self-test、legacy benchmark 和现有 unavailable diagnostic 不受 R2 影响；
并已复测 OpenVDB ON smoke 可用；
并已完成 outer varnish、clearance、topology、material closure assist 四类 utility capability matrix；
下一步应在不写 production TIFF 的前提下生成最小 slicesoft.openvdb_sdf_utility.12b_r2.1 utility report 原型。
```

## 6. 风险与边界

R2 最大风险：

```text
把 OpenVDB utility 误当成 production slicer replacement。
```

防护：

```text
1. productionReplacementAllowed=false；
2. USE_OPENVDB=OFF 默认构建必须保持通过；
3. OpenVDB ON lane 失败只记录 blocker，不影响 legacy；
4. 不写 production RGBWSV TIFF；
5. 不改变 12A/12D 材料语义。
```

## 7. R2-03 OFF Guard 验证记录

验证时间：

```text
2026-07-08
```

验证结果：

| 项目 | 结果 | 证据 |
|---|---|---|
| 默认构建 OpenVDB 状态 | PASS | `build/CMakeCache.txt` 中 `USE_OPENVDB:BOOL=OFF` |
| Debug build | PASS | `cmake --build build --config Debug --target slicer_cli slicer_debug_ui` |
| UI self-test | PASS | `PASS startup`、`PASS experimental-report-summary` |
| Legacy benchmark | PASS | `output/benchmarks/core_benchmark_12b_r2_off_guard.json` |
| OFF unavailable diagnostic | PASS / existing 09P evidence | `scripts/run_09p_cli_experimental_tests.ps1 -BuildDir build -Config Debug` |

benchmark 摘要：

```text
schema=slicesoft.benchmark.12b.1
buildType=Release
engine=legacy
available=true
outputPolicy.writeTiff=false
outputPolicy.writePreview=false
coreComputeMs=18.072
replacementPass=false
```

说明：

```text
本次验证证明默认 OFF 轨道不受 R2 文档和 schema 补齐影响；
现有 09P experimental diagnostic 在 USE_OPENVDB=OFF 下仍输出 OPENVDB_UNAVAILABLE，且不写 production package；
R2 独立 slicesoft.openvdb_sdf_utility.12b_r2.1 report 原型仍属于 R2-06，不在 R2-03 中提前实现。
```

## 8. R2-04 ON Smoke 验证记录

验证时间：

```text
2026-07-08
```

验证命令：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
```

验证结果：

| 项目 | 结果 | 证据 |
|---|---|---|
| OpenVDB ON build 配置 | PASS | `build-openvdb-09p/CMakeCache.txt` 中 `USE_OPENVDB:BOOL=ON` |
| OpenVDB toolchain | PASS | `CMAKE_TOOLCHAIN_FILE=D:\vcpkg-openvdb\scripts\buildsystems\vcpkg.cmake` |
| geometry_kernel_demo build | PASS | 脚本增量构建成功 |
| OpenVDB smoke | PASS | `OpenVDB smoke passed.` |
| smoke report | PASS | `output/GeometryKernelOpenVdb/reports/geometry_kernel_report.json` |

smoke report 摘要：

```text
schema=p0.geometry_kernel_report.1
caseName=openvdb-smoke
openvdb.enabled=true
openvdb.available=true
openvdb.version=12.0.1
openvdb.activeVoxels=27
shellStats.shellPixels=884
shellStats.interiorPixels=508
shellStats.boundaryPixels=440
distanceStats.minDistanceMm=-0.129999995231628
distanceStats.maxDistanceMm=0.0781024992465973
```

说明：

```text
本次验证证明当前机器上 OpenVDB ON lane 可构建、可运行；
输出的是现有 geometry kernel smoke report；
R2 独立 slicesoft.openvdb_sdf_utility.12b_r2.1 report 原型仍属于 R2-06。
```

## 9. R2-05 Capability Matrix 结论

输出文档：

```text
docs/slice/DOC/DOC_MATRIX_12B_R2_OpenVDBSdfUtilityCapability.md
```

矩阵结论：

| Utility | promoteDecision | R2 处理 |
|---|---|---|
| OuterVarnishShellOffset | `promote` | 进入 R2-06 最小 utility report 原型，不替换 production V 通道 |
| ClearanceDistance | `keep_experimental` | 保留实验指标，不作为 production acceptance gate |
| TopologyDiagnostic | `promote` | 推进为 report/gate utility，不绕过 strict blocker |
| MaterialClosureAssist | `keep_experimental` | 只做辅助研究，12D semantic masks 仍为生产真源 |

说明：

```text
promote 只表示推进为辅助 utility；
promote 不表示 OpenVDB 可以替代 production slicer；
promote 不允许写 production RGBWSV TIFF。
```
