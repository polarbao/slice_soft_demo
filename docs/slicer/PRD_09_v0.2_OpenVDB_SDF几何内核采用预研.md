# PRD_09_v0.2_OpenVDB_SDF几何内核采用预研

> 文档版本：v0.2  
> 文档状态：Draft / PRD  
> 适用阶段：09  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

当前项目已完成：

```text
R1：核心模块边界重构
R2：配置、报告、测试、CI 工程化固化
08：支撑形态优化
08A：支撑桥接 fixture、unit test、真实 3MF support profile
```

后续正式切片能力需要更强的几何基础：

```text
surface_shell_texture
compensated_varnish
精确外表面/内表面判断
支撑 clearance / overhang diagnostics
薄壁、开口、复杂闭合模型鲁棒处理
```

这些能力不适合继续仅基于 2D layer mask 堆叠，因此需要 OpenVDB / SDF 预研。

---

## 2. 产品目标

09 v0.2 的目标：

```text
验证 OpenVDB 能否作为后续正式切片几何内核；
建立可独立运行的 geometry_kernel_demo；
建立 mesh/heightfield/mask 到 SDF/shell/slice mask 的最小闭环；
形成依赖接入方案和风险记录；
不影响当前生产 RGBWSV 输出。
```

---

## 3. 用户场景

### 3.1 无 OpenVDB 环境的开发者

开发者在未安装 OpenVDB 的环境中运行：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

期望：

```text
主项目仍能构建；
现有 slicer_cli / rip_reader_test / support_shape_unit_tests 不受影响；
geometry_kernel_demo 可在 stub 或 pure-cpp mode 下运行。
```

### 3.2 有 OpenVDB 环境的开发者

开发者开启：

```powershell
cmake -S . -B build-openvdb -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON
cmake --build build-openvdb --config Debug --target geometry_kernel_demo
```

期望：

```text
OpenVDB adapter 可编译链接；
可创建 OpenVDB grid；
可写入简单 level set / float grid；
可从 grid 导出 2D slice mask；
可输出 geometry_kernel_report.json。
```

### 3.3 几何策略预研

开发者运行：

```powershell
.\build\Debug\geometry_kernel_demo.exe --case surface-shell --shell-mm 0.05
```

期望：

```text
输出 shellPixels / interiorPixels / boundaryPixels；
生成 preview；
不接入生产 MaterialPolicy。
```

---

## 4. 必须支持能力

### 4.1 GeometryKernelBoundary

定义输入输出：

```text
Input:
  binary mask
  heightfield
  scene mesh / triangle soup
  voxel size / layer height

Output:
  DistanceField
  ShellMask
  SliceMask
  Diagnostics
```

### 4.2 DistanceField2D

无 OpenVDB 环境下的基础能力：

```text
binary mask → distance field → shell mask
```

第一版优先正确性，不追求性能。

### 4.3 OpenVDB Adapter

真实 OpenVDB 环境下验证：

```text
create FloatGrid
write simple values
read grid
export slice mask
record dependency status
```

### 4.4 GeometryKernelReport

输出：

```text
reports/geometry_kernel_report.json
schema = p0.geometry_kernel_report.1
```

### 4.5 Dependency Notes

必须输出：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

记录：

```text
Windows / CMake / C++20 接入方式
vcpkg / conan / source build 方案
OpenVDB / TBB / Blosc / Boost / Imath 依赖
USE_OPENVDB=ON 结果
USE_OPENVDB=OFF 结果
已知风险
```

---

## 5. 验收标准

```text
1. spike/09-openvdb-sdf-kernel 分支建立；
2. USE_OPENVDB=OFF 默认可构建；
3. USE_OPENVDB=ON 在目标环境至少完成一次真实验证或形成失败记录；
4. geometry_kernel_demo target 可构建；
5. heightfield-sdf case 可运行；
6. surface-shell case 可运行；
7. OpenVDB smoke case 可运行或 graceful skip；
8. geometry_kernel_report.json 输出；
9. preview PNG 输出；
10. run_ci_quick.ps1 仍通过；
11. 不修改 p0.rgbwsv.2；
12. 不接入生产 RGBWSV 输出。
```

---

## 6. 非目标

```text
不替换当前生产 slicer
不替换当前 support shape optimizer
不实现 production surface_shell_texture
不实现 production compensated_varnish
不接设备
不做 RIP 半色调
不做 ICC / CMYK
不强制 OpenVDB 成为所有开发环境的必需依赖
