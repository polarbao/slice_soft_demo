# REPORT_09A_R2_OpenVDB真实Smoke收口当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-12  
> 适用阶段：09A-R2

---

## 1. 阶段结论

09A-R2 已完成 OpenVDB 无空格依赖根与真实 ON smoke 收口。

本次确认：

- [A] `D:\vcpkg-openvdb` 已 clone 并 bootstrap 成功。
- [A] `D:\vcpkg-openvdb\scripts\buildsystems\vcpkg.cmake` 存在。
- [A] `build-openvdb-r2` 使用 `USE_OPENVDB=ON` 配置成功。
- [A] vcpkg manifest feature `openvdb` 已解析并安装完成。
- [A] `openvdb:x64-windows@12.0.1` 已安装。
- [A] `geometry_kernel_demo` 在 OpenVDB ON 构建下编译成功。
- [A] `openvdb-smoke` 执行成功。
- [A] `openvdb.activeVoxels = 27`，满足 `activeVoxels > 0`。
- [A] 默认 `USE_OPENVDB=OFF` 构建通过。
- [A] `scripts/run_ci_quick.ps1` 通过。

结论：

```text
09A-R2 通过。
可以进入 09B，但 09B 仍需按独立 PRD/DEV/TASKS 执行。
本阶段没有将 OpenVDB 接入 production slicer_cli。
```

---

## 2. VCPKG_ROOT 与版本

本次使用无空格 vcpkg root：

```text
VCPKG_ROOT = D:\vcpkg-openvdb
toolchain = D:\vcpkg-openvdb\scripts\buildsystems\vcpkg.cmake
toolchain exists = true
triplet = x64-windows
```

vcpkg 信息：

```text
vcpkg commit = b216ddff25a1f432870e6c340ce79357049ef86e
vcpkg version = 2026-05-27-d5b6777d666efc1a7f491babfcdab37794c1ae3e
```

OpenVDB 信息：

```text
OpenVDB port/version = openvdb:x64-windows@12.0.1
OpenVDB report version = 12.0.1
OpenVDB library = build-openvdb-r2/vcpkg_installed/x64-windows/debug/lib/openvdb.lib
```

---

## 3. ON Configure 结果

执行：

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"
.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-r2 `
  -Triplet x64-windows
```

结果：

```text
PASS
```

关键配置项：

```text
BuildDir = build-openvdb-r2
CMAKE_TOOLCHAIN_FILE = D:/vcpkg-openvdb/scripts/buildsystems/vcpkg.cmake
Z_VCPKG_ROOT_DIR = D:/vcpkg-openvdb
USE_OPENVDB = ON
VCPKG_MANIFEST_FEATURES = openvdb
Generator = Visual Studio 18 2026
```

说明：

```text
首次 configure 因工具调用超时未捕获退出码，但后台 vcpkg/cmake 已完成并生成 CMakeCache 与构建文件。
随后使用相同 BuildDir 重新执行 configure，命令返回 0，并确认所有依赖已安装完成。
```

---

## 4. ON Build 与 Smoke 结果

执行：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r2
```

结果：

```text
PASS
```

构建结果：

```text
geometry_kernel_demo.vcxproj -> build-openvdb-r2\Debug\geometry_kernel_demo.exe
```

Smoke 输出摘要：

```text
case = openvdb-smoke
openvdbCompiled = true
shellPixels = 884
interiorPixels = 508
boundaryPixels = 440
OpenVDB version = 12.0.1
activeVoxels = 27
```

报告文件：

```text
output/GeometryKernelOpenVdb/reports/geometry_kernel_report.json
```

报告关键字段：

```text
schema = p0.geometry_kernel_report.1
openvdb.enabled = true
openvdb.available = true
openvdb.version = 12.0.1
openvdb.activeVoxels = 27
openvdb.gridName = openvdb_smoke_float_grid
openvdb.voxelSizeMm = 0.01
```

---

## 5. OFF 回归结果

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

## 6. 本次代码与脚本变更

本次只修改 OpenVDB smoke 辅助脚本：

```text
scripts/run_openvdb_smoke.ps1
```

变更：

```text
构建文件检查新增 SliceSoftDemo.slnx。
```

原因：

```text
Visual Studio 18 2026 生成 SliceSoftDemo.slnx。
旧脚本只识别 SliceSoftDemo.sln / build.ninja / Makefile，会误判 build-openvdb-r2 不完整。
```

---

## 7. 与阶段边界符合情况

符合项：

- [A] 使用无空格 vcpkg root 验证 OpenVDB。
- [A] 使用独立 `build-openvdb-r2`，未污染默认 `build`。
- [A] OpenVDB 仍为可选依赖，默认 `USE_OPENVDB=OFF`。
- [A] 未修改 production `slicer_cli` 切片链路。
- [A] 未修改 `p0.rgbwsv.2` 协议。
- [A] 未替换 `SupportShapePipeline`。
- [A] 未实现 production `surface_shell_texture`。
- [A] 未实现 production `compensated_varnish`。

---

## 8. 是否进入 09B

判断：

```text
可以进入 09B。
```

依据：

```text
USE_OPENVDB=ON configure 成功。
geometry_kernel_demo ON build 成功。
openvdb-smoke 成功。
activeVoxels > 0 已验证。
OpenVDB version 已写入 report。
默认 OFF build 与 quick CI 通过。
```

限制：

```text
09B 不应直接把 OpenVDB 接入生产 slicer_cli。
09B 应继续保持实验/demo 边界，除非后续 PRD/DEV 明确扩大范围。
```

---

## 9. 后续建议

建议下一阶段按 09B 文档执行：

1. 明确 OpenVDB SDF / shell demo 的实验范围。
2. 保持 `USE_OPENVDB=OFF` 默认不变。
3. 保持 `geometry_kernel_demo` 或独立实验 target 承载验证。
4. 不改变 RGBWSV TIFF 输出协议。
5. 不替换现有支撑生成主链路。

