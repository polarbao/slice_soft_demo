# REPORT_12B_R2 OpenVDB SDF Utility 当前状态

> 文档状态：Current Status / Stage 12B-R2
> 生成日期：2026-07-10

## 1. 阶段结论

12B-R2 已完成。OpenVDB 的正式定位为：

```text
optional / disabled-by-default SDF utility candidate
```

OpenVDB 不替代 legacy production slicer，不写生产 RGBWSV TIFF/package，不改变 `p0.rgbwsv.2`、`R G B W S V`、uint8 或 `black_is_print`。

## 2. Current State

12B-R0 已建立 `slicesoft.benchmark.12b.1`、真实模型 Release core-only baseline 和 replacement gate，结论为 `replacementPass=false`。

12B-R1 已建立 coarse profile，确认支撑生成和层合成是主要热点，并完成 `support.shape.enabled=false` 的低风险 fast path。三个真实模型获得 11.50% 到 28.57% 的 core-only 改善；独立 heightfield fast path 不继续推进。

12B-R2 已完成：

```text
1. OpenVDB utility 代码盘点；
2. slicesoft.openvdb_sdf_utility.12b_r2.1 schema；
3. OpenVDB OFF 默认轨道保护；
4. OpenVDB ON smoke；
5. 四类 utility capability matrix；
6. OFF/ON 最小 utility report 原型；
7. 本阶段状态收口。
```

最小原型入口：

```text
src/slicer_core/geometry/OpenVdbSdfUtilityReport.*
apps/openvdb_sdf_utility_probe/main.cpp
tests/unit/openvdb_sdf_utility_report/main.cpp
scripts/run_12b_r2_openvdb_sdf_utility.ps1
```

## 3. Target State

后续 OpenVDB 仅在明确任务中承担局部辅助能力：

```text
OuterVarnishShellOffset：SDF 壳层候选统计与厚度诊断；
TopologyDiagnostic：几何诊断和 admission gate 辅助；
ClearanceDistance：保留实验研究；
MaterialClosureAssist：只能辅助 12D semantic mask，不单独判定生产闭环。
```

生产真源保持：

```text
legacy slicer_cli production path；
12A/12D RGBWSV 材料语义；
ProductionAdmissionPolicy strict blocker；
p0.rgbwsv.2 production package。
```

## 4. Historical State

09P/09B 曾验证 OpenVDB surface shell、真实 OBJ/3MF、纹理转移和 candidate package。这些结果是历史实验能力，不代表 OpenVDB 已成为默认生产引擎。

12B-R0 已通过同姿态、同分辨率、同语义 gate 证明当前 candidate 不具备 production replacement 资格。R2 未推翻该结论。

## 5. Capability Matrix

| Utility | R2 决策 | 当前执行状态 | 后续边界 |
|---|---|---|---|
| OuterVarnishShellOffset | `promote` | ON fixture 输出 shell/inside/interior 统计 | 只推进辅助 utility，不替换 V 通道 |
| ClearanceDistance | `keep_experimental` | 共享 level-set 证据存在，独立 utility 未实现 | 不作为生产验收 gate |
| TopologyDiagnostic | `promote` | 闭合 fixture 输出 topology PASS | 不绕过 strict blocker，不做自动 repair |
| MaterialClosureAssist | `keep_experimental` | `not_evaluated` | 12D semantic masks 仍为生产真源 |

`promote` 只表示可进入后续 production-adjacent utility 设计，不表示 production engine replacement。

## 6. Utility Report 结果

OFF report：

```text
output/benchmarks/12b_r2_openvdb_sdf_utility_off.json
build.useOpenVdb=false
build.openVdbAvailable=false
utilities.*.status=unavailable
decision.productionReplacementAllowed=false
```

ON report：

```text
output/benchmarks/12b_r2_openvdb_sdf_utility_on.json
build.useOpenVdb=true
build.openVdbAvailable=true
build.openVdbVersion=12.0.1
outerVarnishShell.status=pass
outerVarnishShell.metrics.activeVoxels=7214
outerVarnishShell.metrics.candidateShellVoxels=2524
topologyDiagnostic.status=pass
topologyDiagnostic.metrics.sourceTriangles=12
clearanceDistance.status=not_evaluated
materialClosureAssist.status=not_evaluated
decision.productionReplacementAllowed=false
```

两份报告均满足：

```text
writesProductionPackage=false
writesProductionTiff=false
writesPreview=false
modifiesLegacyOutput=false
protocolSchemaTouched=false
```

## 7. 验证记录

已运行并通过：

```powershell
ctest --test-dir build-12b-r2-off -C Debug -R openvdb_sdf_utility_report_unit_tests --output-on-failure
ctest --test-dir build-openvdb-09p -C Debug -R openvdb_sdf_utility_report_unit_tests --output-on-failure
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_r2_openvdb_sdf_utility.ps1 -BuildDir build-12b-r2-off -Config Debug -Output output\benchmarks\12b_r2_openvdb_sdf_utility_off.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_r2_openvdb_sdf_utility.ps1 -BuildDir build-openvdb-09p -Config Debug -Output output\benchmarks\12b_r2_openvdb_sdf_utility_on.json
powershell -ExecutionPolicy Bypass -File .\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
cmake --build build-12b-r2-default --config Debug --target slicer_cli
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

补充结果：

```text
OFF/ON unit test：各 1/1 PASS；
OFF/ON utility report validator：PASS；
OpenVDB smoke：PASS，version=12.0.1，activeVoxels=27；
fresh slicer_cli Debug build：PASS；
现有 UI binary self-test：PASS startup / experimental-report-summary。
```

## 8. 构建环境残留问题

当前 canonical `build` 缓存仍引用已被 Visual Studio 更新替换的 MSVC `14.50.35717`。新建 MSVC `19.51/14.51` 构建可以编译 `slicer_cli`，但 fresh Qt UI build 被 Qt 5.15.2 头文件中的 `stdext::make_checked_array_iterator` 阻断。

该问题不影响 R2 utility OFF/ON 结论，但会阻断 12C UI 的可重复 fresh build，因此必须作为 12C-R0 的第一项准入任务处理。

## 9. Pending Confirmation

R2 不再需要功能性 follow-up 才能进入 12C。以下能力如需继续，应另开明确任务：

```text
1. 真实 OBJ/3MF outer shell utility 验证；
2. clearance 业务阈值和独立 DTO；
3. 与 12D semantic masks 对齐的 closure assist；
4. OpenVDB utility Release 性能和内存预算。
```

这些任务均不能阻断 legacy production path，也不能在无新决策时写 production TIFF。

## 10. 下一阶段建议

进入 12C Qt UI 配置与预览工作台收口。执行顺序建议：

```text
1. 先修复并锁定 Qt/MSVC fresh build lane；
2. 审计已有 ScenarioRegistry、QuickConfigPanel、LayerPreview 和 Overlay 能力；
3. 在现有实现上增量收口 Profile、generated config、统一预览和诊断抽屉；
4. 不重复实现已经存在的 11/12A UI 能力。
```

## 11. 最终判定

```text
12B-R2：COMPLETE
12B：COMPLETE
OpenVDB role：sdf_utility_candidate
Production replacement：NOT ALLOWED
Legacy production path：RETAINED
Next stage：12C readiness / Qt UI workbench closure
```
