# TASKS_09A_R1_OpenVDB真实环境复测任务清单

> 文档版本：v0.1
> 文档状态：Codex Task List
> 适用阶段：09A-R1
> 建议提交目录：`docs/slicer/`

## Milestone 09A-R1-0：确认当前失败原因

- [x] 阅读 `REPORT_09A_OpenVDB依赖锁定与真实Smoke当前状态.md`
- [x] 确认上次失败原因是使用了错误 `C:\vcpkg`
- [x] 确认本机实际 `VCPKG_ROOT = D:\Program Files Tools\vcpkg`
- [x] 确认 `D:\Program Files Tools\vcpkg\scripts\buildsystems\vcpkg.cmake` 存在

## Milestone 09A-R1-1：脚本空格路径兼容复查

- [x] 复查 `scripts/configure_openvdb_vcpkg.ps1`
- [x] 确认 `-DCMAKE_TOOLCHAIN_FILE=` 参数在含空格路径下可进入 vcpkg manifest mode
- [x] 复查 `scripts/run_openvdb_smoke.ps1`
- [x] 确认 build dir 路径存在性检查合理
- [x] 必要时为输出增加更明确日志

说明：R1 已为 `configure_openvdb_vcpkg.ps1` 增加旧 CMakeCache 检查，并在 `VcpkgRoot` 含空格时输出警告。真实失败发生在 vcpkg 传递依赖 `hwloc` 的 autotools/libtool 构建阶段，不是 PowerShell 传参本身。

## Milestone 09A-R1-2：重新执行 ON Configure

执行：

```powershell
$env:VCPKG_ROOT = "D:\Program Files Tools\vcpkg"
.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows
```

记录：

```text
configure: failed
vcpkg feature resolution: entered
OpenVDB package found: not reached
CMake generator: Visual Studio 18 2026
CMakeCache: generated, but solution not generated
failure package: hwloc:x64-windows@2.11.2
failure reason: vcpkg root path contains spaces
```

## Milestone 09A-R1-3：执行 ON Build 与 Smoke

执行：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```

必须校验：

```text
openvdb.enabled == true: not reached
openvdb.available == true: not reached
openvdb.activeVoxels > 0: not reached
openvdb.version 非空: not reached
```

结果：`run_openvdb_smoke.ps1 -BuildDir build-openvdb-r1` 已执行，失败原因为 build directory incomplete。

## Milestone 09A-R1-4：OFF 主线回归

执行：

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

确保：

```text
USE_OPENVDB=OFF 默认路径不受影响: passed
```

## Milestone 09A-R1-5：更新依赖文档

更新：

```text
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
```

必须记录：

```text
实际 VCPKG_ROOT: recorded
triplet: recorded
configure 命令: recorded
build 命令: recorded
OpenVDB version: not available
activeVoxels: not available
失败或成功日志摘要: recorded
下一步建议: 09A-R2 with no-space vcpkg root
```

## Milestone 09A-R1-6：生成状态报告

生成：

```text
docs/slicer/REPORT_09A_R1_OpenVDB真实环境复测当前状态.md
```

报告必须判断：

```text
是否进入 09B
是否还需要 09A-R2
是否只能进入 09B-alt
```
