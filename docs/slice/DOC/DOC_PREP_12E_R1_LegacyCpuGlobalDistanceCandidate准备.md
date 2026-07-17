# DOC_PREP_12E-R1 Legacy CPU 全局三维距离候选准备

> 文档状态：PREPARED / 12E-03 READY FOR USER ADMISSION
> 日期：2026-07-17
> 前置任务：12E-01、12E-02 COMPLETE
> 覆盖任务：12E-03 Legacy CPU 3D Distance Candidate

## 1. 准备结论

12E-02 已建立 backend-neutral request/candidate/result、可注入 backend service、3D mask 结构和统一 partition invariant validator。12E-03 可以在用户明确指定后实现默认 OpenVDB OFF 可运行的 CPU whole-model candidate。

12E-03 只产生 diagnostic partition result、距离与性能证据，不接入 composer、TIFF writer、Qt UI 或 production Profile。完成本任务也不代表 production admission。

## 2. 当前代码事实

可复用能力：

```text
TriangleMeshData / SceneModelTriangleMeshAdapter：最终场景转 indexed triangle mesh；
AnalyzeMeshTopology / AnalyzeMeshRobustness：boundary、non-manifold、winding、duplicate 和 self-intersection 诊断；
NearestTriangleQuery：AABB BVH 最近三角形、距离、重心坐标和查询统计；
MeshScaleTolerance：按模型尺度生成 epsilon；
ProcessMemoryStats：Windows working set / peak working set；
GlobalTextureFillPartitionService：统一校验 3D mask XOR/union/outside/overlap/unassigned。
```

当前缺口：

```text
没有 backend-neutral PointInClosedMeshQuery；
没有用于全三维 occupancy 的 ray/BVH 查询；
没有 LegacyCpuGlobalDistanceBackend；
GlobalTextureFillPartitionResult 尚无 width metrics、distance stats 和 closest-surface reference；
没有 box/sphere/thin-wall/cavity 的 CPU partition golden；
没有 CPU candidate core timing/peak memory report。
```

现有 `slicer.cpp` 的逐层 triangle-plane scanline 只能作为历史算法参考，不能直接作为 `global_3d_distance` 实现。

## 3. 输入契约

12E-03 使用 12E-02 已冻结的 backend-neutral request：

```text
mesh：完成单位、缩放、旋转、平移后的 TriangleMeshData 非拥有指针；
grid：与目标输出对齐的 width/height/depth、origin 和 spacing；
options：requestedWidthMm、widthStepMm、baseMinimumWidthMm、surfaceScope。
```

动态校验：

```text
mesh != null；
grid finite/positive，voxel count 无 size_t 溢出；
surfaceScope=all_closed_surfaces；
strict topology/robustness 无 blocker；
requestedWidthMm finite；
requestedWidthMm >= effectiveMinimumWidthMm。
```

## 4. CPU Candidate 模块边界

建议新增：

```text
geometry/PointInClosedMeshQuery.*
  AABB BVH ray query；
  stable ray direction / boundary tie rule；
  query stats 与 brute-force test oracle。

materials/texture_application/LegacyCpuGlobalDistanceBackend.*
  实现 IGlobalTextureFillPartitionBackend；
  构建 occupancy；
  调用 NearestTriangleQuery；
  生成 texture/fill exact complement；
  不写 package。

TextureFillPartitionTypes.*
  width metrics；
  distance/query/performance stats；
  closest triangle index + barycentric reference。
```

依赖方向：

```text
LegacyCpuGlobalDistanceBackend -> geometry query + partition DTO；
GlobalTextureFillPartitionService -> backend interface + invariant validation；
reports/tests -> validated result；
slicer/composer/output/UI 本任务不依赖 candidate。
```

## 5. Occupancy 算法

对 grid cell center `p` 执行完整三维点内判定：

```text
1. BVH 过滤 ray-triangle candidates；
2. 使用固定主射线与确定性备用方向；
3. 顶点/边命中采用 half-open/tie rule，避免双计数；
4. 奇数交点 -> inside，偶数 -> outside；
5. 距表面 <= boundaryEpsilonMm 的点采用最近面法向/稳定规则分类；
6. 记录 ambiguousBoundary、fallbackRay 和 query cost。
```

禁止使用：

```text
逐层 2D morphology；
顶面高度列投影；
OpenVDB OFF 时静默调用 OpenVDB；
仅凭 bbox 判断 inside。
```

为避免把新 BVH 错误带入候选，generated fixture 必须同时与 brute-force ray oracle 对比。

## 6. 距离与分区

对 `ModelMask=1` 的 voxel center：

```text
nearest = NearestTriangleQuery.FindNearestWithStats(p)；
distanceMm = nearest.distance_mm；
TextureSurface = distanceMm <= effectiveWidthMm + epsilonMm；
ModelFill = NOT TextureSurface；
```

动态量：

```text
classificationResolutionMm = max(spacingX, spacingY, spacingZ)；
effectiveMinimumWidthMm = max(0.10, 2 * classificationResolutionMm)；
maxInteriorDistanceMm = max(distanceMm for model voxels)；
allTextureThresholdMm =
  max(effectiveMinimumWidthMm, ceil(maxInteriorDistanceMm / 0.01) * 0.01)；
effectiveWidthMm = min(requestedWidthMm, allTextureThresholdMm)。
```

若请求宽度小于 effective minimum，必须稳定失败，不能 clamp 后伪装成原请求。请求宽度大于阈值时允许 effective width 收敛到阈值，并明确 `allTexture=true`。

## 7. Closest Surface Reference

至少为 TextureSurface voxel 保存：

```text
triangleIndex；
barycentric[3]；
distanceMm；
surface component/tie 信息若可用。
```

12E-03 不执行 RGB 纹理采样，但必须保证 reference 与 mask 同 grid、outside model 无 reference。12E-06 将复用该证据，避免重新做逐层最近面查询。

## 8. Strict Topology Gate

CPU candidate 必须复用并补齐：

```text
boundaryEdges == 0；
nonManifoldEdges == 0；
degenerate/duplicate/opposite duplicate 不构成 blocker；
local winding 一致；
confirmed self-intersection == 0；
self-intersection 检查未因采样上限而被误判为完整 PASS。
```

任何 blocker 返回 `available=true/status=blocked/partitionPass=false` 或等价 diagnostic result，不生成 production package。

建议稳定 issue：

```text
E_12E_CPU_MESH_MISSING
E_12E_CPU_GRID_INVALID
E_12E_CPU_TOPOLOGY_BLOCKED
E_12E_CPU_OCCUPANCY_FAILED
E_12E_CPU_NEAREST_SURFACE_FAILED
E_12E_SURFACE_SHELL_WIDTH_BELOW_EFFECTIVE_MINIMUM
E_12E_ALL_TEXTURE_THRESHOLD_UNAVAILABLE
E_12E_PARTITION_BACKEND_FAILED
```

backend 抛出的标准异常必须由 service 转换为 `E_12E_PARTITION_BACKEND_FAILED`，不得越过 diagnostic 边界导致进程异常退出；backend 返回的 grid 必须与 request 指定 grid 完全一致。

## 9. 性能与内存

只统计 core，不混入 TIFF/PNG/JSON I/O：

```text
topologyMs；
occupancyBuildMs；
distanceQueryMs；
partitionMs；
totalCoreMs；
peakWorkingSetBytes；
gridVoxelCount / insideVoxelCount；
rayQueryCount / testedTriangles / visitedNodes；
nearestQueryCount / testedTriangles / visitedNodes；
mask/distance/reference estimated bytes。
```

12E-03 先建立实际 baseline，不预设虚构的绝对性能门槛。

## 10. Generated Fixture Matrix

| Fixture | 重点 | 必须断言 |
|---|---|---|
| closed box | occupancy、距离、阈值 | partition pass；中心最大距离正确到量化误差 |
| sphere/sloped closed body | 三维欧氏距离 | 不随 layer XY 方向漂移 |
| thin wall | 双侧 shell 相遇 | 局部 fill=0，无 overlap/unassigned |
| closed cavity | 内外闭合表面 | all_closed_surfaces 均参与 |
| concave body | 凹面最近面 | 不退化为投影距离 |
| boundary sample | ray tie | 多次运行结果确定 |
| open mesh | topology | blocked |
| non-manifold | topology | blocked |
| self-intersection | topology | blocked |
| width below minimum | dynamic config | stable error |
| all-texture threshold | 终点 | texture=model、fill=0 |

若 sphere/cavity fixture 当前不存在，可在测试目录生成，不新增大体积二进制模型。

## 11. 文件范围

允许修改：

```text
src/slicer_core/geometry/PointInClosedMeshQuery.*
src/slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.*
src/slicer_core/materials/texture_application/TextureFillPartitionTypes.*
src/slicer_core/materials/texture_application/GlobalTextureFillPartitionService.*
src/slicer_core/system/ProcessMemoryStats.*（仅确有统计缺口时）
tests/unit/legacy_cpu_global_distance/*
CMakeLists.txt
12E schema/matrix/task/report/prep 文档
```

禁止修改：

```text
slicer.cpp production generation；
MaterialChannelComposer / TIFF writer；
Qt UI；
OpenVDB default 或 vcpkg 依赖；
p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
12D repair。
```

## 12. 验证计划

```powershell
cmake --build build --config Debug --target legacy_cpu_global_distance_unit_tests
.\build\Debug\legacy_cpu_global_distance_unit_tests.exe
ctest --test-dir build -C Debug -R "legacy_cpu_global_distance|texture_fill_partition_service" --output-on-failure
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

若真实模型性能基线被纳入，应使用 Release 独立命令并输出机器可读报告；Debug 数据不得作为性能结论。

## 13. 退出与后续 Gate

12E-03 完成需满足：

```text
generated closed fixtures partition invariants PASS；
width metrics 与 allTexture threshold 可复现；
strict topology blocker 生效；
默认 USE_OPENVDB=OFF 可独立运行；
core timing/peak memory 有实际值；
没有 production package 写入。
```

之后才可准备/执行 12E-04 OpenVDB Conformance Adapter。12E-03 完成不自动启动 12E-04，也不解除当前 production admission 门禁。
