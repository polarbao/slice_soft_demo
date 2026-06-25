# DEV_09B_R2_拓扑诊断多材质Seam与性能基线设计

> 文档版本：v0.1
> 文档状态：DEV / 设计说明
> 适用阶段：09B-R2
> 建议提交目录：`docs/slicer/`

---

## 1. 当前代码审查结论

当前实现已经具备：

```text
SceneModel adapter
edge incidence topology
global signed volume
AABB BVH
closest point / barycentric
texture cache
texture/diffuse/fallback
report v2
```

但仍有以下工程缺口：

```text
1. 拓扑诊断未覆盖 duplicate face、自交、局部 winding inconsistency、connected components；
2. position epsilon 和 degenerate epsilon 是固定值，未结合模型尺度/voxel size；
3. BVH 没有节点访问统计和内存统计；
4. 当前 peakEstimatedBytes 未覆盖 OpenVDB grid、BVH、texture cache；
5. fixture 只有 12 triangle 等价盒体；
6. 自动脚本只使用固定 Debug 参数；
7. 多 material/texture seam 和 tie-break 未形成稳定语义。
```

---

## 2. 推荐新增模块

```text
src/slicer_core/geometry/
  MeshRobustnessDiagnostics.h
  MeshRobustnessDiagnostics.cpp
  TriangleIntersectionQuery.h
  TriangleIntersectionQuery.cpp
  MeshScaleTolerance.h
  MeshScaleTolerance.cpp

src/slicer_core/materials/texture_application/
  SurfaceSeamPolicy.h
  SurfaceSeamPolicy.cpp
  SurfaceShellBenchmarkReport.h
  SurfaceShellBenchmarkReport.cpp

apps/surface_shell_robustness_demo/
  main.cpp

tests/unit/surface_shell_robustness/
  main.cpp

scripts/
  run_surface_shell_robustness_tests.ps1
  run_surface_shell_benchmarks.ps1
```

---

## 3. MeshScaleTolerance

建议：

```cpp
struct MeshScaleTolerance {
    double position_epsilon_mm;
    double area_epsilon_mm2;
    double tie_epsilon_mm;
    double self_intersection_epsilon_mm;
};

MeshScaleTolerance MakeMeshScaleTolerance(
    const BoundingBox& bbox,
    double voxelSizeMm);
```

原则：

```text
不得只使用固定 1e-6 mm；
epsilon 应有绝对下限和相对模型尺度上限；
report 必须输出最终 epsilon。
```

---

## 4. MeshRobustnessDiagnostics

新增统计：

```text
connectedComponents
duplicateFaces
oppositeDuplicateFaces
inconsistentOrientedEdges
selfIntersectionPairs
zeroVolumeComponents
minEdgeLengthMm
maxEdgeLengthMm
minTriangleAreaMm2
maxTriangleAspectRatio
thinFeatureWarnings
```

### 4.1 Duplicate Face

将 triangle 的三个 vertex index 排序作为无向 face key：

```text
相同 key 出现多次 → duplicate face
方向相反 → opposite duplicate face
```

### 4.2 Local Winding

对 incidence=2 的边检查两个三角形是否沿相反方向使用该边。相同方向使用表示局部 winding inconsistency。

### 4.3 Connected Components

按共享 edge 建立 triangle adjacency，统计 components 和每个 component 的 signed volume。

### 4.4 Self Intersection

第一版使用：

```text
triangle AABB broad phase
→ 排除共享 vertex/edge 邻接 triangle
→ triangle-triangle narrow phase
```

对大模型允许：

```text
maxIntersectionPairs
timeBudgetMs
sampled=true
```

报告不得将 sampled 检查表述为完整检查。

---

## 5. BVH Instrumentation

扩展 `NearestTriangleQuery`：

```cpp
struct NearestTriangleQueryStats {
    std::uint64_t query_count;
    std::uint64_t visited_nodes;
    std::uint64_t tested_triangles;
    std::uint64_t max_visited_nodes;
    std::size_t node_count;
    std::size_t estimated_bytes;
};
```

增加：

```text
FindNearestWithStats
GetBuildStats
```

默认 production-free prototype 仍可使用现有 API。

---

## 6. Stable Tie-break

当前仅按 `< best.distance` 更新，等距结果可能受 BVH 排列影响。

09B-R2 必须改为稳定比较：

```text
distance
→ barycentric interior margin
→ source triangle index
```

建议：

```cpp
bool IsBetterHit(
    const NearestTriangleHit& candidate,
    const NearestTriangleHit& best,
    double tieEpsilonMm);
```

单元测试：

```text
equidistant coplanar triangles
UV seam triangles
material seam triangles
不同 BVH build order 结果一致
```

---

## 7. Texture/Material Cache 统计

增加：

```text
loadedTextureCount
textureCacheHits
textureCacheMisses
textureCacheBytes
materialCount
textureCount
perMaterialSampledVoxels
perTextureSampledVoxels
```

避免在 report 中只看到总数。

---

## 8. 内存统计

当前估算应扩展为：

```text
mesh bytes
triangle attribute bytes
mask bytes
shell RGB/source bytes
BVH bytes
texture cache bytes
OpenVDB memUsage bytes
preview buffer bytes
process peak working set
```

Windows 可选使用：

```text
GetProcessMemoryInfo
```

无平台实现时：

```text
字段标记 unavailable
```

不得伪造 peak memory。

---

## 9. Benchmark Fixture

建议增加程序生成高面数闭合 fixture：

```text
subdivided box
UV sphere
heightfield relief mesh
```

规模：

```text
1k / 10k / 50k / 100k triangles
```

真实指甲 fixture 作为业务 golden；程序生成 fixture 作为可控性能基线。

---

## 10. Benchmark 输出

```text
reports/surface_shell_benchmark_report.json
schema = p0.surface_shell_benchmark_report.1
```

不将绝对时间加入 strict golden，只记录和比较相对趋势。

---

## 11. Release 构建

独立目录：

```text
build-openvdb-09b-r2-release
```

命令：

```powershell
cmake --build build-openvdb-09b-r2-release --config Release
```

Debug 用于 correctness；Release 用于性能和内存基线。
