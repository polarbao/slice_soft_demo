# DOC_MATRIX_12B_R2 OpenVDB SDF Utility Capability

> 文档状态：Capability Matrix / Stage 12B-R2
> 日期：2026-07-09
> 对应任务：12B-R2-05 Utility Capability Matrix
> Schema 参考：`slicesoft.openvdb_sdf_utility.12b_r2.1`

## 1. 目标

本文档判断 OpenVDB 在 12B-R2 阶段是否适合作为 SDF utility 能力继续推进。

判断对象：

```text
1. OuterVarnishShellOffset；
2. ClearanceDistance；
3. TopologyDiagnostic；
4. MaterialClosureAssist。
```

结论只用于后续 utility 设计，不允许被解释为 OpenVDB 可替代 legacy production slicer。

## 2. 输入证据

### 2.1 R0/R1 结论

```text
12B-R0 已确认 OpenVDB replacementPass=false；
12B-R1 已确认 legacy slicer 仍是当前 production path；
12B-R1 不继续新增独立 2.5D heightfield fast path。
```

### 2.2 R2 文档和 schema

```text
docs/slice/PRD/PRD_12B_R2_OpenVDB_SDFUtility定位.md
docs/slice/DEV/DEV_12B_R2_OpenVDB_SDFUtility评估设计.md
docs/slice/DOC/DOC_SCHEMA_12B_R2_OpenVDBSdfUtilityReport.md
docs/slice/DOC/DOC_AUDIT_12B_R2_OpenVDB_SDFUtility代码盘点.md
```

### 2.3 OFF lane 证据

```text
build/CMakeCache.txt: USE_OPENVDB:BOOL=OFF；
cmake --build build --config Debug --target slicer_cli slicer_debug_ui：PASS；
slicer_debug_ui.exe --self-test：PASS；
run_12b_core_benchmark.ps1 legacy Release NoImageWrite：PASS；
run_09p_cli_experimental_tests.ps1 在 OFF 下输出 OPENVDB_UNAVAILABLE 且不写 production package。
```

### 2.4 ON lane 证据

```text
build-openvdb-09p/CMakeCache.txt: USE_OPENVDB:BOOL=ON；
CMAKE_TOOLCHAIN_FILE=D:\vcpkg-openvdb\scripts\buildsystems\vcpkg.cmake；
run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p：PASS；
```

ON smoke report：

```text
path=output/GeometryKernelOpenVdb/reports/geometry_kernel_report.json
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
该 report 是现有 geometry kernel smoke report；
不是 R2 独立 slicesoft.openvdb_sdf_utility.12b_r2.1 report；
R2 独立 report 原型属于 R2-06。
```

## 3. 决策枚举

本矩阵使用 R2 schema 中的 `promoteDecision`：

| 决策 | 含义 |
|---|---|
| `promote` | 建议进入后续 production-adjacent utility 设计，但不接入 production composer |
| `keep_experimental` | 保留实验能力和验证入口，不进入生产邻近路径 |
| `reject` | 不建议继续投入 |
| `not_evaluated` | 证据不足，暂不能判断 |

R2 对 `promote` 的限制：

```text
promote 只表示推进为辅助 utility；
promote 不表示 OpenVDB 可以替代 production slicer；
promote 不允许写 production RGBWSV TIFF；
promote 不改变 p0.rgbwsv.2。
```

## 4. Capability Matrix 总览

| Utility | 当前证据 | 成熟度 | promoteDecision | 结论 |
|---|---|---|---|---|
| OuterVarnishShellOffset | `OpenVdbSurfaceShell`、geometry smoke shellStats | 中 | `promote` | 可进入 R2-06 最小 utility report 原型 |
| ClearanceDistance | geometry smoke distanceStats、level set 基础 | 低 | `keep_experimental` | 保留实验，不进入 production-adjacent 设计 |
| TopologyDiagnostic | mesh diagnostics、robustness、admission gate | 高 | `promote` | 可推进为诊断/gate utility |
| MaterialClosureAssist | 12D 语义已定义，OpenVDB assist DTO 缺失 | 低 | `keep_experimental` | 先保留研究，不单独判定闭环 PASS |

整体结论：

```text
OpenVDB 在 R2 适合继续作为 SDF utility candidate；
优先推进 OuterVarnishShellOffset 和 TopologyDiagnostic；
ClearanceDistance 与 MaterialClosureAssist 保持 experimental；
不建议在当前阶段替代 legacy production slicer。
```

## 5. OuterVarnishShellOffset

### 5.1 当前能力

代码基础：

```text
src/slicer_core/geometry/OpenVdbSurfaceShell.h
src/slicer_core/geometry/OpenVdbSurfaceShell.cpp
src/slicer_core/geometry/OpenVdbGeometryKernelService.cpp
```

已具备：

```text
1. 基于 OpenVDB level set 的 inside/shell/interior 分类；
2. shell_thickness_mm 参数；
3. inside_mask / shell_mask / interior_mask；
4. inside_voxels / shell_voxels / interior_voxels / outside_voxels；
5. OpenVDB OFF 时稳定返回 OPENVDB_UNAVAILABLE；
6. ON smoke 已输出 shellStats。
```

### 5.2 证据

R2 ON smoke：

```text
shellStats.shellPixels=884
shellStats.interiorPixels=508
shellStats.boundaryPixels=440
shellStats.shellThicknessMm=0.05
openvdb.activeVoxels=27
```

R2 schema 已定义：

```text
utilities.outerVarnishShell.metrics.requestedThicknessMm
utilities.outerVarnishShell.metrics.effectiveThicknessMmMin
utilities.outerVarnishShell.metrics.effectiveThicknessMmMax
utilities.outerVarnishShell.metrics.candidateShellPixels
```

### 5.3 风险

```text
1. 当前 shell mask 是 SDF/voxel 候选，不等同于 12A production outerVarnish V 通道；
2. 需要对齐 12A thicknessMm、pixelPitchUm、allowXYExpansion；
3. 真实 OBJ/3MF 上可能受 topology blocker 影响；
4. voxelSizeMm 会影响厚度误差，不能直接宣称优于 legacy 像素膨胀。
```

### 5.4 决策

```text
promoteDecision=promote
```

推进范围：

```text
进入 R2-06 最小 utility report 原型；
只输出 candidate metrics；
不替换 production V 通道生成；
不写 production TIFF。
```

## 6. ClearanceDistance

### 6.1 当前能力

当前具备：

```text
1. OpenVDB level set 基础；
2. geometry smoke report 中已有 distanceStats；
3. min/max/positive/negative/zero 像素统计可作为初始证据。
```

R2 ON smoke：

```text
distanceStats.minDistanceMm=-0.129999995231628
distanceStats.maxDistanceMm=0.0781024992465973
distanceStats.negativePixels=952
distanceStats.positivePixels=1680
distanceStats.zeroPixels=440
```

### 6.2 缺口

```text
1. 尚无独立 ClearanceDistance utility DTO；
2. 尚无 nearSurfaceDistanceMm / thinRegionCount / minClearanceMm 验收阈值；
3. 尚未定义支撑距离、壳层厚度不足、材料层间距异常的业务判定；
4. 现有 smoke distanceStats 不能直接等价于生产 clearance 诊断。
```

### 6.3 风险

```text
如果直接使用 smoke distanceStats 做工艺判定，可能把几何 SDF 符号距离误解为可打印材料间距；
这会影响支撑、光油、白墨闭环判断。
```

### 6.4 决策

```text
promoteDecision=keep_experimental
```

保留范围：

```text
允许继续作为实验指标输出；
R2-06 可在 report 中标记 status=not_evaluated 或 keep_experimental；
不得作为 production acceptance gate。
```

## 7. TopologyDiagnostic

### 7.1 当前能力

代码基础：

```text
src/slicer_core/diagnostics/ProductionAdmissionPolicy.cpp
src/slicer_core/materials/texture_application/SurfaceShellRealModelReport.cpp
src/slicer_core/geometry/OpenVdbGeometryKernelService.cpp
```

已具备：

```text
1. strict_closed / warn_and_attempt / diagnostic_only / repair_then_strict admission mode；
2. boundary edges、non-manifold、duplicate faces、opposite duplicate、local winding 等 blocker；
3. confirmed self-intersection fail-fast；
4. OPENVDB_UNAVAILABLE 与 OPENVDB_LEVEL_SET_FAILED 稳定 issue code；
5. surface-shell real model report 中已有 meshDiagnostics、robustnessDiagnostics、productionAdmission。
```

### 7.2 证据

R2 代码盘点已确认：

```text
TopologyDiagnostic 成熟度高；
已有 robustness/admission/report；
可输出 R2 capability matrix。
```

Production admission 规则：

```text
MESH_BOUNDARY_EDGES
MESH_NON_MANIFOLD_EDGES
MESH_DUPLICATE_FACES
MESH_OPPOSITE_DUPLICATE_FACES
MESH_LOCAL_WINDING_INCONSISTENCY
OPENVDB_LEVEL_SET_FAILED
OPENVDB_UNAVAILABLE
```

在 strict mode 下仍是 blocker。

### 7.3 风险

```text
1. warn_and_attempt 不能被解释为 production-safe；
2. topology diagnostic 可以给出原因，但不能自动 repair；
3. 当前 R2 不实现 mesh repair / repair_then_strict；
4. 诊断通过不等于 12A/12D 材料语义通过。
```

### 7.4 决策

```text
promoteDecision=promote
```

推进范围：

```text
可推进为 report/gate utility；
可进入 R2-06 最小 utility report；
不得绕过 strict blocker；
不得把 warn_and_attempt 输出写入 production package。
```

## 8. MaterialClosureAssist

### 8.1 当前能力

当前具备：

```text
1. 12D 已定义横截面材料无缝闭环语义；
2. R2 schema 已定义 materialClosureAssist 字段；
3. 可将 OpenVDB 作为 semantic_mask_plus_sdf_assist 的辅助来源。
```

### 8.2 缺口

```text
1. 尚无 OpenVDB material closure assist DTO；
2. 尚无 gapPixelCount / nearSurfaceGapVoxelCount / modelSupportContactGapCount 生成逻辑；
3. 尚无 confidence 计算规则；
4. 尚无与 12D semantic masks 的一致性验证。
```

### 8.3 风险

```text
材料闭环是生产语义问题，不是单纯 SDF 几何问题；
如果 OpenVDB 单独判定 PASS，会绕过 RGBWSV semantic masks 真源；
可能导致模型填充、支撑填充、光油壳层冲突处理被误判。
```

### 8.4 决策

```text
promoteDecision=keep_experimental
```

保留范围：

```text
允许 R2-06 report 中输出 status=not_evaluated 或 keep_experimental；
允许记录 source=semantic_mask_plus_sdf_assist；
不允许单独判定 production material closure PASS；
后续若推进，应在 12D 语义 mask 验证之后再接入。
```

## 9. R2-06 输入建议

R2-06 最小 utility report 原型建议按以下能力输出：

```text
outerVarnishShell：
  available=true
  executed=true when OpenVDB ON smoke or shell service runs
  promoteDecision=promote
  metrics 至少包含 voxelSizeMm、shellThicknessMm、shellPixels/interiorPixels/boundaryPixels 或 candidateShellPixels

clearanceDistance：
  available=true if distanceStats exists
  executed=true only if独立 utility执行
  promoteDecision=keep_experimental
  metrics 可先记录 minDistanceMm/maxDistanceMm/negativePixels/positivePixels/zeroPixels

topologyDiagnostic：
  available=true
  promoteDecision=promote
  metrics 输出 admissionMode、blockers、warnings、strict status

materialClosureAssist：
  available=false 或 not_evaluated
  promoteDecision=keep_experimental
  blockers 包含 material_closure_assist_not_implemented
```

## 10. 后续不做项

R2 后续仍不做：

```text
1. 不替代 legacy slicer_cli production path；
2. 不把 OpenVDB 设为默认 ON；
3. 不写 production RGBWSV TIFF；
4. 不修改 p0.rgbwsv.2；
5. 不修改 RGBWSV 通道顺序；
6. 不修改 uint8 / black_is_print；
7. 不实现 mesh repair；
8. 不把 warn_and_attempt 视为 production-safe。
```

## 11. R2-05 完成判定

R2-05 已完成以下要求：

```text
1. 已评估 outer varnish shell offset；
2. 已评估 clearance / distance diagnostic；
3. 已评估 topology diagnostic；
4. 已评估 material closure gap analysis assist；
5. 每项均给出 promoteDecision；
6. 明确 OpenVDB 仍为 utility candidate，不是 production replacement。
```
