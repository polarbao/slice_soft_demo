# OPENVDB_DEPENDENCY_NOTES

> 文档版本：v0.2  
> 文档状态：Dependency Notes  
> 生成日期：2026-06-11  
> 适用阶段：09

---

## 1. 目标

记录 OpenVDB 在当前项目中的接入方式、依赖风险和验证结果。

09 阶段只做采用预研，不把 OpenVDB / SDF 接入生产 `slicer_cli`、RGBWSV TIFF 输出或支撑生成主链路。

---

## 2. 环境信息

```text
OS: Windows x64
Compiler: MSVC 19.50.35730.0 / Visual Studio 18 2026
CMake: 4.3.1
Build type: Debug
Default track: USE_OPENVDB=OFF
OpenVDB track: USE_OPENVDB=ON attempted, dependency package config not found
Branch: spike/09-openvdb-sdf-kernel
```

---

## 3. 当前接入策略

当前 CMake 选项：

```cmake
option(ENABLE_GEOMETRY_KERNEL_DEMO "Build experimental geometry kernel demo" ON)
option(USE_OPENVDB "Enable optional OpenVDB adapter" OFF)
```

默认：

```text
USE_OPENVDB=OFF
```

行为：

- 不查找 OpenVDB。
- `OpenVdbAdapter` 返回 stub status。
- `geometry_kernel_demo --case openvdb-smoke` graceful skip。
- 主项目仍完整构建。

ON 轨：

```cmake
find_package(OpenVDB CONFIG REQUIRED)
```

目标导入名按优先级尝试：

```text
OpenVDB::openvdb
OpenVDB::OpenVDB
```

---

## 4. 依赖项

基于 OpenVDB 官方依赖文档和包管理器信息，实际生产接入需要关注：

```text
OpenVDB
TBB
Blosc
Boost
Imath
OpenEXR
Zlib
```

说明：

- OpenVDB 官方文档列出多个 required / optional dependencies，并单独说明 Windows / vcpkg 构建建议。
- vcpkg openvdb port 当前依赖包含 `blosc`、多个 Boost 组件、`imath`、`openexr`、`tbb` 等。
- OpenVDB 官方 license 为 MPL-2.0。

参考：

- https://www.openvdb.org/documentation/doxygen/dependencies.html
- https://www.openvdb.org/documentation/doxygen/build.html
- https://vcpkg.io/en/package/openvdb.html
- https://www.openvdb.org/license/

---

## 5. 候选接入方式比较

### 5.1 vcpkg

建议优先级：高。

优点：

- 与当前项目“vcpkg manifest mode when needed”的方向一致。
- Windows / MSVC 下对 CMake toolchain 支持直接。
- 可通过 triplet 控制 x64 / dynamic / static。
- vcpkg port 明确列出 OpenVDB 依赖链。

风险：

- OpenVDB 依赖链较重，Debug 构建耗时和磁盘占用较高。
- DLL 部署需要统一处理 TBB / Blosc / Boost / OpenEXR / Imath 等运行时库。
- triplet 不一致会导致 Debug/Release 或 CRT ABI 问题。

建议命令形态：

```powershell
vcpkg install openvdb:x64-windows
cmake -S . -B build-openvdb `
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>\scripts\buildsystems\vcpkg.cmake `
  -DUSE_OPENVDB=ON `
  -DENABLE_GEOMETRY_KERNEL_DEMO=ON
```

### 5.2 Conan

建议优先级：中。

优点：

- ConanCenter 有 `openvdb` recipe。
- 对企业内部二进制缓存、profile、锁版本有优势。
- 可形成独立 profile，不强依赖全局 vcpkg。

风险：

- 当前项目没有 Conan 工程化基础。
- 与现有 CMake/vcpkg 方向并行会增加维护面。
- Windows/MSVC profile、运行时库和 transitive DLL 部署仍需额外规范。

参考：

- https://conan.io/center/recipes/openvdb

### 5.3 Source Build

建议优先级：低，仅用于依赖排障或定制。

优点：

- 可完全控制 OpenVDB / TBB / Blosc / Boost / Imath / OpenEXR 版本。
- 适合后续需要裁剪 feature 或做源码级问题定位时使用。

风险：

- 维护成本最高。
- Windows 下依赖定位、Debug/Release、DLL 部署风险最大。
- 不适合作为团队默认接入方式。

---

## 6. USE_OPENVDB=OFF 结果

已执行并通过：

```powershell
cmake --build build --config Debug
cmake --build build --config Debug --target geometry_kernel_demo
.\build\Debug\geometry_kernel_demo.exe --case heightfield-sdf --output output\GeometryKernelDemo
.\build\Debug\geometry_kernel_demo.exe --case surface-shell --shell-mm 0.05 --output output\GeometryKernelShell
.\build\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdbStub
.\scripts\run_geometry_kernel_tests.ps1
.\scripts\run_ci_quick.ps1
```

结果摘要：

```text
heightfield-sdf: PASS
surface-shell: PASS
openvdb-smoke: graceful skip / stub PASS
geometry_kernel_report.schema = p0.geometry_kernel_report.1
preview PNG generated
run_ci_quick: PASS
```

---

## 7. USE_OPENVDB=ON 结果

已执行：

```powershell
cmake -S . -B build-openvdb -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON
```

结果：

```text
Configure: FAILED
Build: not executed
openvdb-smoke: not executed
Runtime: not executed
```

失败摘要：

```text
Could not find a package configuration file provided by "OpenVDB"
with any of the following names:

  OpenVDB.cps
  openvdb.cps
  OpenVDBConfig.cmake
  openvdb-config.cmake

Add the installation prefix of "OpenVDB" to CMAKE_PREFIX_PATH or set
"OpenVDB_DIR" to a directory containing one of the above files.
```

判断：

```text
当前机器未配置 OpenVDB 开发包或 CMake package config。
这是依赖环境缺失，不影响 USE_OPENVDB=OFF 默认构建。
```

---

## 8. 已知问题

1. OpenVDB 依赖链重，Windows/MSVC 下配置成本高。
2. Debug / Release 运行时库和 DLL 部署必须统一。
3. `find_package(OpenVDB CONFIG REQUIRED)` 依赖 package config 路径，未通过 toolchain 或 `OpenVDB_DIR` 配置时会失败。
4. 若使用静态链接，需要重新评估 Boost/TBB/Blosc/OpenEXR/Imath 的链接和许可证分发要求。
5. OpenVDB 为 MPL-2.0，若修改 OpenVDB 源文件并分发，需要遵守 MPL-2.0 对修改文件的开源义务。

---

## 9. 推荐方案

当前推荐：

```text
09 阶段保持 USE_OPENVDB=OFF 默认；
后续 09A/09B/09C 前，用 vcpkg x64-windows 建立单独 build-openvdb 验证环境；
不要把 OpenVDB 放入默认主线构建；
OpenVDB adapter 保持隔离模块；
生产 slicer path 只在后续专项阶段通过明确 ADR/PRD 再接入。
```

下一步建议：

1. 增加 `vcpkg.json` 或单独 `vcpkg-configuration.json` 前，先确认团队统一 triplet。
2. 建立 `build-openvdb` 专用 CI 或手工验证流程，不阻塞默认 CI。
3. 在 09A/09B/09C 中只消费 geometry kernel 的中间 mask/report，不直接改 RGBWSV writer。
