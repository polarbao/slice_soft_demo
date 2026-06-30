# REPORT_09A_R1_OpenVDB真实环境复测当前状态

> 文档版本：v0.1
> 文档状态：当前实现状态
> 生成日期：2026-06-12
> 适用阶段：09A-R1

---

## 1. 阶段结论

09A-R1 已完成真实环境复测，但 OpenVDB 真实 smoke 仍未通过。

本次确认：

- [A] 本机 `VCPKG_ROOT = D:\Program Files Tools\vcpkg`。
- [A] `D:\Program Files Tools\vcpkg\scripts\buildsystems\vcpkg.cmake` 存在。
- [A] `configure_openvdb_vcpkg.ps1` 能正确进入 vcpkg manifest mode。
- [A] vcpkg 已解析 `openvdb` optional feature。
- [A] vcpkg 解析到 `openvdb:x64-windows@12.0.1`。
- [A] `build-openvdb-r1/CMakeCache.txt` 已生成并记录 vcpkg toolchain。
- [A] ON configure 最终失败于 `hwloc:x64-windows@2.11.2`。
- [A] 失败原因是 vcpkg root 路径包含空格，`hwloc` 的 autotools/libtool/make 构建无法可靠处理。
- [A] OFF `cmake --build build --config Debug` 通过。
- [A] OFF `scripts/run_ci_quick.ps1` 通过。

当前不能进入 09B。建议进入 09A-R2：准备无空格 vcpkg root 后重测。

---

## 2. 实际 VCPKG_ROOT

```text
VCPKG_ROOT = D:\Program Files Tools\vcpkg
toolchain = D:\Program Files Tools\vcpkg\scripts\buildsystems\vcpkg.cmake
toolchain exists = true
triplet = x64-windows
```

---

## 3. 脚本复查与修正

复查：

```text
scripts/configure_openvdb_vcpkg.ps1
scripts/run_openvdb_smoke.ps1
```

修正：

```text
configure_openvdb_vcpkg.ps1
```

新增行为：

- 如果 `VcpkgRoot` 包含空格，输出 warning，提示 `hwloc` 等 OpenVDB 传递依赖可能失败。
- 如果 `BuildDir` 已有不匹配的旧 `CMakeCache.txt`，直接失败并提示使用 clean BuildDir 或移除旧 cache。

验证：

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows
```

结果：

```text
FAILED as expected
Existing build directory is not configured with the requested vcpkg toolchain: build-openvdb.
```

说明：

```text
build-openvdb 是 09A 前次失败残留目录；R1 使用 build-openvdb-r1 做真实复测。
```

---

## 4. ON Configure 结果

执行：

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb-r1 -Triplet x64-windows
```

结果：

```text
FAILED
```

已确认：

```text
VcpkgRoot: D:\Program Files Tools\vcpkg
BuildDir: build-openvdb-r1
Triplet: x64-windows
Vcpkg manifest mode: entered
VCPKG_MANIFEST_FEATURES=openvdb
CMAKE_TOOLCHAIN_FILE=D:/Program Files Tools/vcpkg/scripts/buildsystems/vcpkg.cmake
VCPKG_TARGET_TRIPLET=x64-windows
OpenVDB resolved: openvdb:x64-windows@12.0.1
Installed packages before failure: 17
```

已安装到 `build-openvdb-r1/vcpkg_installed` 的包包括：

```text
assimp
tiff
nlohmann-json
libjpeg-turbo
liblzma
zlib
vcpkg-cmake
vcpkg-cmake-config
vcpkg-cmake-get-vars
utfcpp
stb
rapidjson
pugixml
polyclipping
minizip
kubazip
jhasse-poly2tri
```

失败包：

```text
hwloc:x64-windows@2.11.2
```

关键日志：

```text
configure: WARNING: Libtool does not cope well with whitespace in `pwd`
configure: line 21376: D:/Program: No such file or directory
make[3]: *** No rule to make target '/d/Program', needed by 'hwloc-annotate.exe'. Stop.
cl: source file "D:\Program" ignored
c1: fatal error C1083: cannot open source file "Tools\vcpkg\..."
```

判断：

```text
PowerShell 脚本对含空格 toolchain 参数的传递可进入 vcpkg manifest mode；
真实失败发生在 vcpkg 传递依赖 hwloc 的 autotools/libtool/make 构建阶段。
根因是当前 vcpkg root 路径包含空格。
```

---

## 5. ON Build 结果

执行：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r1
```

结果：

```text
FAILED
```

失败原因：

```text
OpenVDB build directory is incomplete: build-openvdb-r1.
```

判断：

```text
ON configure 未完成，未生成 Visual Studio solution，因此 geometry_kernel_demo ON build 未执行。
```

---

## 6. openvdb-smoke 结果

```text
openvdb-smoke: not executed
openvdb.enabled: not available
openvdb.available: not available
activeVoxels: not available
OpenVDB version: not available
```

原因：

```text
OpenVDB 传递依赖未安装完成，ON build 未生成 geometry_kernel_demo。
```

---

## 7. OFF 回归结果

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
USE_OPENVDB=OFF 默认路径不受影响。
production slicer_cli 未接入 OpenVDB。
RGBWSV 输出协议未修改。
SupportShapePipeline 未替换。
```

---

## 8. 是否进入 09B

结论：

```text
不能进入 09B。
```

原因：

```text
USE_OPENVDB=ON configure 未成功。
geometry_kernel_demo ON build 未成功。
openvdb-smoke 未执行。
activeVoxels > 0 未验证。
OpenVDB version 未写入 report。
```

---

## 9. 是否需要 09A-R2

结论：

```text
需要 09A-R2。
```

建议目标：

```text
将 vcpkg root 移动或重新 bootstrap 到无空格路径，例如：
D:\vcpkg
D:\Tools\vcpkg
```

09A-R2 建议命令：

```powershell
$env:VCPKG_ROOT = "D:\vcpkg"
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb-r2 -Triplet x64-windows
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r2
```

如果团队不能调整本机 vcpkg root，则只能进入：

```text
09B-alt：pure-cpp shell prototype 临时过渡方案
```

但必须标记：

```text
09B-alt 不等价于 OpenVDB 采用完成。
```
