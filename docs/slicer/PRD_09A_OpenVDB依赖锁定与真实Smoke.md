# PRD_09A_OpenVDB依赖锁定与真实Smoke

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：09A  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

09 已完成几何内核隔离原型，但 `USE_OPENVDB=ON` 配置失败，失败原因是 CMake 找不到 OpenVDB package config。

既然后续正式切片工作将使用 OpenVDB，必须先把依赖安装、CMake 查找、链接和运行路径固化。

---

## 2. 产品目标

09A 的产品目标：

```text
让开发者可以在目标环境中按文档复现 OpenVDB 构建与 smoke 测试，
并且不会影响无 OpenVDB 环境下的默认构建。
```

---

## 3. 用户场景

### 3.1 无 OpenVDB 环境

开发者运行：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

期望：

```text
所有主线 target 正常；
geometry_kernel_demo 可构建；
openvdb-smoke graceful skip；
USE_OPENVDB=OFF 默认不报错。
```

### 3.2 有 OpenVDB 环境

开发者按文档安装依赖后运行：

```powershell
cmake -S . -B build-openvdb -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON -DCMAKE_TOOLCHAIN_FILE=<vcpkg-or-conan-toolchain>
cmake --build build-openvdb --config Debug --target geometry_kernel_demo
.\build-openvdb\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdb
```

期望：

```text
OpenVDB configure 成功；
geometry_kernel_demo 链接成功；
openvdb-smoke executed=true；
activeVoxels > 0；
geometry_kernel_report.openvdb.enabled=true；
geometry_kernel_report.openvdb.available=true。
```

---

## 4. 必须支持能力

### 4.1 依赖文档

输出并维护：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

必须包含：

```text
安装方式
CMake 配置命令
依赖库清单
Debug/Release 注意事项
USE_OPENVDB=OFF 结果
USE_OPENVDB=ON 结果
失败日志摘要
推荐方案
```

### 4.2 vcpkg manifest

建议新增：

```text
vcpkg.json
```

候选依赖：

```text
openvdb
tbb
blosc
boost
imath
zlib
```

具体名称以实际 vcpkg port 为准。

### 4.3 OpenVDB Smoke

`openvdb-smoke` 必须验证：

```text
openvdb::initialize()
FloatGrid::create()
setValue()
activeVoxelCount()
report 写出
preview 输出或至少 report 输出
```

### 4.4 构建脚本

建议新增：

```text
scripts/configure_openvdb_vcpkg.ps1
scripts/run_openvdb_smoke.ps1
```

---

## 5. 验收标准

```text
1. USE_OPENVDB=OFF 默认构建仍通过；
2. run_ci_quick.ps1 仍通过；
3. USE_OPENVDB=ON 至少在一个目标环境配置成功；
4. geometry_kernel_demo 链接 OpenVDB 成功；
5. openvdb-smoke executed=true；
6. activeVoxels > 0；
7. geometry_kernel_report 记录 openvdb.enabled=true；
8. OPENVDB_DEPENDENCY_NOTES.md 写明可复现步骤；
9. 不修改 production slicer_cli；
10. 不修改 RGBWSV 输出协议。
```

---

## 6. 非目标

```text
不实现 production surface_shell_texture
不实现 production compensated_varnish
不替换当前 slicer pipeline
不要求所有开发者必须安装 OpenVDB
不做设备通信
不做 RIP 半色调
