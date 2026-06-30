# REPORT_09B_R2_壳层纹理鲁棒性性能与多材质策略当前状态

> 阶段：09B-R2
> 分支：`spike/09B-R2-shell-robustness-performance`
> 状态：实验链路已完成一轮实现与验证
> 结论：可以作为 OpenVDB 表面壳层纹理鲁棒性/性能基线；尚不能进入生产 RGBWSV 输出或 09P 生产化。

## 1. 阶段目标

09B-R2 在 09B / 09B-R1 的真实 OBJ/3MF 表面壳层纹理基础上，补齐以下能力：

1. 真实指甲 OBJ / 3MF golden 样例验证。
2. scale-aware tolerance，避免固定 epsilon 在不同尺寸模型上失效。
3. 拓扑鲁棒性诊断，包括连通组件、重复面、反向重复面、局部 winding、薄特征、基础自相交候选统计。
4. seam 与 multi-material 策略收口，保证最近三角命中后按该三角的 UV / material 采样，不跨 seam 平均。
5. BVH / texture cache / memory / benchmark 指标输出。
6. 生成 checklist、golden expected JSON、回归脚本和阶段报告。

本阶段仍严格限定为实验 OpenVDB 路径，不改生产 `slicer_cli` RGBWSV TIFF 链路。

## 2. 当前新增工程内容

### 2.1 Core Geometry

- `src/slicer_core/geometry/MeshScaleTolerance.h/.cpp`
  - 输出 `positionEpsilonMm`、`areaEpsilonMm2`、`tieEpsilonMm`、`selfIntersectionEpsilonMm`。
- `src/slicer_core/geometry/MeshRobustnessDiagnostics.h/.cpp`
  - 输出 connected components、duplicate faces、opposite duplicates、local winding inconsistency、zero-volume components、min edge / area / aspect ratio。
  - 自相交当前为 AABB broad-phase candidate / sampled 统计，不是完整 narrow-phase 三角相交。
- `src/slicer_core/geometry/NearestTriangleQuery.h/.cpp`
  - 增加 stable nearest hit tie-break。
  - 增加 query count、visited nodes、tested triangles、max visited nodes、node count、estimated bytes。
- `src/slicer_core/geometry/OpenVdbLevelSetBuilder.h/.cpp`
  - 增加 OpenVDB grid memory bytes。

### 2.2 Surface Shell Texture

- `SurfaceTextureTransfer`
  - 接入 stable tie-break。
  - 输出 texture cache hit/miss/bytes、loaded texture count、per-material/per-texture sampled voxels。
- `SurfaceShellRealModelPrototype`
  - 接入 topology diagnostics、scale-aware tolerance、memory baseline、nonProduction 标识。
  - 支持 `warn_and_attempt` 下继续处理真实不完美模型。
- `SurfaceShellRealModelReport`
  - report schema 维持 `p0.surface_shell_texture_report.2`。
  - 增加 epsilon、robustnessDiagnostics、nearestQueryStats、memory、nonProduction。
- `SurfaceShellBenchmarkReport`
  - 新增 benchmark schema：`p0.surface_shell_benchmark_report.1`。

### 2.3 App / Test / Script

- `surface_shell_robustness_demo`
  - 生成 texture report、benchmark report、preview。
- `surface_shell_robustness_unit_tests`
  - 覆盖 tolerance、重复面、局部反向、自相交候选、tie-break、BVH stats。
- `scripts/run_surface_shell_robustness_tests.ps1`
  - 支持基础 fixture、matrix、真实模型验证。
- `scripts/run_surface_shell_benchmarks.ps1`
  - Release 下生成 1k / 10k / 50k benchmark fixture 并输出报告。

## 3. Fixture 与 Golden

| Fixture | 类型 | 用途 |
|---|---|---|
| `samples/models/textured/textured_relief.obj` | 真实 OBJ | 指甲浮雕纹理 golden |
| `samples/models/3mf/03.3mf` | 真实 3MF | 指甲多纹理 3MF golden |
| `samples/models/openvdb/surface_shell_multimaterial_seam.obj` | 生成 OBJ | multi-material / multi-texture seam |
| `samples/models/openvdb/surface_shell_thin_wall.obj` | 生成 OBJ | thin wall |
| `samples/models/openvdb/surface_shell_duplicate_face.obj` | 生成 OBJ | duplicate face negative |
| `samples/models/openvdb/surface_shell_local_reversed.obj` | 生成 OBJ | local reversed face negative |
| `samples/models/openvdb/surface_shell_self_intersect.obj` | 生成 OBJ | self-intersection candidate |

Golden expected 文件：

- `tests/golden/expected/surface_shell_real_model_r2.json`

Fixture hash、来源和授权记录已写入：

- `docs/slicer/SURFACE_SHELL_R2_ROBUSTNESS_PERFORMANCE_CHECKLIST.md`

## 4. 真实模型验证结果

| Case | Triangles | Materials | Textures | Inside | Shell | Interior | Result |
|---|---:|---:|---:|---:|---:|---:|---|
| OBJ golden | 70262 | 1 | 1 | 338713 | 112436 | 226277 | PASS，`nonProduction=true` |
| 3MF golden | 75596 | 3 | 3 | 373358 | 116234 | 257124 | PASS，`nonProduction=true` |

OBJ golden 诊断：

- connectedComponents：2
- nonManifoldEdges：299
- inconsistentOrientedEdges：1305
- duplicateFaces：0
- selfIntersectionPairs：6，`selfIntersectionSampled=true`
- warnings：3

3MF golden 诊断：

- connectedComponents：3
- nonManifoldEdges：10939
- duplicateFaces：7190
- oppositeDuplicateFaces：7190
- selfIntersectionPairs：6，`selfIntersectionSampled=true`
- warnings：4

结论：真实 OBJ/3MF 可以在实验链路跑通并产生纹理壳层结果；但由于拓扑诊断未满足 strict closed 生产准入，当前只能以 `warn_and_attempt` + `nonProduction=true` 记录。

## 5. Seam 与多材质策略

当前策略：

1. 壳层 voxel 先通过 BVH 找最近三角。
2. tie-break 顺序为 distance、barycentric interior margin、triangle index。
3. 命中三角后只使用该三角的 UV / material / texture。
4. 不跨 UV seam 平均。
5. 不跨 material seam 混色。

`surface_shell_multimaterial_seam` 实测：

- triangles：12
- materials：2
- textures：2
- inside/shell/interior：40931 / 18188 / 22743
- sampledTextureVoxels：18188
- perMaterialSampledVoxels：
  - `red_checker`：13968
  - `blue_gradient`：4220
- textureCacheMisses：2
- textureCacheHits：18186

未完成项：repeat/wrap 纹理 fixture 尚未建立；当前采样边界按 clamp 行为验证。

## 6. 鲁棒性诊断状态

已完成：

- connected components
- duplicate faces
- opposite duplicates
- local winding inconsistency
- zero-volume components 字段
- min edge / min area / max aspect ratio
- thin feature warning
- AABB broad-phase self-intersection candidate / sampled 统计

未完成或不应误判为完成：

- 完整 narrow-phase triangle-triangle self-intersection 尚未实现。
- warnings/errors 仍为字符串，尚未固化为稳定 error code。
- zero-volume 当前有字段与逻辑，缺少专门触发 fixture。

## 7. 性能基线

Release benchmark 输出：

| Fixture | Triangles | LevelSet ms | BVH Build ms | Transfer ms | Peak Estimated MB | BVH Nodes | Tested Triangles |
|---|---:|---:|---:|---:|---:|---:|---:|
| `bench_1k` | 1152 | 64.59 | 0.64 | 1.48 | 2.84 | 767 | 26232 |
| `bench_10k` | 10400 | 59.96 | 7.22 | 2.82 | 4.69 | 8191 | 27727 |
| `bench_50k` | 51072 | 89.84 | 48.06 | 4.13 | 12.27 | 32767 | 31614 |

说明：

- `peakEstimatedBytes` 是结构级估算，不是系统进程峰值工作集。
- `processPeakWorkingSetBytes` 字段已预留，但当前 `processPeakWorkingSetAvailable=false`。
- 性能 golden 不做 strict time equality，只用于趋势观察。
- 100k fixture 是可选项，本轮未运行。

## 8. 验证命令

本轮已运行并通过：

```powershell
cmake --build build --config Debug --target surface_shell_robustness_unit_tests
.\build\Debug\surface_shell_robustness_unit_tests.exe

$env:VCPKG_ROOT='D:\vcpkg-openvdb'
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09b-r2 -Triplet x64-windows
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r2
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r2 -RunMatrix
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r2 -RunRealModels
.\scripts\run_surface_shell_real_model_tests.ps1 -BuildDir build-openvdb-09b-r2
.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09b-r2
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09b-r2

.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09b-r2-release -Triplet x64-windows
.\scripts\run_surface_shell_benchmarks.ps1 -BuildDir build-openvdb-09b-r2-release -Config Release

cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

OpenVDB 真实环境：

- `VCPKG_ROOT=D:\vcpkg-openvdb`
- OpenVDB version：12.0.1
- CMake configure 有 Boost `CMP0167` dev warning，不影响本轮构建。

## 9. 与生产链路的关系

本轮未改变以下生产协议：

- RGBWSV schema：`p0.rgbwsv.2`
- 通道顺序：R G B W S V
- 输出位深：uint8
- 极性：`black_is_print`
- MaterialPolicy 默认链路
- SupportShapePipeline
- `slicer_cli` 生产切片输出

09B-R2 输出只服务于实验报告、preview 和 benchmark，不写入生产 TIFF 包。

## 10. 未完成项与风险

| 项 | 状态 | 风险 |
|---|---|---|
| narrow-phase self-intersection | 未完成 | 当前自相交统计可能包含 AABB false positive，不能作为生产拒绝依据 |
| repeat/wrap texture fixture | 未完成 | 纹理边界策略尚未覆盖 repeat 模式 |
| process peak working set | 未完成 | 当前只能看结构估算内存，缺少 OS 级峰值 |
| stable warning/error code | 未完成 | 自动化系统不宜长期依赖字符串判断 |
| OBJ/3MF 跨格式一致性 | 未完成 | 真实 OBJ/3MF 来源拓扑和材质不一致，不能做逐像素一致性宣称 |
| 真实模型生产准入 | 未通过 | OBJ/3MF 均存在拓扑问题，只能 `warn_and_attempt` |

## 11. 下一阶段建议

建议先进入 09B-R3，而不是直接进入 09P 生产化：

1. 实现 narrow-phase triangle-triangle self-intersection，并降低 AABB candidate 误报。
2. 引入稳定 `ValidationErrorCode` / `WarningCode`。
3. 建立 repeat/wrap 纹理 fixture。
4. 接入 Windows process peak working set。
5. 对真实 OBJ/3MF 做拓扑修复策略评估：自动修复、拒绝、或保留 `warn_and_attempt` 非生产路径。

09C 可以并行做补偿光油几何原型，但必须继续保持实验路径隔离，不应依赖当前 09B-R2 直接进入生产 TIFF。
