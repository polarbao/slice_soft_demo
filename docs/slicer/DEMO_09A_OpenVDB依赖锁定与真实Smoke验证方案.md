# DEMO_09A_OpenVDB依赖锁定与真实Smoke验证方案

> 文档版本：v0.1  
> 文档状态：Demo / 验证方案  
> 适用阶段：09A  
> 建议提交目录：`docs/slicer/`

---

## 1. 验证目标

验证 OpenVDB 能在当前工程中被真实配置、编译、链接和运行，同时保持默认 OFF 主线稳定。

---

## 2. 默认 OFF 验证

```powershell
cmake --build build --config Debug
.\build\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdbStub
.\scripts\run_geometry_kernel_tests.ps1
.\scripts\run_ci_quick.ps1
```

期望：

```text
openvdb.enabled=false
openvdb.available=false
openvdb-smoke graceful skip
CI quick complete
```

---

## 3. ON 验证：vcpkg 方案

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot C:\vcpkg -BuildDir build-openvdb -Triplet x64-windows

.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb
```

或手动：

```powershell
cmake -S . -B build-openvdb -DUSE_OPENVDB=ON -DENABLE_GEOMETRY_KERNEL_DEMO=ON -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build-openvdb --config Debug --target geometry_kernel_demo

.\build-openvdb\Debug\geometry_kernel_demo.exe --case openvdb-smoke --output output\GeometryKernelOpenVdb
```

---

## 4. 验收 Checklist

- [ ] OFF 默认构建通过。
- [ ] OFF `run_ci_quick.ps1` 通过。
- [ ] ON configure 成功，或失败原因写入 dependency notes。
- [ ] ON build 成功。
- [ ] ON openvdb-smoke 运行成功。
- [ ] report 中 `openvdb.enabled=true`。
- [ ] report 中 `openvdb.available=true`。
- [ ] activeVoxels > 0。
- [ ] 不修改 production slicer_cli。
- [ ] 不修改 RGBWSV 输出协议。
