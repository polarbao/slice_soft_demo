# DEV_09B_OpenVDB_SDF表面壳层纹理原型设计

> 文档版本：v0.1  
> 文档状态：DEV / 设计说明  
> 适用阶段：09B  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

新增隔离的 OpenVDB surface shell prototype：

```text
TriangleMeshData
→ OpenVdbLevelSetBuilder
→ OpenVdbSurfaceShellClassifier
→ SurfaceShellTexturePrototype
→ Report / Preview
```

不接入 production `slicer_cli`。

---

## 2. 推荐新增模块

```text
src/slicer_core/geometry/
  TriangleMeshData.h
  OpenVdbLevelSetBuilder.h
  OpenVdbLevelSetBuilder.cpp
  OpenVdbSurfaceShell.h
  OpenVdbSurfaceShell.cpp

src/slicer_core/materials/texture_application/
  SurfaceShellTexturePrototype.h
  SurfaceShellTexturePrototype.cpp
  SurfaceShellTextureReport.h
  SurfaceShellTextureReport.cpp

apps/surface_shell_texture_demo/
  main.cpp

tests/unit/surface_shell_texture/
  main.cpp

scripts/
  run_surface_shell_texture_tests.ps1
```

---

## 3. TriangleMeshData

建议结构：

```cpp
struct TriangleMeshData {
    std::vector<Vec3d> vertices;
    std::vector<std::array<int, 3>> triangles;
};
```

第一版必须支持程序生成：

```text
box
sphere-like closed fixture
```

真实 OBJ/3MF adapter 可作为 P1。

---

## 4. OpenVDB Level Set

在 `SLICER_CORE_USE_OPENVDB` 下使用 OpenVDB tools 将 mesh 转为 level set。

建议输入：

```cpp
struct OpenVdbLevelSetOptions {
    double voxel_size_mm{0.05};
    double exterior_band_voxels{3.0};
    double interior_band_voxels{3.0};
};
```

实现注意：

```text
1. 明确 world mm 到 index space 的 transform；
2. 使用 level set grid class；
3. 记录 voxel size；
4. 记录 active voxel bbox；
5. 对空 mesh / 无三角形 fail fast；
6. OFF 构建返回 graceful unavailable。
```

---

## 5. Surface Shell 分类

OpenVDB level set 内部为负值。

分类：

```cpp
inside = phi < 0.0;
shell = phi < 0.0 && phi >= -shell_thickness_mm;
interior = phi < -shell_thickness_mm;
outside = phi >= 0.0;
```

注意 narrow-band level set 背景值语义。分类实现不得只遍历 active voxels 后误把深内部全部当作 outside。

第一版应：

```text
根据目标 bbox 扫描 index space；
使用 grid accessor 查询值；
结合 level set background / inactive value 处理深内部区域；
在 report 中记录 unknown/unclassified voxels。
```

---

## 6. Shell RGB 应用

建议结果：

```cpp
struct SurfaceShellTextureResult {
    VoxelMask3D shell_mask;
    VoxelMask3D interior_mask;
    std::vector<Rgb8> shell_rgb;
    std::string interior_fill_role{"base"};
};
```

第一版纹理来源：

```text
fixture_constant
fixture_checker
```

可选增强：

```text
existing_texture_sampler_adapter
```

不得伪造真实 UV nearest-surface 映射完成度。

---

## 7. Report Schema

```json
{
  "schema": "p0.surface_shell_texture_report.1",
  "caseName": "generated-box",
  "openvdb": {},
  "grid": {
    "voxelSizeMm": 0.05,
    "activeVoxels": 0,
    "bbox": {}
  },
  "policy": {
    "mode": "surface_shell",
    "shellThicknessMm": 0.10,
    "shellRegion": "outer_surface",
    "fillRole": "base"
  },
  "stats": {
    "insideVoxels": 0,
    "shellVoxels": 0,
    "interiorVoxels": 0,
    "outsideColoredVoxels": 0,
    "unclassifiedVoxels": 0
  },
  "warnings": [],
  "timings": {}
}
```

---

## 8. Demo Target

新增：

```text
surface_shell_texture_demo
```

参数：

```text
--case generated-box
--case generated-sphere
--case imported-mesh
--input <path>
--output <dir>
--voxel-mm <value>
--shell-mm <value>
--texture-source constant|checker
```

`imported-mesh` 第一版可 graceful unsupported，除非本阶段明确完成 importer adapter。

---

## 9. 测试

单元测试：

```text
generated_box_level_set_non_empty
shell_and_interior_disjoint
shell_plus_interior_equals_inside
outside_colored_voxels_zero
shell_thickness_monotonic
invalid_shell_thickness_rejected
openvdb_off_graceful_skip
```

脚本测试：

```text
USE_OPENVDB=ON generated-box
0.05 / 0.10 / 0.20 shell monotonic
report schema
preview existence
OFF main CI regression
```
