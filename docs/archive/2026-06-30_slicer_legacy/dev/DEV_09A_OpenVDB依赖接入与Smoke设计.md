# DEV_09A_OpenVDB依赖接入与Smoke设计

> 文档版本：v0.1  
> 文档状态：DEV / 设计说明  
> 适用阶段：09A  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

修正 09 暴露的依赖缺口：

```text
CMake 找不到 OpenVDB package config
```

并建立一条可复现的 OpenVDB ON 构建链路。

---

## 2. CMake 策略

保持现有：

```cmake
option(USE_OPENVDB "Enable optional OpenVDB adapter" OFF)
```

增强建议：

```cmake
if(USE_OPENVDB)
    find_package(OpenVDB CONFIG REQUIRED)
    target_compile_definitions(slicer_core PRIVATE SLICER_CORE_USE_OPENVDB=1)
    ...
endif()
```

可以补充更友好的错误提示：

```text
请设置 CMAKE_TOOLCHAIN_FILE 或 OpenVDB_DIR
示例：-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
示例：-DOpenVDB_DIR=<path-to-openvdb-config>
```

---

## 3. vcpkg manifest 方案

建议新增 `vcpkg.json`：

```json
{
  "name": "slice-soft-demo",
  "version-string": "0.1.0",
  "dependencies": [
    "openvdb"
  ]
}
```

如实际构建需要额外依赖，由 vcpkg port 自动解析；若需要显式补充，再记录到 `OPENVDB_DEPENDENCY_NOTES.md`。

---

## 4. 脚本设计

### 4.1 configure_openvdb_vcpkg.ps1

参数：

```text
-VcpkgRoot
-BuildDir build-openvdb
-Triplet x64-windows
```

执行：

```powershell
cmake -S . -B $BuildDir `
  -DUSE_OPENVDB=ON `
  -DENABLE_GEOMETRY_KERNEL_DEMO=ON `
  -DCMAKE_TOOLCHAIN_FILE="$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=$Triplet
```

### 4.2 run_openvdb_smoke.ps1

执行：

```powershell
cmake --build build-openvdb --config Debug --target geometry_kernel_demo
.\build-openvdb\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdb
```

并检查：

```text
geometry_kernel_report.openvdb.enabled == true
geometry_kernel_report.openvdb.available == true
activeVoxels > 0
```

---

## 5. OpenVdbAdapter 增强

当前 adapter 已有：

```text
GetOpenVdbStatus()
RunOpenVdbSmokeCase()
```

09A 建议补充：

```text
active voxel bbox
grid class / grid name
voxel size metadata
OpenVDB version
```

输出到 report：

```json
{
  "openvdb": {
    "enabled": true,
    "available": true,
    "version": "...",
    "activeVoxels": 27
  }
}
```

---

## 6. 不影响默认构建

必须确认：

```text
USE_OPENVDB=OFF
cmake --build build --config Debug
run_ci_quick.ps1
```

仍然通过。

---

## 7. 状态报告

09A 完成后生成：

```text
docs/slicer/REPORT_09A_OpenVDB依赖锁定与真实Smoke当前状态.md
```
