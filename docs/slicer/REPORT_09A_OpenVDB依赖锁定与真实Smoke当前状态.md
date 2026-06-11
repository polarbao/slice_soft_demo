# REPORT_09A_OpenVDB依赖锁定与真实Smoke当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-11  
> 适用阶段：09A

---

## 1. 阶段结论

09A 已完成 OpenVDB 依赖锁定与真实 smoke 路径的工程化收口，但本机尚未完成真实 OpenVDB smoke。

完成内容：

- [A] `vcpkg.json` 已通过 optional feature 声明 `openvdb`。
- [A] `USE_OPENVDB=OFF` 默认构建仍通过。
- [A] `scripts/run_ci_quick.ps1` 仍通过。
- [A] `USE_OPENVDB=ON` 的 CMake 缺包错误已增强为可操作提示。
- [A] 新增 `scripts/configure_openvdb_vcpkg.ps1`。
- [A] 新增 `scripts/run_openvdb_smoke.ps1`。
- [A] `geometry_kernel_report.openvdb` 已包含 `activeVoxels`、`gridName`、`gridClass`、`voxelSizeMm`。
- [A] 未接入 production `slicer_cli` 输出链路。
- [A] 未修改 RGBWSV 协议。

未完成内容：

- [B] 前次 ON 验证使用了错误的 `C:\vcpkg` 路径；本机应使用 `VCPKG_ROOT=D:\Program Files Tools\vcpkg` 重新验证。
- [B] `activeVoxels > 0` 仅在代码路径和脚本校验中实现，尚未在本机真实 OpenVDB 环境验证。

---

## 2. 变更范围

代码与配置：

```text
CMakeLists.txt
vcpkg.json
src/slicer_core/geometry/GeometryKernelTypes.h
src/slicer_core/geometry/OpenVdbAdapter.cpp
src/slicer_core/geometry/GeometryKernelReport.cpp
scripts/configure_openvdb_vcpkg.ps1
scripts/run_openvdb_smoke.ps1
```

文档：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
docs/slicer/TASKS_09A_OpenVDB依赖锁定与真实Smoke任务清单.md
docs/slicer/REPORT_09A_OpenVDB依赖锁定与真实Smoke当前状态.md
```

未修改：

```text
apps/slicer_cli
src/slicer_core/output/rgbwsv
src/slicer_core/support
src/slicer_core/pipeline production path
manifest schema p0.rgbwsv.2
RGBWSV channel order
TIFF 8-bit / black_is_print polarity
```

---

## 3. OFF 构建结果

已执行：

```powershell
cmake --build build --config Debug
```

结果：

```text
PASS
```

摘要：

```text
slicer_core.lib generated
geometry_kernel_demo.exe generated
slicer_cli.exe generated
rip_reader_test.exe generated
support_shape_unit_tests.exe generated
slicer_debug_ui.exe generated
```

---

## 4. OFF CI Quick 结果

已执行：

```powershell
.\scripts\run_geometry_kernel_tests.ps1
.\scripts\run_ci_quick.ps1
```

结果：

```text
PASS
Geometry kernel tests complete.
CI quick complete.
```

覆盖：

- 支撑形态单测。
- regression quick。
- schema tests。
- support shape tests。
- golden tests。
- UI self-test。
- 3MF negative tests。
- RIP reader bad package tests。
- geometry kernel `heightfield-sdf` / `surface-shell` / `openvdb-smoke` OFF stub。

---

## 5. ON Configure 结果

已执行：

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot C:\vcpkg -BuildDir build-openvdb -Triplet x64-windows
```

结果：

```text
FAILED
```

失败原因：

```text
vcpkg toolchain file was not found:
C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

判断：

```text
前次命令使用了错误的硬编码路径 C:\vcpkg。
本机实际 vcpkg root 应从 VCPKG_ROOT 获取：
D:\Program Files Tools\vcpkg
且该目录下 scripts\buildsystems\vcpkg.cmake 已确认存在。
因此 ON configure 需要基于 VCPKG_ROOT 重新执行，当前真实 ON 结果不应继续归因为“本机无 vcpkg”。
```

另执行 CMake 缺包提示验证：

```powershell
cmake -S . -B build-openvdb-cmake-missing -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON
```

结果：

```text
FAILED as expected
```

验证点：

```text
CMake 错误中已提示 vcpkg manifest mode、CMAKE_TOOLCHAIN_FILE、VCPKG_TARGET_TRIPLET、OpenVDB_DIR。
```

---

## 6. ON Build 结果

已执行：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```

结果：

```text
FAILED
```

失败原因：

```text
OpenVDB build directory is incomplete: build-openvdb.
Re-run scripts/configure_openvdb_vcpkg.ps1 after installing/configuring vcpkg.
```

判断：

```text
ON configure 未完成，因此没有可构建的 Visual Studio solution / Ninja build files。
```

---

## 7. openvdb-smoke 结果

OFF stub 已执行：

```powershell
.\build\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdbStub09A
```

结果：

```text
PASS graceful skip
```

report 摘要：

```text
schema = p0.geometry_kernel_report.1
openvdb.enabled = false
openvdb.available = false
openvdb.version = ""
openvdb.activeVoxels = 0
openvdb.gridName = stub
openvdb.gridClass = stub
openvdb.voxelSizeMm = 0
```

真实 ON smoke：

```text
NOT EXECUTED
```

原因：

```text
前次 ON configure 使用了错误的 C:\vcpkg 路径，未完成可构建目录生成。
需要使用 VCPKG_ROOT=D:\Program Files Tools\vcpkg 重新配置后再执行 smoke。
```

---

## 8. activeVoxels

当前状态：

```text
OFF stub activeVoxels = 0
ON real activeVoxels = not available in this machine
```

代码路径：

```text
RunOpenVdbSmokeCase()
  openvdb::initialize()
  openvdb::FloatGrid::create(0.0F)
  grid->setName(...)
  grid->setTransform(...)
  accessor.setValue(...) for 3 x 3 x 3 voxels
  grid->activeVoxelCount()
```

预期真实 ON 结果：

```text
activeVoxels = 27
```

说明：

```text
脚本会强制校验 openvdb.activeVoxels > 0。
```

---

## 9. OpenVDB Version

当前状态：

```text
OFF stub version = ""
ON real version = not available in this machine
```

代码路径：

```text
OPENVDB_LIBRARY_MAJOR_VERSION_NUMBER
OPENVDB_LIBRARY_MINOR_VERSION_NUMBER
OPENVDB_LIBRARY_PATCH_VERSION_NUMBER
```

说明：

```text
真实 ON build 成功后，report 会写入 OpenVDB library version。
```

---

## 10. Dependency Notes

已更新：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

已记录：

- vcpkg manifest 当前内容，`openvdb` 为 optional feature。
- 推荐 triplet：`x64-windows`。
- Debug / Release 注意事项。
- OFF 通过结果。
- ON 配置失败结果。
- CMake 缺包提示验证结果。
- 推荐安装与后续验证步骤。

---

## 11. 是否进入 09B

建议：

```text
暂不把 09B 视为正式 OpenVDB 路径已解锁。
```

原因：

```text
真实 OpenVDB configure/build/smoke 尚未在本机通过。
```

可选路线：

1. 使用当前本机 `VCPKG_ROOT=D:\Program Files Tools\vcpkg` 重新执行 09A ON smoke，成功后进入 `09B surface shell texture prototype`。
2. 如果短期无法准备 OpenVDB 环境，可进入 `09B-alt pure-cpp shell prototype`，但必须明确标记为过渡方案，不能替代最终 OpenVDB 采用验证。

---

## 12. 下一步命令

准备 vcpkg 后执行：

```powershell
$env:VCPKG_ROOT = "D:\Program Files Tools\vcpkg"
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```

或使用环境变量：

```powershell
$env:VCPKG_ROOT = "D:\Program Files Tools\vcpkg"
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```
