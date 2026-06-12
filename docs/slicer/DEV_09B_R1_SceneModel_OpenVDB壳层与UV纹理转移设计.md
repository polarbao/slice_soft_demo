# DEV_09B_R1_SceneModel_OpenVDB壳层与UV纹理转移设计

> 文档版本：v0.1  
> 文档状态：DEV / 设计说明  
> 适用阶段：09B-R1  
> 建议提交目录：`docs/slicer/`

---

## 1. 当前代码基础

现有 `SceneModel` 实际为 `ModelReport`，已经包含：

```text
triangles
triangle_textures
material_infos
bbox_mm
OBJ/3MF report 信息
```

其中每个 `TriangleTextureInfo` 包含：

```text
has_uv
uv[3]
material_name
```

每个 `MaterialInfo` 包含：

```text
diffuse_rgb
has_diffuse
diffuse_texture_path
has_texture
texture_exists
texture_source
```

09B-R1 应复用这些数据，不重新实现 OBJ/3MF parser。

---

## 2. 推荐新增模块

```text
src/slicer_core/geometry/
  SceneModelTriangleMeshAdapter.h
  SceneModelTriangleMeshAdapter.cpp
  MeshTopologyDiagnostics.h
  MeshTopologyDiagnostics.cpp
  NearestTriangleQuery.h
  NearestTriangleQuery.cpp

src/slicer_core/materials/texture_application/
  SurfaceAttributeMap.h
  SurfaceAttributeMap.cpp
  SurfaceTextureTransfer.h
  SurfaceTextureTransfer.cpp
  SurfaceShellRealModelReport.h
  SurfaceShellRealModelReport.cpp

apps/surface_shell_real_model_demo/
  main.cpp

tests/unit/surface_shell_real_model/
  main.cpp

scripts/
  run_surface_shell_real_model_tests.ps1
```

---

## 3. SceneModelTriangleMeshAdapter

建议输出：

```cpp
struct SurfaceTriangleAttributes {
    std::size_t source_triangle_index;
    bool has_uv;
    std::array<TexCoord, 3> uv;
    std::string material_name;
};

struct AdaptedTriangleMesh {
    TriangleMeshData mesh;
    std::vector<SurfaceTriangleAttributes> triangle_attributes;
    MeshTopologyReport topology;
};
```

要求：

```text
mesh.triangles 与 triangle_attributes 一一对应；
变换后的 world-mm 坐标保持不变；
不得打乱 source triangle index；
退化 triangle 可以过滤，但必须记录映射和数量。
```

---

## 4. 顶点索引与拓扑诊断

`SceneModel` 当前保存非索引三角形。为诊断边界边和非流形边，需要构造稳定顶点索引。

建议：

```text
使用可配置 positionEpsilonMm 做顶点量化去重；
默认 1e-6 mm 或依据 voxel size 设置更合理的下限；
边采用排序后的 vertex index pair；
edge incidence = 1 → boundary edge；
edge incidence = 2 → manifold edge；
edge incidence > 2 → non-manifold edge。
```

报告：

```text
sourceTriangles
acceptedTriangles
degenerateTriangles
uniqueVertices
boundaryEdges
nonManifoldEdges
signedVolumeMm3
orientationFlipped
```

只允许做全局 orientation flip，不在本阶段做洞修补和复杂 remesh。

---

## 5. 最近三角形查询

真实模型不能对所有 shell voxel × 所有 triangle 做无界暴力搜索。

建议新增简单 AABB BVH：

```text
每个 triangle 建立 AABB 和 centroid；
按最长轴中位数分割；
leaf 保存有限数量 triangle；
nearest query 使用 point-to-AABB distance 剪枝；
返回 closest point、triangle index、barycentric、distanceMm。
```

接口建议：

```cpp
struct NearestTriangleHit {
    bool found;
    std::size_t triangle_index;
    Vec3 closest_point_mm;
    std::array<double, 3> barycentric;
    double distance_mm;
};

class NearestTriangleQuery {
public:
    explicit NearestTriangleQuery(const TriangleMeshData& mesh);
    NearestTriangleHit FindNearest(const Vec3& pointMm) const;
};
```

单元测试必须覆盖：

```text
triangle plane
edge closest point
vertex closest point
barycentric sum ~= 1
BVH 与 brute-force 小 fixture 结果一致
```

---

## 6. UV 插值与纹理采样

插值：

```text
uv = b0 * uv0 + b1 * uv1 + b2 * uv2
```

纹理采样复用：

```cpp
sample_texture_rgb(
    TextureImage,
    u,
    v,
    TextureSampleOptions,
    uv_out_of_range);
```

新增 texture cache：

```text
key = normalized texture path / embedded resource id
value = TextureImage
```

避免每个 voxel 重复加载纹理。

---

## 7. 颜色来源与 fallback

建议枚举：

```cpp
enum class ShellColorSource {
    Texture,
    MaterialDiffuse,
    Fallback
};
```

统计：

```text
sampledTextureVoxels
materialDiffuseVoxels
fallbackVoxels
missingUvVoxels
missingTextureVoxels
uvOutOfRangeVoxels
transferDistanceExceededVoxels
uniqueColorCount
```

`outsideColoredVoxels` 必须继续为 0。

---

## 8. Transfer Distance

shell voxel center 距离真实表面理论上受 shell thickness、voxel 对角线和 level-set 误差影响。

建议默认：

```text
maxTransferDistanceMm =
  shellThicknessMm + sqrt(3) * voxelSizeMm
```

可通过 demo 参数覆盖：

```text
--max-transfer-mm
```

超过阈值：

```text
不使用该 hit 采样纹理；
记录 transferDistanceExceeded；
按 material diffuse / fallback 处理。
```

---

## 9. Report v2

```json
{
  "schema": "p0.surface_shell_texture_report.2",
  "caseName": "obj-real-texture",
  "input": {
    "format": "obj",
    "modelPath": "",
    "configPath": ""
  },
  "openvdb": {},
  "grid": {},
  "policy": {},
  "meshDiagnostics": {
    "sourceTriangles": 0,
    "acceptedTriangles": 0,
    "degenerateTriangles": 0,
    "boundaryEdges": 0,
    "nonManifoldEdges": 0,
    "orientationFlipped": false
  },
  "stats": {},
  "transferStats": {
    "sampledTextureVoxels": 0,
    "materialDiffuseVoxels": 0,
    "fallbackVoxels": 0,
    "missingUvVoxels": 0,
    "missingTextureVoxels": 0,
    "uvOutOfRangeVoxels": 0,
    "transferDistanceExceededVoxels": 0,
    "maxObservedDistanceMm": 0.0,
    "uniqueColorCount": 0
  },
  "performance": {
    "importMs": 0,
    "levelSetMs": 0,
    "bvhBuildMs": 0,
    "transferMs": 0,
    "peakEstimatedBytes": 0
  },
  "warnings": [],
  "errors": []
}
```

---

## 10. Demo 参数

```text
surface_shell_real_model_demo
  --config <config.json>
  --output <dir>
  --voxel-mm <value>
  --shell-mm <value>
  --mesh-policy strict_closed|warn_and_attempt
  --max-transfer-mm <value>
```

通过现有 `load_slice_config` 和 `load_model_report` 导入，不重复写 importer。

---

## 11. OFF 构建

OFF 构建必须：

```text
surface_shell_real_model_unit_tests 中纯几何/UV/BVH tests 可运行；
OpenVDB real-model demo graceful unavailable；
run_ci_quick.ps1 继续通过。
```
