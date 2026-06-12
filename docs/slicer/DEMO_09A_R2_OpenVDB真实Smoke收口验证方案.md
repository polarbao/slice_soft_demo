# DEMO_09A_R2_OpenVDB真实Smoke收口验证方案

> 文档版本：v0.1  
> 文档状态：Demo / 验证方案  
> 适用阶段：09A-R2  
> 建议提交目录：`docs/slicer/`

---

## 1. 验证目标

验证使用无空格 vcpkg root 后，OpenVDB 能在当前工程中真实配置、编译、链接和运行。

---

## 2. 环境准备

```powershell
git clone https://github.com/microsoft/vcpkg.git D:\vcpkg-openvdb
& "D:\vcpkg-openvdb\bootstrap-vcpkg.bat"
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"
```

---

## 3. ON Configure

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-r2 `
  -Triplet x64-windows
```

---

## 4. ON Build 与 Smoke

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-r2
```

---

## 5. OFF 回归

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

---

## 6. 验收 Checklist

- [ ] 无空格 VCPKG_ROOT 生效。
- [ ] hwloc 不再因路径空格失败。
- [ ] ON configure 返回 0。
- [ ] ON build 返回 0。
- [ ] openvdb-smoke 返回 0。
- [ ] report schema 为 `p0.geometry_kernel_report.1`。
- [ ] `openvdb.enabled = true`。
- [ ] `openvdb.available = true`。
- [ ] `activeVoxels > 0`。
- [ ] `openvdb.version` 非空。
- [ ] OFF CI quick 通过。
- [ ] production RGBWSV 输出协议不变。
