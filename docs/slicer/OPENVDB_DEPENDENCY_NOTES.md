# OPENVDB_DEPENDENCY_NOTES

> 文档版本：v0.4
> 文档状态：Dependency Notes  
> 生成日期：2026-06-12  
> 适用阶段：09A / 09A-R2

---

## 1. 目标

记录 OpenVDB 在当前项目中的依赖接入方式、构建脚本、风险和验证结果。

09A 只做 OpenVDB 依赖锁定与真实 smoke 可复现路径，不把 OpenVDB / SDF 接入生产 `slicer_cli`、RGBWSV TIFF 输出或支撑生成主链路。

---

## 2. 环境信息

```text
OS: Windows x64
Compiler: MSVC 19.50.35730.0 / Visual Studio 18 2026
CMake: 4.3.1
Build type: Debug
Default track: USE_OPENVDB=OFF
OpenVDB track: USE_OPENVDB=ON verified through vcpkg helper script
Previous VCPKG_ROOT: D:\Program Files Tools\vcpkg
OpenVDB VCPKG_ROOT: D:\vcpkg-openvdb
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
- 主项目和 CI quick 不需要 OpenVDB。

ON 轨：

```cmake
find_package(OpenVDB CONFIG QUIET)
```

如果找不到 OpenVDB，CMake 会输出可操作提示：

```text
USE_OPENVDB=ON but OpenVDB package config was not found.
Use vcpkg manifest mode...
Or configure manually with CMAKE_TOOLCHAIN_FILE / VCPKG_TARGET_TRIPLET.
Alternatively set OpenVDB_DIR.
```

目标导入名按优先级尝试：

```text
OpenVDB::openvdb
OpenVDB::OpenVDB
```

---

## 4. vcpkg Manifest

当前 `vcpkg.json` 已通过 optional feature 声明 OpenVDB：

```json
{
  "name": "slice-soft-demo",
  "version-string": "0.1.0",
  "dependencies": [
    "nlohmann-json",
    "tiff",
    "assimp"
  ],
  "features": {
    "openvdb": {
      "description": "Optional OpenVDB dependency for experimental geometry kernel smoke builds.",
      "dependencies": [
        "openvdb"
      ]
    }
  }
}
```

推荐 triplet：

```text
x64-windows
```

说明：

- OpenVDB 依赖链由 vcpkg port 解析。
- `openvdb` 位于 vcpkg optional feature 中，默认 manifest 构建不会自动拉取 OpenVDB。
- 09A 配置脚本通过 `-DVCPKG_MANIFEST_FEATURES=openvdb` 显式启用该 feature。
- 当前没有提交 `vcpkg-configuration.json` baseline，因此 vcpkg registry 版本仍由开发环境所选 vcpkg checkout 决定。
- 如果后续需要严格复现二进制依赖版本，应在 09B 或独立依赖治理阶段补充 registry baseline / CI cache 策略。

---

## 5. 依赖项

基于 OpenVDB 官方构建文档和 vcpkg port 信息，生产接入需要关注：

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

- OpenVDB 官方文档提供 vcpkg 构建入口，并说明 Windows 下可使用 `x64-windows` triplet。
- vcpkg `openvdb` port 当前依赖包含 `blosc`、多个 Boost 组件、`imath`、`openexr`、`tbb` 等。
- ConanCenter 存在 `openvdb` recipe，可作为企业内部二进制缓存路线的备选。
- OpenVDB 官方 license 为 MPL-2.0。

参考：

- https://www.openvdb.org/documentation/doxygen/build.html
- https://vcpkg.io/en/package/openvdb.html
- https://conan.io/center/recipes/openvdb
- https://www.openvdb.org/license/

---

## 6. 构建脚本

新增：

```text
scripts/configure_openvdb_vcpkg.ps1
scripts/run_openvdb_smoke.ps1
```

配置命令：

```powershell
$env:VCPKG_ROOT = "D:\Program Files Tools\vcpkg"
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows
```

脚本行为：

- 检查 `-VcpkgRoot` 或 `VCPKG_ROOT`。
- 检查 `<vcpkg-root>\scripts\buildsystems\vcpkg.cmake` 是否存在。
- 使用 vcpkg toolchain 配置 `USE_OPENVDB=ON`。
- 使用 `-DVCPKG_MANIFEST_FEATURES=openvdb` 显式启用 OpenVDB optional feature。
- 不修改默认 `build` 目录。

Smoke 命令：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```

脚本行为：

- 构建 `geometry_kernel_demo`。
- 执行 `--case openvdb-smoke`。
- 校验 `openvdb.enabled == true`。
- 校验 `openvdb.available == true`。
- 校验 `openvdb.activeVoxels > 0`。
- 如果 `build-openvdb` 是失败配置残留目录，会提示重新配置。

---

## 7. USE_OPENVDB=OFF 结果

已执行并通过：

```powershell
cmake --build build --config Debug
.\scripts\run_geometry_kernel_tests.ps1
.\scripts\run_ci_quick.ps1
.\build\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdbStub09A
```

结果摘要：

```text
Default build: PASS
Geometry kernel tests: PASS
CI quick: PASS
openvdb-smoke OFF: graceful skip / stub PASS
```

OFF report 摘要：

```text
openvdb.enabled = false
openvdb.available = false
openvdb.version = ""
openvdb.activeVoxels = 0
openvdb.gridName = stub
openvdb.gridClass = stub
openvdb.voxelSizeMm = 0
```

---

## 8. USE_OPENVDB=ON 结果

已执行：

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot C:\vcpkg -BuildDir build-openvdb -Triplet x64-windows
```

结果：

```text
Configure through script: FAILED
Build: not executed
openvdb-smoke: not executed
Runtime: not executed
activeVoxels: not available
OpenVDB version: not available
```

失败原因：

```text
vcpkg toolchain file was not found:
C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

修正说明：

```text
上述失败来自错误使用硬编码 C:\vcpkg。
当前本机可用 vcpkg root 应从 VCPKG_ROOT 获取：
D:\Program Files Tools\vcpkg
且该目录下 scripts\buildsystems\vcpkg.cmake 已确认存在。
```

应重新执行：

```powershell
$env:VCPKG_ROOT = "D:\Program Files Tools\vcpkg"
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows
```

随后执行：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```

结果：

```text
FAILED
OpenVDB build directory is incomplete: build-openvdb.
Re-run scripts/configure_openvdb_vcpkg.ps1 after installing/configuring vcpkg.
```

判断：

```text
当前机器并非没有 vcpkg；前次失败是因为命令使用了错误的 `C:\vcpkg` 路径。
真实 OpenVDB configure/build/smoke 需要基于 `VCPKG_ROOT` 重新执行。
在重新执行并得到结果前，ON 结果状态应视为 pending。
```

---

## 9. CMake 缺包提示验证

已执行：

```powershell
cmake -S . -B build-openvdb-cmake-missing -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON
```

结果：

```text
Configure: FAILED as expected
```

失败提示已包含：

```text
USE_OPENVDB=ON but OpenVDB package config was not found.
Use vcpkg manifest mode...
-DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
-DVCPKG_TARGET_TRIPLET=x64-windows
-DOpenVDB_DIR=<directory-containing-OpenVDBConfig.cmake>
```

---

## 10. 09A-R1 真实环境复测结果

已执行：

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows
```

结果：

```text
FAILED
```

原因：

```text
build-openvdb 是旧的无 vcpkg toolchain 失败缓存目录。
脚本已增强为识别该状态并提示使用 clean BuildDir 或移除旧 CMakeCache.txt。
```

随后使用独立目录执行：

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb-r1 -Triplet x64-windows
```

结果：

```text
Configure: FAILED
CMakeCache: generated
Solution: not generated
Vcpkg manifest mode: entered
Vcpkg feature: openvdb enabled
OpenVDB resolved version: openvdb:x64-windows@12.0.1
Installed packages before failure: 17
Failure package: hwloc:x64-windows@2.11.2
```

失败摘要：

```text
configure: WARNING: Libtool does not cope well with whitespace in `pwd`
configure: line 21376: D:/Program: No such file or directory
make[3]: *** No rule to make target '/d/Program', needed by 'hwloc-annotate.exe'. Stop.
cl: source file "D:\Program" ignored
c1: fatal error C1083: cannot open source file "Tools\vcpkg\..."
```

判断：

```text
当前脚本能正确读取 VCPKG_ROOT，并能进入 vcpkg manifest feature resolution。
真实失败点不是项目 CMake，也不是 OpenVDB adapter 代码，而是 vcpkg root 路径包含空格。
OpenVDB 传递依赖 hwloc 使用 autotools/libtool/make，在 Windows/MSVC 下不能可靠处理 `D:\Program Files Tools\vcpkg`。
```

已执行 smoke：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r1
```

结果：

```text
FAILED
OpenVDB build directory is incomplete: build-openvdb-r1.
```

因此：

```text
ON build: not executed
openvdb-smoke: not executed
activeVoxels: not available
OpenVDB version in report: not available
```

09A-R1 后建议：

```text
不要进入 09B。
需要 09A-R2：将 vcpkg root 移动或重新 bootstrap 到无空格路径，例如 D:\vcpkg 或 D:\Tools\vcpkg，再使用 clean BuildDir 重跑。
```

---

## 11. 09A-R2 无空格 vcpkg root 真实 Smoke 结果

09A-R2 使用无空格 vcpkg root 重新验证：

```text
VCPKG_ROOT = D:\vcpkg-openvdb
vcpkg commit = b216ddff25a1f432870e6c340ce79357049ef86e
vcpkg version = 2026-05-27-d5b6777d666efc1a7f491babfcdab37794c1ae3e
triplet = x64-windows
BuildDir = build-openvdb-r2
Generator = Visual Studio 18 2026
```

已执行：

```powershell
git clone https://github.com/microsoft/vcpkg.git D:\vcpkg-openvdb
& "D:\vcpkg-openvdb\bootstrap-vcpkg.bat"
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-r2 -Triplet x64-windows
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r2
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

结果：

```text
vcpkg clone/bootstrap: PASS
USE_OPENVDB=ON configure: PASS
ON build geometry_kernel_demo: PASS
openvdb-smoke: PASS
OFF default build: PASS
OFF run_ci_quick: PASS
```

OpenVDB 解析结果：

```text
OpenVDB port/version = openvdb:x64-windows@12.0.1
OpenVDB report version = 12.0.1
OpenVDB library = build-openvdb-r2/vcpkg_installed/x64-windows/debug/lib/openvdb.lib
USE_OPENVDB = ON
```

`output/GeometryKernelOpenVdb/reports/geometry_kernel_report.json` 摘要：

```text
schema = p0.geometry_kernel_report.1
caseName = openvdb-smoke
openvdb.enabled = true
openvdb.available = true
openvdb.version = 12.0.1
openvdb.activeVoxels = 27
openvdb.gridName = openvdb_smoke_float_grid
openvdb.voxelSizeMm = 0.01
shellPixels = 884
interiorPixels = 508
boundaryPixels = 440
```

脚本修正：

```text
scripts/run_openvdb_smoke.ps1 现在同时识别 SliceSoftDemo.sln 与 SliceSoftDemo.slnx。
原因是 Visual Studio 18 2026 生成 .slnx，旧检查只识别 .sln 时会误判 build directory incomplete。
```

结论：

```text
09A-R2 已完成 OpenVDB 真实 ON configure/build/smoke 收口。
含空格 vcpkg root 导致 hwloc 构建失败的问题已通过 D:\vcpkg-openvdb 规避。
默认 USE_OPENVDB=OFF 主线构建和 CI quick 不受影响。
可以进入 09B，但 09B 仍必须通过独立 PRD/DEV 明确边界后执行。
```

---

## 12. Debug / Release 注意事项

1. OpenVDB 依赖链较重，Debug 构建耗时和磁盘占用会明显增加。
2. `x64-windows` 默认动态链接，运行 `geometry_kernel_demo.exe` 时需要对应 DLL 可被找到。
3. Debug / Release 不应混用 vcpkg triplet、CRT 或手工拷贝的 DLL。
4. 如果改用 static triplet，应重新评估 Boost/TBB/Blosc/OpenEXR/Imath 的链接和分发策略。
5. OpenVDB 为 MPL-2.0；如果修改 OpenVDB 源文件并分发，需要遵守 MPL-2.0 对修改文件的开源义务。

---

## 13. 推荐方案

当前推荐：

```text
09A 保持 USE_OPENVDB=OFF 默认；
使用 vcpkg manifest mode 作为首选 OpenVDB 接入路径；
用 build-openvdb 隔离真实 OpenVDB 验证；
不要把 OpenVDB 放入默认主线构建；
OpenVDB adapter 保持在 src/slicer_core/geometry 内；
生产 slicer path 只在后续专项阶段通过明确 PRD/DEV 再接入。
```

下一步：

1. 准备无空格 vcpkg root，例如 `D:\vcpkg` 或 `D:\Tools\vcpkg`。
2. 设置 `VCPKG_ROOT` 到无空格路径。
3. 使用 clean BuildDir，例如 `build-openvdb-r2`。
4. 重新执行 `scripts/configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb-r2 -Triplet x64-windows`。
5. 配置成功后执行 `scripts/run_openvdb_smoke.ps1 -BuildDir build-openvdb-r2`。
6. 若 `openvdb-smoke` 真实通过且 `activeVoxels > 0`，再进入 `09B surface shell texture prototype`。
