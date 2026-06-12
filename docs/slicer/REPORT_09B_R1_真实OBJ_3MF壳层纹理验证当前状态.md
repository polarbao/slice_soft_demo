# REPORT_09B_R1_真实OBJ_3MF壳层纹理验证当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-12  
> 适用阶段：09B-R1  
> 分支：`spike/09B-R1-real-model-shell-texture`

---

## 1. 阶段结论

09B-R1 已完成隔离原型范围内的真实 importer 数据链路：

```text
OBJ/MTL/PNG 或 3MF Texture2D
→ SceneModel
→ indexed TriangleMeshData + UV/material mapping
→ topology diagnostics
→ OpenVDB level set
→ shell / interior 分类
→ BVH nearest triangle
→ barycentric UV
→ texture / diffuse / fallback RGB
→ report v2 / preview
```

OBJ 与 3MF 闭合纹理 fixture 均通过；缺失纹理、无 UV、开放网格和非流形网格行为均有自动验证。该链路仍是实验原型，没有接入 production `slicer_cli` 或 RGBWSV TIFF。

## 2. 新增工程结构

核心模块：

```text
src/slicer_core/geometry/SceneModelTriangleMeshAdapter.*
src/slicer_core/geometry/MeshTopologyDiagnostics.*
src/slicer_core/geometry/NearestTriangleQuery.*
src/slicer_core/materials/texture_application/SurfaceAttributeMap.*
src/slicer_core/materials/texture_application/SurfaceTextureTransfer.*
src/slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.*
src/slicer_core/materials/texture_application/SurfaceShellRealModelReport.*
```

Demo、测试与脚本：

```text
apps/surface_shell_real_model_demo/main.cpp
tests/unit/surface_shell_real_model/main.cpp
scripts/run_surface_shell_real_model_tests.ps1
```

新增 CMake target：

```text
surface_shell_real_model_demo
surface_shell_real_model_unit_tests
```

## 3. Mesh Adapter 与拓扑诊断

`SceneModelTriangleMeshAdapter` 当前支持：

- 将现有 `SceneModel` 三角形转换为 world-mm indexed mesh。
- 保留 source triangle index、三角形 UV 和 material name。
- 按位置容差量化去重顶点。
- 过滤并统计退化三角形。
- 对负 signed volume 的整体闭合网格执行可选 orientation flip，并同步交换 UV 顶点。

`MeshTopologyDiagnostics` 输出 source/accepted/degenerate triangle、unique vertex、boundary edge、non-manifold edge、signed volume 和 orientation flip 统计。

策略：

```text
strict_closed：boundary/non-manifold 输入在 OpenVDB 构建前拒绝
warn_and_attempt：保留诊断警告并允许实验性尝试
```

## 4. 最近三角形与纹理转移

已实现 AABB BVH，避免 `shellVoxel × triangle` 全量暴力搜索。每个 shell voxel 可得到最近三角形、closest point、barycentric coordinates 和 `distanceMm`。

颜色来源优先级：

```text
texture sample → material diffuse → configured fallback RGB
```

纹理转移复用现有 `load_texture_image`、`sample_texture_rgb` 和 `TextureSampleOptions`，并增加 texture cache、最大转移距离和来源计数。BVH 与 brute-force 已在 plane、edge、vertex 和组合 fixture 上对照通过。

## 5. Fixtures 与结果

共同参数：

```text
OpenVDB = 12.0.1
voxelSizeMm = 0.05
shellThicknessMm = 0.10
maxTransferDistanceMm = 0.186602540378444
meshPolicy = strict_closed
```

### 5.1 OBJ 与 3MF 正向用例

| 指标 | OBJ + MTL + PNG | 3MF Texture2D |
|---|---:|---:|
| acceptedTriangles | 12 | 12 |
| boundaryEdges | 0 | 0 |
| nonManifoldEdges | 0 | 0 |
| signedVolumeMm3 | 4.5 | 4.5 |
| activeVoxels | 103574 | 103574 |
| insideVoxels | 40931 | 40931 |
| shellVoxels | 18188 | 18188 |
| interiorVoxels | 22743 | 22743 |
| sampledTextureVoxels | 18188 | 18188 |
| uniqueColorCount | 191 | 8 |
| fallbackVoxels | 0 | 0 |
| uvOutOfRangeVoxels | 0 | 0 |
| outsideColoredVoxels | 0 | 0 |
| maxObservedDistanceMm | 0.05 | 0.05 |

OBJ 与 3MF 使用等价几何，shell voxel 差异为 0%，通过 1% 容差检查；两者均满足 `shell + interior = inside`，且壳层外没有 RGB 着色。

### 5.2 Fallback 与负向用例

| Case | 结果 |
|---|---|
| OBJ missing texture | `missingTextureVoxels=18188`，全部转入 `materialDiffuseVoxels=18188` |
| OBJ no UV | `missingUvVoxels=18188`，全部转入 `fallbackVoxels=18188` |
| Open mesh | `boundaryEdges=4`，`strict_closed` 在 level set 前拒绝 |
| Non-manifold OBJ | `nonManifoldEdges=1`，`strict_closed` 在 level set 前拒绝 |

## 6. Report v2

报告 schema 为 `p0.surface_shell_texture_report.2`，包含：

```text
input / meshDiagnostics / openvdb / grid / policy
stats / transferStats / performance / warnings / errors
```

主要报告路径：

```text
output/SurfaceShellObjReal/reports/surface_shell_texture_report.json
output/SurfaceShell3MfReal/reports/surface_shell_texture_report.json
output/SurfaceShellObjMissingTexture/reports/surface_shell_texture_report.json
output/SurfaceShellObjNoUv/reports/surface_shell_texture_report.json
output/SurfaceShellOpenMesh/reports/surface_shell_texture_report.json
output/SurfaceShellNonManifold/reports/surface_shell_texture_report.json
```

## 7. Preview

每个成功用例输出 `shell_layer_*.png`、`interior_layer_*.png` 和 `composite_layer_*.png`。

已人工检查：

- OBJ composite preview 显示 PNG 采样后的连续颜色。
- 3MF composite preview 显示 Texture2D checker 颜色。
- interior preview 与 shell RGB 分离。
- preview 仅用于实验诊断，不写 production TIFF。

## 8. 性能记录

2026-06-12 Debug 单次运行：

| Case | import | level set | transfer |
|---|---:|---:|---:|
| OBJ textured | 2.16 ms | 839.82 ms | 255.92 ms |
| 3MF textured | 10.58 ms | 848.79 ms | 285.93 ms |
| OBJ missing texture | 5.03 ms | 829.59 ms | 49.92 ms |
| OBJ no UV | 3.74 ms | 792.78 ms | 54.07 ms |

这些数据是诊断记录，不是 Release 性能基线。当前 fixture 只有 12 个三角形，尚不能代表复杂模型的 BVH 和纹理缓存开销。

## 9. 自动验证

已执行并通过：

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09b-r1 -Triplet x64-windows
.\scripts\run_surface_shell_real_model_tests.ps1 -BuildDir build-openvdb-09b-r1
.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09b-r1
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09b-r1
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

结果：

```text
real-model unit tests: PASS
OBJ/3MF/fallback/negative cases: PASS
generated-box 09B regression: PASS
OpenVDB smoke: PASS, version 12.0.1
USE_OPENVDB=OFF Debug build: PASS
CI quick: PASS
```

## 10. Production 影响

本阶段未修改 production `slicer_cli` 执行链、RGBWSV TIFF writer/reader、`p0.rgbwsv.2` schema、`R G B W S V` 通道顺序、8-bit/`black_is_print` 极性、MaterialPolicy 默认行为、SupportShapePipeline 或 Qt UI production workflow。

新增代码由独立 demo/test target 使用；默认 `USE_OPENVDB=OFF` 构建和 CI quick 已通过。

## 11. 当前限制

- 已验证的是通过现有 importer 加载的闭合 OBJ/3MF 纹理 fixture，不等价于复杂生产模型全面通过。
- 未验证真实指甲浮雕模型、薄壁、小间隙、尖角、自交、重复面和超大网格。
- `warn_and_attempt` 只作为实验策略提供，不代表开放或非流形模型可可靠生成 SDF。
- 未实现多 texture/material 混合壳层的复杂遮挡和接缝策略。
- 未实现 compensated varnish、支撑 clearance 或 overhang SDF。
- 未接入 production config、RGBWSV package 或 Qt UI。

## 12. 下一阶段判断

不建议直接进入 09P production 接入。建议进入 `09B-R2`，优先完成：

1. 使用真实指甲 OBJ/3MF 与复杂纹理模型建立 golden fixtures。
2. 增加薄壁、尖角、自交、反向局部面、重复面和大网格鲁棒性测试。
3. 建立 Release 性能与内存基线，验证 BVH 在高三角形数量下的收益。
4. 明确 UV seam、多个 texture/material、clamp/repeat 和纹理缺失的产品策略。
5. 09B-R2 通过后再评估 09C compensated varnish 或 09P production 接入设计。
