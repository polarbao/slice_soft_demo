# DOC_PREP_12E-R1 OpenVDB Conformance Adapter 准备

> 文档状态：PREPARED / 12E-04 READY FOR USER ADMISSION
> 日期：2026-07-17
> 前置任务：12E-01、12E-02、12E-03 COMPLETE
> 覆盖任务：12E-04 OpenVDB Conformance Adapter

## 1. 准备结论

12E-03 已提供默认 `USE_OPENVDB=OFF` 可运行的 CPU whole-model occupancy/distance candidate、统一 DTO、严格拓扑门禁、closest-surface reference 和核心性能证据。12E-04 可以在用户明确指定后实现 OpenVDB conformance backend，并在同一 mesh、同一 request grid、同一 width 下与 CPU candidate 比较。

12E-04 只提供 conformance/utility evidence。它不得写 production package，不得接入 Qt 或 composer，不得把 OpenVDB 设为默认，也不得把差异未冻结的结果标记为 production-safe。

## 2. 当前代码事实

可复用能力：

```text
BuildOpenVdbLevelSet：USE_OPENVDB=ON 时将 TriangleMeshData 转为 FloatGrid level set；
ClassifyOpenVdbSurfaceShell：基于 scan bounds 生成 inside/shell/interior 原型 mask；
OpenVdbIndexToWorld：index 到 world-mm 转换；
GetOpenVdbStatus：OFF stub 与 ON runtime 状态；
LegacyCpuGlobalDistanceBackend：CPU occupancy、欧氏距离、动态阈值和 closest reference；
GlobalTextureFillPartitionService：统一 grid/mask/invariant 校验；
NearestTriangleQuery：可为 OpenVDB inside voxel 建立同口径 triangle/barycentric reference；
build-openvdb-09p：当前缓存为 USE_OPENVDB=ON、x64-windows，工具链根为 D:\vcpkg-openvdb；
openvdb_sdf_utility_probe.exe：当前 ON build 中存在。
```

明确缺口：

```text
当前 OpenVdbSurfaceShellResult 使用自身 scan bounds，不保证与 request grid 对齐；
当前 API 没有 backend-neutral world-space SDF sample；
默认 interior band=3 voxels，不能直接证明厚模型 maxInteriorDistance；
当前 OpenVDB 原型没有输出 12E width metrics、closest references 或统一 performance DTO；
当前没有 CPU/OpenVDB conformance result、差异统计和 12E-04 测试 target；
OFF lane 尚无 12E adapter unavailable 单测。
```

## 3. 固定角色

```text
Legacy CPU backendRole = production_candidate；
OpenVDB backendRole = conformance_candidate；
USE_OPENVDB=OFF 时 OpenVDB adapter = unavailable；
USE_OPENVDB=ON 且通过自身不变量时 status 仍为 diagnostic；
productionAcceptance 始终为 not_evaluated；
12E-04 不改变任何 production admission policy。
```

## 4. Grid 对齐设计

OpenVDB backend 必须消费 12E request 中的最终 grid：

```text
TextureFillPartitionGridSpec：width/height/depth、origin、spacing；
每个分类点使用 request grid cell center 的 world-mm 坐标；
通过 backend 内部 world-to-index/sample API 读取 SDF；
返回的 model/texture/fill mask 必须逐字段复用 request grid；
不得把 OpenVDB scan-bounds mask 直接冒充 production-grid mask；
不得在 adapter 公共 DTO 中暴露 openvdb::FloatGrid、Coord 或 Transform。
```

建议新增内部 API：

```text
OpenVdbSignedDistanceSample SampleOpenVdbSignedDistanceWorld(
    levelSet,
    worldPointMm)

返回 available、signedDistanceMm 和 active/background 诊断；
公共头继续只使用 STL、Vec3 和项目 DTO。
```

## 5. Interior Band 与距离完整性

12E 需要模型内部最大距离，不能把窄带截断值当作真实阈值。12E-04 必须选择以下诚实策略：

```text
1. 根据 mesh bbox 对角线和 voxel size 计算足以覆盖内部的 interior band；或
2. OpenVDB 只负责 occupancy/shell candidate，maxInteriorDistance 和 closest reference 复用 NearestTriangleQuery；
3. 无论采用哪种策略，report 必须区分 SDF distance 和 exact nearest-triangle distance；
4. 若 band 不足，返回 blocked/fail 或明确 incomplete，不得输出全纹理阈值 PASS。
```

首选方案：OpenVDB 负责 signed occupancy，`NearestTriangleQuery` 对 OpenVDB inside voxels计算与 CPU 同口径欧氏距离和 reference。这样 conformance 比较聚焦 occupancy/sign 差异，避免窄带截断污染动态阈值。

## 6. Strict Admission

OpenVDB adapter 必须在 level-set 构建前复用 12E-03 的严格条件：

```text
ValidateTriangleMesh PASS；
boundaryEdges=0；
nonManifoldEdges=0；
duplicate/opposite duplicate=0；
local winding inconsistency=0；
confirmed self-intersection=0；
self-intersection audit 未被采样截断。
```

OFF、拓扑阻断、level-set 构建失败和 SDF sample 不完整必须使用稳定 issue，不允许异常退出或静默 fallback 到 CPU 后再标为 OpenVDB。

建议稳定 issue：

```text
E_12E_OPENVDB_BACKEND_UNAVAILABLE
E_12E_OPENVDB_TOPOLOGY_BLOCKED
E_12E_OPENVDB_LEVEL_SET_FAILED
E_12E_OPENVDB_GRID_SAMPLE_FAILED
E_12E_OPENVDB_DISTANCE_INCOMPLETE
E_12E_BACKEND_CONFORMANCE_FAILED
```

## 7. Conformance DTO

建议新增 backend-neutral 比较结果：

```text
cpu/openvdb available/status/backendRole；
sameGrid；
modelOnlyCpuVoxels；
modelOnlyOpenVdbVoxels；
textureOnlyCpuVoxels；
textureOnlyOpenVdbVoxels；
fillOnlyCpuVoxels；
fillOnlyOpenVdbVoxels；
max/mean distance delta；
allTextureThreshold delta；
runtime/peak memory pair；
partitionInvariantPass；
conformanceStatus = diagnostic | pass | fail | unavailable；
productionAcceptance = not_evaluated。
```

12E-04 可以记录实际差异，但不能在看到证据前虚构生产容差。阈值冻结留给后续明确决策或 12E-05 report/width-sweep 工作。

## 8. 测试矩阵

OFF lane：

| Case | 必须断言 |
|---|---|
| adapter status | unavailable + stable issue |
| CPU candidate | 继续独立 PASS |
| default build | 不链接 OpenVDB DLL，不改变现有 CTest |

ON lane：

| Fixture | 必须断言 |
|---|---|
| closed box | 同 grid；两 backend invariant PASS；记录 occupancy 差异 |
| sloped body | 记录 boundary/SDF 差异，不依赖逐层方向 |
| thin wall | 两 backend 均允许局部/整体 fill=0 |
| closed cavity | 内外闭合表面参与；中心 cavity 不属于 model |
| width minimum/threshold | 动态字段同单位、同 0.01 mm 量化 |
| open/non-manifold/self-intersection | OpenVDB 构建前 blocked |
| repeat | mask 和差异统计确定 |

## 9. 性能与依赖

分别记录：

```text
levelSetMs；
gridSampleMs；
nearestDistanceMs；
partitionMs；
totalCoreMs；
OpenVDB grid bytes；
mask/reference/BVH bytes；
process peak working set；
CPU/OpenVDB ratio 只作为实际观测，不先设结论。
```

依赖规则：

```text
默认 build：USE_OPENVDB=OFF，不要求 vcpkg/OpenVDB；
ON build：继续使用独立、无空格 D:\vcpkg-openvdb 和 x64-windows；
不得把 D:\Program Files Tools\vcpkg 的含空格路径用于当前 OpenVDB lane；
不得复制 installed/packages/buildtrees 目录做依赖迁移；
不得在现有 build cache 中切换 toolchain root。
```

## 10. 文件范围

允许修改：

```text
src/slicer_core/geometry/OpenVdbLevelSetBuilder.*（仅 backend-neutral sample/完整性接口）；
src/slicer_core/materials/texture_application/OpenVdbTextureFillConformanceBackend.*；
src/slicer_core/materials/texture_application/TextureFillPartitionTypes.*；
src/slicer_core/materials/texture_application/GlobalTextureFillPartitionService.*；
tests/unit/openvdb_texture_fill_conformance/*；
CMakeLists.txt；
12E task/schema/matrix/report/prep 文档。
```

禁止修改：

```text
slicer.cpp production generation；
MaterialChannelComposer / TIFF writer；
Qt UI；
OpenVDB 默认值；
legacy CPU candidate 的分区语义；
p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
12D repair。
```

## 11. 验证计划

默认 OFF：

```powershell
cmake --build build --config Debug --target openvdb_texture_fill_conformance_unit_tests
.\build\Debug\openvdb_texture_fill_conformance_unit_tests.exe
ctest --test-dir build -C Debug -R "openvdb_texture_fill_conformance|legacy_cpu_global_distance" --output-on-failure
```

可选 ON：

```powershell
cmake --build build-openvdb-09p --config Debug --target openvdb_texture_fill_conformance_unit_tests
.\build-openvdb-09p\Debug\openvdb_texture_fill_conformance_unit_tests.exe
ctest --test-dir build-openvdb-09p -C Debug -R "openvdb_texture_fill_conformance|legacy_cpu_global_distance|openvdb_sdf_utility" --output-on-failure
```

共同守门：

```powershell
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

## 12. Gate 结论

```text
12E-01：COMPLETE；
12E-02：COMPLETE；
12E-03：COMPLETE；
12E-04：PREPARED / READY FOR USER ADMISSION；
12E production：NOT ADMITTED。
```

12E-04 完成后才可准备 12E-05 Width Sweep 与 Report Schema。准备完成不自动启动 12E-04。
