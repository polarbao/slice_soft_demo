# REPORT_09B_R3_壳层纹理生产准入前诊断策略收口当前状态

> 阶段：09B-R3
> 分支：`spike/09B-R3-shell-production-readiness`
> 状态：已完成生产准入前诊断策略收口实现与本地验证
> 结论：可以进入 09P 的设计与实验集成准备，但不能直接宣称真实 OBJ / 3MF 已满足 production RGBWSV 输出准入。

## 1. 阶段目标

09B-R3 基于 09B-R2 的 OpenVDB 表面壳层纹理实验链路，补齐进入生产化讨论前必须具备的诊断、错误码、纹理边界与内存观测能力。

本阶段只处理以下内容：

1. narrow-phase triangle-triangle self-intersection。
2. 稳定 `ValidationErrorCode` / `WarningCode`。
3. repeat / wrap texture fixture。
4. Windows process peak working set。
5. 真实模型 topology production admission 策略。

本阶段未接入 production `slicer_cli`，未写 production RGBWSV TIFF，未修改 `p0.rgbwsv.2`、RGBWSV 通道顺序、uint8 位深和 `black_is_print` 极性。

## 2. 当前新增工程内容

### 2.1 Narrow-phase 自相交诊断

新增：

- `src/slicer_core/geometry/TriangleIntersectionQuery.h`
- `src/slicer_core/geometry/TriangleIntersectionQuery.cpp`

当前能力：

- 保留 AABB broad-phase candidate。
- 增加 triangle-triangle narrow-phase 检测。
- 区分 `confirmed intersection`、`coplanar overlap`、`touching only`、`AABB false positive`。
- 跳过共享 vertex index 的相邻面，避免把正常相邻拓扑误判为自相交。
- 保留 sampled / max pair check 策略，避免真实大模型诊断无界增长。

`MeshRobustnessDiagnostics` 新增输出：

- `selfIntersectionCandidates`
- `confirmedSelfIntersections`
- `coplanarOverlapPairs`
- `touchingOnlyPairs`
- `selfIntersectionFalsePositiveCandidates`
- `selfIntersectionCheckSampled`

兼容字段 `self_intersection_pairs` 仍保留，但语义调整为 confirmed + coplanar，不再等同于 AABB candidate。

### 2.2 稳定 Issue Code

新增：

- `src/slicer_core/diagnostics/ValidationIssue.h`
- `src/slicer_core/diagnostics/ValidationIssue.cpp`

当前报告同时输出 human-readable message 与稳定 code：

- `MESH_BOUNDARY_EDGES`
- `MESH_NON_MANIFOLD_EDGES`
- `MESH_DUPLICATE_FACES`
- `MESH_OPPOSITE_DUPLICATE_FACES`
- `MESH_LOCAL_WINDING_INCONSISTENCY`
- `MESH_SELF_INTERSECTION_CONFIRMED`
- `MESH_SELF_INTERSECTION_SAMPLED`
- `MESH_THIN_FEATURE_EDGE`
- `MESH_THIN_FEATURE_AREA`
- `TEXTURE_MISSING`
- `TEXTURE_UV_MISSING`
- `TEXTURE_UV_OUT_OF_RANGE`
- `OPENVDB_UNAVAILABLE`
- `OPENVDB_LEVEL_SET_FAILED`

报告新增：

- `issues`
- `warningCodes`
- `errorCodes`

脚本中的负向 fixture 校验已改为依赖稳定 code，而不是依赖错误字符串。

### 2.3 Repeat / Clamp 纹理边界 Fixture

新增：

- `samples/models/openvdb/surface_shell_repeat_texture.obj`
- `samples/models/openvdb/surface_shell_repeat_texture.mtl`
- `samples/configs/openvdb/surface_shell_repeat_texture.json`
- `samples/configs/openvdb/surface_shell_repeat_texture_clamp.json`

该 fixture 使用超出 `[0, 1]` 的 UV，并复用 `samples/models/textured/textures/checker.png`，用于验证 `repeat` 与 `clamp` 采样行为存在可观测差异。

报告新增：

- `uvAddressMode`
- `sampler`
- `repeatedSampledVoxels`

已验证 repeat 模式下：

- `sampledTextureVoxels > 0`
- `uvOutOfRangeVoxels > 0`
- `repeatedSampledVoxels > 0`
- `warningCodes` 包含 `TEXTURE_UV_OUT_OF_RANGE`
- repeat 与 clamp 的 `repeatedSampledVoxels` 不同

### 2.4 Windows 进程峰值内存

新增：

- `src/slicer_core/system/ProcessMemoryStats.h`
- `src/slicer_core/system/ProcessMemoryStats.cpp`

Windows 下使用 `GetProcessMemoryInfo` 读取进程 working set，CMake 在 Windows 链接 `psapi`。非 Windows 返回 unavailable，不作为失败条件。

报告新增：

- `processPeakWorkingSetAvailable`
- `processWorkingSetBytes`
- `processPeakWorkingSetBytes`

该指标与 R2 已有 `peakEstimatedBytes` 不同：

- `peakEstimatedBytes` 是工程结构估算值；
- `processPeakWorkingSetBytes` 是 OS 级进程峰值观测值。

## 3. 真实模型诊断结果

### 3.1 OBJ Golden

报告路径：

- `output/SurfaceShellR2NailObjGolden/reports/surface_shell_texture_report.json`

关键结果：

| 指标 | 值 |
|---|---:|
| triangles | 70262 |
| boundary edges | 0 |
| non-manifold edges | 299 |
| duplicate faces | 0 |
| opposite duplicate faces | 0 |
| local winding inconsistency | 1305 |
| connected components | 2 |
| AABB candidates | 6 |
| confirmed self-intersections | 0 |
| coplanar overlap pairs | 0 |
| false positive candidates | 6 |
| process peak working set | 约 242.96 MB |

稳定 code：

- `errorCodes`: `MESH_LOCAL_WINDING_INCONSISTENCY`
- `warningCodes`: `MESH_SELF_INTERSECTION_SAMPLED`, `MESH_NON_MANIFOLD_EDGES`

结论：

OBJ 真实模型的 R2 自相交候选在 R3 narrow-phase 中被确认是 AABB false positive；当前阻断 production 准入的主要问题是 non-manifold、局部 winding 不一致和多连通组件。

### 3.2 3MF Golden

报告路径：

- `output/SurfaceShellR2Nail3MfGolden/reports/surface_shell_texture_report.json`

关键结果：

| 指标 | 值 |
|---|---:|
| triangles | 75596 |
| boundary edges | 0 |
| non-manifold edges | 10939 |
| duplicate faces | 7190 |
| opposite duplicate faces | 7190 |
| local winding inconsistency | 0 |
| connected components | 3 |
| AABB candidates | 6 |
| confirmed self-intersections | 0 |
| coplanar overlap pairs | 0 |
| false positive candidates | 6 |
| process peak working set | 约 241.15 MB |

稳定 code：

- `errorCodes`: `MESH_DUPLICATE_FACES`, `MESH_OPPOSITE_DUPLICATE_FACES`
- `warningCodes`: `MESH_THIN_FEATURE_AREA`, `MESH_SELF_INTERSECTION_SAMPLED`, `MESH_NON_MANIFOLD_EDGES`

结论：

3MF 真实模型同样没有 confirmed self-intersection，但存在大量 duplicate / opposite duplicate face 与 non-manifold edge。该模型可以继续作为实验链路验证样例，但不能作为 production-safe 输入直接进入 RGBWSV 输出。

## 4. Repeat Fixture 验证结果

报告路径：

- `output/SurfaceShellR3RepeatTexture/reports/surface_shell_texture_report.json`

关键结果：

| 指标 | 值 |
|---|---:|
| triangles | 12 |
| boundary edges | 0 |
| non-manifold edges | 0 |
| duplicate faces | 0 |
| connected components | 1 |
| sampledTextureVoxels | 18188 |
| uvOutOfRangeVoxels | 15364 |
| repeatedSampledVoxels | 15364 |
| process peak working set | 约 120.24 MB |

稳定 code：

- `warningCodes`: `TEXTURE_UV_OUT_OF_RANGE`
- `errorCodes`: 空

结论：

repeat / clamp 的边界行为已有 fixture 覆盖，可用于后续 09P 或 09C 判断纹理采样策略是否被误改。

## 5. Benchmark 结果

Release benchmark 已扩展到 100k triangle fixture：

| Fixture | Triangles | LevelSet ms | BVH ms | Transfer ms | Total ms | Estimated MB | Process Peak MB |
|---|---:|---:|---:|---:|---:|---:|---:|
| bench_1k | 1152 | 61.56 | 1.13 | 2.14 | 68.61 | 2.84 | 84.55 |
| bench_10k | 10400 | 86.64 | 12.96 | 6.46 | 141.90 | 4.69 | 124.86 |
| bench_50k | 51072 | 90.59 | 51.88 | 6.40 | 314.55 | 12.27 | 130.98 |
| bench_100k | 101120 | 115.15 | 145.20 | 7.46 | 592.88 | 21.77 | 119.39 |

说明：

- benchmark 不做 strict time equality，只用于趋势与门槛观察。
- 当前建议 soft gate：10k 必须通过，50k 建议通过，100k 可放入 nightly 或手动性能复测。
- `processPeakWorkingSetBytes` 已可在 Windows 本机输出，后续 CI 是否强制要求需取决于 CI OS。

## 6. 生产准入策略判断

新增策略文档：

- `docs/slicer/DOC_DECISION_09B_R3_真实模型拓扑生产准入策略.md`

当前判断：

| 策略 | 适用范围 | 当前建议 |
|---|---|---|
| `strict_closed` | 生产 RGBWSV 输出准入 | 继续保留为生产默认方向 |
| `repair_then_strict` | 后续拓扑修复实验 | 建议进入后续专门阶段，不在 R3 实现 |
| `warn_and_attempt` | 实验 preview / report / benchmark | 当前真实 OBJ / 3MF 仍使用该模式 |
| `diagnostic_only` | 无法安全切片但需要报告 | 保留 |

真实模型现状：

- R3 证明 R2 的 AABB 自相交候选主要是误报。
- 真实 OBJ / 3MF 的 production blocker 已收敛为 non-manifold、duplicate/opposite duplicate、local winding、多连通组件。
- 这些问题不能通过 R3 的 narrow-phase 自相交诊断自动修复。

因此，09P 可以进入设计与实验集成准备，但 09P-R1 必须保持 feature flag / experimental path；真实模型 production 输出必须先满足 strict admission，或进入专门的 repair-then-strict 阶段。

## 7. 验证命令

本轮已执行并通过：

```powershell
cmake --build build --config Debug --target surface_shell_robustness_unit_tests
.\build\Debug\surface_shell_robustness_unit_tests.exe

$env:VCPKG_ROOT='D:\vcpkg-openvdb'
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09b-r3 -Triplet x64-windows
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3 -RunMatrix
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3 -RunRealModels
.\scripts\run_surface_shell_real_model_tests.ps1 -BuildDir build-openvdb-09b-r3
.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09b-r3
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09b-r3

.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09b-r3-release -Triplet x64-windows
.\scripts\run_surface_shell_benchmarks.ps1 -BuildDir build-openvdb-09b-r3-release -Config Release

cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

OpenVDB configure 仍有 Boost `CMP0167` dev warning，不影响当前构建与测试结果。

## 8. 与生产链路的关系

本阶段未修改：

- production `slicer_cli`
- RGBWSV schema：`p0.rgbwsv.2`
- 通道顺序：R G B W S V
- 输出位深：uint8
- 极性：`black_is_print`
- `TIFFWriter` 生产输出协议
- `RIPReader` 生产读取协议
- `MaterialPolicy` 默认生产链路
- `SupportShapePipeline`

R3 结果只服务于 OpenVDB 表面壳层纹理实验链路的诊断、报告、preview 与 benchmark，不写入 production TIFF 包。

## 9. 是否进入 09P

可以进入 09P 的设计与实验集成准备，但不能直接进入 production 输出。

进入条件说明：

- narrow-phase 自相交误报问题已显著收敛；
- 稳定错误码、repeat fixture、Windows 进程内存观测已具备；
- 真实模型仍不满足 strict production admission；
- 09P 必须保留实验路径隔离和 feature flag。

建议 09P 首阶段只做：

1. 将表面壳层策略以实验策略接入更正式的 pipeline 边界。
2. 保持 `nonProduction=true` 与稳定 code 输出。
3. 禁止把 `warn_and_attempt` 结果写成 production RGBWSV TIFF。

## 10. 是否需要 09B-R4

当前不建议为了 R3 目标追加 09B-R4。

R3 的五类目标已经完成并通过本地验证。后续如果要继续处理真实模型生产准入，应作为新的专题阶段展开，例如：

- topology repair / remeshing；
- duplicate / opposite duplicate face 合并；
- local winding 修复；
- multi-component admission policy；
- 生产级 strict admission gate。

这些内容超出 09B-R3 的收口范围，不应混入本阶段。

## 11. 是否可以并行 09C

可以并行 09C，但必须保持隔离：

- 09C 可以继续做补偿光油几何原型；
- 09C 不应依赖 `warn_and_attempt` 的真实模型结果作为 production-safe 输入；
- 09C 不应修改 R3 已确认的 RGBWSV 生产协议；
- 09C 如需使用壳层纹理结果，应读取 stable issue code 并显式处理 `nonProduction=true`。

## 12. 已知边界

1. 当前没有实现 topology 自动修复。
2. 当前没有实现 production `slicer_cli` 集成。
3. 当前没有实现 production RGBWSV TIFF 输出。
4. 当前没有实现 compensated varnish。
5. 当前没有实现 support clearance。
6. 当前没有做设备或 RIP 工艺联调。
7. 当前本地工作树仍存在 R3 以外的 `.specstory` 和 3MF 样例二进制改动，未作为 R3 任务处理。

## 13. 下一步建议

建议下一阶段按以下顺序推进：

1. 进入 09P，建立 OpenVDB 表面壳层策略到正式 pipeline 的实验接入边界。
2. 保留 feature flag，默认不写 production RGBWSV TIFF。
3. 引入 strict admission gate，把 stable issue code 作为准入条件。
4. 单独规划 topology repair 阶段，解决真实 OBJ / 3MF 的 non-manifold、duplicate、opposite duplicate、local winding 与 multi-component 问题。
5. 允许 09C 补偿光油原型并行推进，但不得绕过 R3 的生产准入结论。
