# DOC_AUDIT_12B_R2 OpenVDB SDF Utility 代码盘点

> 文档状态：Audit / Stage 12B-R2
> 生成日期：2026-07-08
> 对应任务：12B-R2-01 当前 OpenVDB utility 代码盘点

## 1. 盘点结论

当前仓库中 OpenVDB 相关能力已经具备较完整的 experimental / prototype 基础，但还没有形成可直接接入 legacy production composer 的 SDF utility 模块。

结论分级：

| 分类 | 当前状态 | 结论 |
|---|---|---|
| OpenVDB 依赖门禁 | 已有 `USE_OPENVDB`，默认 OFF | 可作为 R2 边界基础 |
| Level set / shell classification | 已有 builder、surface shell mask | 可复用为 utility 内核候选 |
| Real-model shell texture prototype | 已有真实模型 prototype 和 report | 可复用 report/metrics 思路 |
| Texture transfer service | 已有 service boundary | 可作为 utility service 参考 |
| Production RGBWSV package | legacy path 已有，OpenVDB candidate 不应直接写 | R2 不接入 production |
| Clearance utility | 尚无独立模块 | 需要后续设计 |
| Material closure assist | 尚无 OpenVDB 专项 assist | 需要后续设计 |

## 2. CMake 与构建边界

当前 CMake 已有：

```text
option(USE_OPENVDB "Enable optional OpenVDB adapter" OFF)
```

OpenVDB ON 时：

```text
find_package(OpenVDB CONFIG QUIET)
target_compile_definitions(slicer_core PRIVATE SLICER_CORE_USE_OPENVDB=1)
target_link_libraries(slicer_core PRIVATE OpenVDB::openvdb 或 OpenVDB::OpenVDB)
```

判断：

```text
R2 可以继续沿用 USE_OPENVDB gate；
不得让默认 OFF build 依赖 OpenVDB；
R2 utility public report 不应暴露 OpenVDB C++ 类型。
```

## 3. Core API 盘点

### 3.1 `src/slicer_core/geometry/OpenVdbSurfaceShell.*`

已提供：

```text
OpenVdbSurfaceShellOptions.shell_thickness_mm
OpenVdbSurfaceShellResult.width/height/depth
inside_mask / shell_mask / interior_mask
inside_voxels / shell_voxels / interior_voxels / outside_voxels
warnings / error
ClassifyOpenVdbSurfaceShell(...)
MaskIndex3D(...)
```

R2 判断：

```text
可复用为 outer varnish shell / surface-shell metrics 的候选内核；
当前是 3D voxel mask，不等同于 production per-layer RGBWSV mask；
需要独立 utility report schema 描述其候选性质。
```

### 3.2 `SurfaceShellRealModelPrototype.*`

已提供：

```text
SurfaceShellRealModelOptions.voxel_size_mm
SurfaceShellRealModelOptions.shell_thickness_mm
SurfaceShellRealModelOptions.mesh_policy
SurfaceShellRealModelPerformance
SurfaceShellRealModelResult
RunSurfaceShellRealModelPrototype(...)
BuildSurfaceShellRealModelPreviewPixels(...)
```

R2 判断：

```text
可作为 real-model utility smoke 的基础；
当前定位仍是 09B real-model shell texture prototype；
R2 不应直接把该 prototype 输出当 production package。
```

### 3.3 `SurfaceShellTextureService.*`

已提供：

```text
SurfaceShellTextureServiceRequest
SurfaceShellTextureServiceResult
BuildSurfaceShellTextureIssues(...)
SurfaceShellTextureService::ApplyTexture(...)
```

R2 判断：

```text
已有服务边界可借鉴；
但它服务 texture transfer，不等于 clearance / material closure / outer varnish utility；
R2 可抽象相似的 report-only utility service。
```

### 3.4 `SurfaceShellRealModelReport.*`

已提供 report schema：

```text
p0.surface_shell_texture_report.2
```

当前报告内容包括：

```text
openvdb.enabled / available / version / activeVoxels / memoryBytes
grid width/height/depth/voxelSizeMm
policy shellThicknessMm / meshPolicy / maxTransferDistanceMm
meshDiagnostics / robustnessDiagnostics
productionAdmission status / blockers / warnings
performance counters
```

R2 判断：

```text
该 report 可作为 R2 utility report 的字段参考；
但 schema 名称和语义仍属于 surface-shell texture prototype；
R2 需要独立 slicesoft.openvdb_sdf_utility.12b_r2.1。
```

## 4. Apps 盘点

当前存在：

```text
apps/surface_shell_texture_demo
apps/surface_shell_real_model_demo
apps/surface_shell_robustness_demo
```

判断：

```text
这些 app 适合 R2 ON lane smoke / prototype 验证；
不适合作为 UI 普通用户入口；
不应直接和 legacy slicer_cli production path 混淆。
```

## 5. Scripts 盘点

可复用脚本：

```text
scripts/configure_openvdb_vcpkg.ps1
scripts/run_openvdb_smoke.ps1
scripts/run_09p_experimental_pipeline_tests.ps1
scripts/run_09p_r2_ci_matrix.ps1
scripts/run_surface_shell_texture_tests.ps1
scripts/run_surface_shell_robustness_tests.ps1
scripts/run_12b_core_benchmark.ps1
```

R2 判断：

```text
run_openvdb_smoke.ps1 可作为 ON lane 最小验证；
run_12b_core_benchmark.ps1 可作为 OFF lane legacy guard；
surface_shell_* scripts 可作为 candidate utility 评估参考，但需明确 experimental。
```

## 6. Samples 盘点

`samples/configs/openvdb` 下已有：

```text
experimental_openvdb_pipeline_disabled.json
surface_shell_3mf_real.json
surface_shell_duplicate_face.json
surface_shell_local_reversed.json
surface_shell_multimaterial_seam.json
surface_shell_nail_3mf_golden.json
surface_shell_nail_obj_golden.json
surface_shell_non_manifold.json
surface_shell_obj_missing_texture.json
surface_shell_obj_no_uv.json
surface_shell_obj_real.json
surface_shell_open_mesh.json
surface_shell_repeat_texture.json
surface_shell_repeat_texture_clamp.json
surface_shell_self_intersect.json
surface_shell_thin_wall.json
```

R2 判断：

```text
可用于 topology / texture / shell robustness 的候选矩阵；
真实 nail OBJ/3MF golden 可作为 diagnostic 证据；
strict blocker 模型不得绕过 admission 写 production output。
```

## 7. R2 Utility 候选成熟度

| Utility | 当前基础 | 成熟度 | 下一步 |
|---|---|---|---|
| OuterVarnishShellOffset | shell mask / SDF shell prototype | 中 | 定义独立 utility report 与 thickness metrics |
| ClearanceDistance | level set 基础存在 | 低 | 设计 distance metrics，不急于实现 |
| TopologyDiagnostic | robustness/admission/report 已有 | 高 | 输出 R2 capability matrix |
| MaterialClosureAssist | 12D 语义文档已有，SDF assist 未有 | 低 | 先定义 assist 边界和 source/confidence |

## 8. R2 阻断点

当前不应直接进入 production code 的原因：

```text
1. R2 独立 utility report schema 尚未固化；
2. clearance / material closure assist 尚无明确 DTO；
3. surface shell report schema 仍属于 09B prototype；
4. OpenVDB ON build 是否可用需要当前机器复测；
5. 当前真实模型存在 topology blocker，不能绕过 strict admission。
```

## 9. 建议下一任务

建议执行：

```text
Task 12B-R2-02 Utility Report Schema
```

原因：

```text
先固化 slicesoft.openvdb_sdf_utility.12b_r2.1；
再决定是否复用现有 surface shell prototype 生成最小 report；
避免继续向 prototype schema 添加新的产品语义。
```
