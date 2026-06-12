# DEMO_09A_R1_OpenVDB真实Smoke复测验证方案

> 文档版本：v0.1
> 文档状态：Demo / 验证方案
> 适用阶段：09A-R1
> 建议提交目录：`docs/slicer/`

## 1. 验证目标

验证当前机器使用正确 `VCPKG_ROOT` 后，能否跑通真实 OpenVDB configure/build/smoke。

## 2. 验证命令

```powershell
$env:VCPKG_ROOT = "D:\Program Files Tools\vcpkg"

.\scripts\configure_openvdb_vcpkg.ps1 -BuildDir build-openvdb -Triplet x64-windows

.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```

## 3. OFF 回归

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

## 4. 验收 Checklist

- [ ] configure 成功。
- [ ] `build-openvdb/CMakeCache.txt` 存在。
- [ ] `build-openvdb` 内存在 `.sln`、`build.ninja` 或 `Makefile`。
- [ ] `geometry_kernel_demo` ON build 成功。
- [ ] `openvdb-smoke` 返回 0。
- [ ] `geometry_kernel_report.schema = p0.geometry_kernel_report.1`。
- [ ] `openvdb.enabled = true`。
- [ ] `openvdb.available = true`。
- [ ] `openvdb.activeVoxels > 0`。
- [ ] `openvdb.version` 非空。
- [ ] OFF `run_ci_quick.ps1` 仍通过。
