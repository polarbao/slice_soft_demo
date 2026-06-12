# REPORT_09B_OpenVDB_SDF表面壳层纹理原型当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-12  
> 适用阶段：09B  
> 分支：`spike/09B-openvdb-surface-shell-texture`

---

## 1. 阶段结论

09B 已完成 OpenVDB / SDF 表面壳层纹理原型的第一版实现与验证。

本阶段实现的是隔离原型链路：

```text
generated-box TriangleMeshData
→ OpenVDB meshToLevelSet
→ inside / shell / interior 分类
→ shell checker/constant RGB
→ report / preview
```

明确未接入：

```text
production slicer_cli
production RGBWSV TIFF
MaterialPolicy 默认行为
SupportShapePipeline
真实 OBJ/3MF UV nearest-surface transfer
compensated varnish
```

---

## 2. 新增工程结构

新增核心模块：

```text
src/slicer_core/geometry/TriangleMeshData.h
src/slicer_core/geometry/TriangleMeshData.cpp
src/slicer_core/geometry/OpenVdbLevelSetBuilder.h
src/slicer_core/geometry/OpenVdbLevelSetBuilder.cpp
src/slicer_core/geometry/OpenVdbSurfaceShell.h
src/slicer_core/geometry/OpenVdbSurfaceShell.cpp
src/slicer_core/materials/texture_application/SurfaceShellTexturePrototype.h
src/slicer_core/materials/texture_application/SurfaceShellTexturePrototype.cpp
src/slicer_core/materials/texture_application/SurfaceShellTextureReport.h
src/slicer_core/materials/texture_application/SurfaceShellTextureReport.cpp
```

新增 app / test / script：

```text
apps/surface_shell_texture_demo/main.cpp
tests/unit/surface_shell_texture/main.cpp
scripts/run_surface_shell_texture_tests.ps1
```

新增 CMake target：

```text
surface_shell_texture_demo
surface_shell_texture_unit_tests
```

MSVC 构建适配：

```text
slicer_core 增加 /bigobj。
原因：OpenVDB 模板实例化在 MSVC 下超过默认 object section 限制。
```

---

## 3. OpenVDB 与构建环境

使用：

```text
VCPKG_ROOT = D:\vcpkg-openvdb
BuildDir = build-openvdb-09b
Triplet = x64-windows
OpenVDB port/version = openvdb:x64-windows@12.0.1
OpenVDB report version = 12.0.1
Generator = Visual Studio 18 2026
```

已执行：

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09b -Triplet x64-windows
```

结果：

```text
PASS
```

说明：

```text
OpenVDB 12.0.1 的 meshToLevelSet 当前可用重载使用统一 halfWidth。
代码保留 exterior/interior band option，并在调用时取两者最大值作为 halfWidth。
```

---

## 4. Demo 验证结果

已执行：

```powershell
.\build-openvdb-09b\Debug\surface_shell_texture_demo.exe `
  --case generated-box `
  --voxel-mm 0.05 `
  --shell-mm 0.10 `
  --texture-source checker `
  --output output\SurfaceShellTextureBox
```

结果：

```text
PASS
activeVoxels = 18061
insideVoxels = 4641
shellVoxels = 2652
interiorVoxels = 1989
outsideColoredVoxels = 0
```

报告：

```text
output/SurfaceShellTextureBox/reports/surface_shell_texture_report.json
schema = p0.surface_shell_texture_report.1
```

Preview：

```text
output/SurfaceShellTextureBox/preview/shell_layer_0008.png
output/SurfaceShellTextureBox/preview/interior_layer_0008.png
output/SurfaceShellTextureBox/preview/composite_layer_0008.png
```

---

## 5. Report 关键字段

`output/SurfaceShellTexture010/reports/surface_shell_texture_report.json` 摘要：

```text
schema = p0.surface_shell_texture_report.1
caseName = generated-box
openvdb.enabled = true
openvdb.available = true
openvdb.version = 12.0.1
openvdb.activeVoxels = 18061
grid.voxelSizeMm = 0.05
policy.shellThicknessMm = 0.10
policy.textureSource = checker
stats.insideVoxels = 4641
stats.shellVoxels = 2652
stats.interiorVoxels = 1989
stats.outsideColoredVoxels = 0
stats.unclassifiedVoxels = 0
stats.shellPlusInteriorEqualsInside = true
```

---

## 6. 厚度单调性

已执行：

```powershell
.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09b
```

结果：

```text
PASS
```

厚度统计：

```text
shell-mm = 0.05: shellVoxels = 1506, interiorVoxels = 3135
shell-mm = 0.10: shellVoxels = 2652, interiorVoxels = 1989
shell-mm = 0.20: shellVoxels = 4056, interiorVoxels = 585
```

判断：

```text
shellVoxels 单调不减少。
interiorVoxels 单调不增加。
outsideColoredVoxels 始终为 0。
```

---

## 7. Unit Tests

已执行：

```powershell
.\build-openvdb-09b\Debug\surface_shell_texture_unit_tests.exe
```

结果：

```text
PASS openvdb_off_graceful_skip
PASS generated_box_level_set_non_empty
PASS shell_and_interior_disjoint
PASS shell_plus_interior_equals_inside
PASS outside_colored_voxels_zero
PASS shell_thickness_monotonic
PASS invalid_shell_thickness_rejected
```

说明：

```text
默认 OFF 构建下，OpenVDB 相关测试 graceful skip；
ON 构建下执行完整 shell 语义测试。
```

---

## 8. OFF 回归

已执行：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

结果：

```text
PASS
CI quick complete.
```

确认：

```text
USE_OPENVDB=OFF 默认构建通过。
production slicer_cli 默认路径未接入 OpenVDB。
p0.rgbwsv.2 未修改。
R G B W S V 通道顺序未修改。
8-bit / black_is_print 未修改。
SupportShapePipeline 未替换。
MaterialPolicy 默认行为未修改。
```

---

## 9. 已实现与未实现

已实现：

- [A] generated-box fixture。
- [A] 空 mesh / 非法 triangle index fail fast。
- [A] OpenVDB level set builder。
- [A] inside / shell / interior 3D mask 分类。
- [A] shell/interior 不重叠。
- [A] shell + interior = inside。
- [A] 外部不写 RGB。
- [A] constant RGB 与 checker RGB。
- [A] `p0.surface_shell_texture_report.1`。
- [A] shell / interior / composite preview。
- [A] demo、unit tests、PowerShell 自动验证脚本。

未实现：

- [B] generated sphere fixture。
- [B] imported OBJ/3MF mesh adapter。
- [B] 真实纹理 UV nearest-surface transfer。
- [B] production RGBWSV TIFF 写入。
- [B] production `surface_shell_texture` 配置入口。
- [B] compensated varnish。
- [B] support clearance / overhang SDF 诊断。

---

## 10. 是否进入后续阶段

建议：

```text
进入 09B-R1，而不是直接进入 09P。
```

原因：

```text
09B 已证明 generated-box 上 OpenVDB SDF 壳层分类可行；
但尚未验证真实 OBJ/3MF 模型、复杂拓扑、纹理坐标 transfer、薄壁/尖角等鲁棒性。
```

后续优先级：

1. `09B-R1`：真实 OBJ/3MF 纹理模型壳层验证与鲁棒性收口。
2. `09C`：SDF compensated varnish prototype。
3. `09D`：SDF support clearance / overhang diagnostics。
4. `09P`：OpenVDB production pipeline 接入设计。

