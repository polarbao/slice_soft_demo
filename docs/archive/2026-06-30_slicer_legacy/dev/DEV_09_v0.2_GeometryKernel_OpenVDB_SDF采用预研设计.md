# DEV_09_v0.2_GeometryKernel_OpenVDB_SDF采用预研设计

> 文档版本：v0.2  
> 文档状态：DEV / 设计说明  
> 适用阶段：09  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

新增隔离的 geometry kernel experimental 模块：

```text
src/slicer_core/geometry/
apps/geometry_kernel_demo/
scripts/run_geometry_kernel_tests.ps1
```

该模块用于验证 OpenVDB/SDF 几何内核，不接入生产 `slicer_cli` 主流程。

---

## 2. 推荐新增目录

```text
src/slicer_core/geometry/
  GeometryKernelTypes.h
  DistanceField2D.h
  DistanceField2D.cpp
  ShellMask.h
  ShellMask.cpp
  GeometryKernelReport.h
  GeometryKernelReport.cpp
  OpenVdbAdapter.h
  OpenVdbAdapter.cpp
  OpenVdbDependencyStatus.h
  OpenVdbDependencyStatus.cpp

apps/geometry_kernel_demo/
  main.cpp

scripts/
  run_geometry_kernel_tests.ps1
```

---

## 3. CMake 设计

新增选项：

```cmake
option(ENABLE_GEOMETRY_KERNEL_DEMO "Build experimental geometry kernel demo" ON)
option(USE_OPENVDB "Enable optional OpenVDB adapter" OFF)
```

要求：

```text
USE_OPENVDB=OFF：
  不查找 OpenVDB；
  OpenVdbAdapter 使用 stub；
  geometry_kernel_demo 仍可运行 pure-cpp cases。

USE_OPENVDB=ON：
  尝试 find_package(OpenVDB CONFIG REQUIRED) 或项目约定的依赖方式；
  记录编译、链接、运行结果；
  不影响主线 target 的默认构建。
```

---

## 4. DistanceField2D

输入：

```cpp
struct BinaryMask2D {
    int width;
    int height;
    double pixel_size_mm;
    std::vector<std::uint8_t> inside;
};
```

输出：

```cpp
struct DistanceField2D {
    int width;
    int height;
    double pixel_size_mm;
    std::vector<float> distance_mm;
};
```

第一版算法：

```text
O(N^2) 最近边界距离；
优先 correctness；
小 fixture 即可；
后续再优化 EDT。
```

---

## 5. ShellMask

输入：

```text
DistanceField2D
shellThicknessMm
```

输出：

```text
shellMask
interiorMask
boundaryMask
shellPixels
interiorPixels
boundaryPixels
```

用途：

```text
后续 surface_shell_texture prototype；
后续 compensated_varnish prototype；
当前不接入 production MaterialPolicy。
```

---

## 6. OpenVdbAdapter

接口建议：

```cpp
struct OpenVdbStatus {
    bool compiled_with_openvdb;
    bool runtime_available;
    std::string version;
    std::vector<std::string> warnings;
};

OpenVdbStatus GetOpenVdbStatus();

bool RunOpenVdbSmokeCase(const std::filesystem::path& output_dir);
```

`USE_OPENVDB=OFF` 时：

```text
compiled_with_openvdb = false
runtime_available = false
smoke case graceful skip
```

`USE_OPENVDB=ON` 时：

```text
创建 FloatGrid；
写入简单 sphere/box level set 或 float values；
读取 grid；
输出一张 2D slice preview；
写入 geometry_kernel_report。
```

---

## 7. GeometryKernelReport

输出：

```json
{
  "schema": "p0.geometry_kernel_report.1",
  "caseName": "heightfield-sdf",
  "openvdb": {
    "enabled": false,
    "available": false,
    "version": ""
  },
  "grid": {},
  "distanceStats": {},
  "shellStats": {},
  "warnings": [],
  "timings": {}
}
```

---

## 8. geometry_kernel_demo

支持参数：

```text
--case heightfield-sdf
--case surface-shell
--case openvdb-smoke
--case compensated-varnish
--input <path>
--output <dir>
--shell-mm <value>
--thickness-mm <value>
```

默认输出：

```text
output/GeometryKernelDemo/
  reports/geometry_kernel_report.json
  preview/*.png
```

---

## 9. Dependency Notes

新增文档模板：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

必须记录：

```text
OpenVDB 安装方式；
CMake 查找方式；
依赖库；
是否支持 Debug/Release；
USE_OPENVDB=ON 构建结果；
失败日志摘要；
推荐后续方案。
```

---

## 10. 与生产链路隔离

不得修改：

```text
slicer_cli 默认执行路径
tiff_io
rip_reader
support shape optimizer
RGBWSV manifest schema
```

09 的所有输出应位于：

```text
output/GeometryKernel*
```
