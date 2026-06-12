# DEMO_09B_OpenVDB_SDF表面壳层纹理验证方案

> 文档版本：v0.1  
> 文档状态：Demo / 验证方案  
> 适用阶段：09B  
> 建议提交目录：`docs/slicer/`

---

## 1. 分支准备

确保 09A-R2 已提交，然后执行：

```bash
git checkout spike/09-openvdb-sdf-kernel
git checkout -b spike/09B-openvdb-surface-shell-texture
```

---

## 2. OpenVDB ON 配置

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"

.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-09b `
  -Triplet x64-windows
```

---

## 3. 构建

```powershell
cmake --build build-openvdb-09b --config Debug --target surface_shell_texture_demo

cmake --build build-openvdb-09b --config Debug --target surface_shell_texture_unit_tests
```

---

## 4. Demo 验证

```powershell
.\build-openvdb-09b\Debug\surface_shell_texture_demo.exe `
  --case generated-box `
  --voxel-mm 0.05 `
  --shell-mm 0.10 `
  --texture-source checker `
  --output output\SurfaceShellTextureBox
```

---

## 5. 厚度单调性验证

```powershell
.\build-openvdb-09b\Debug\surface_shell_texture_demo.exe --case generated-box --voxel-mm 0.05 --shell-mm 0.05 --output output\SurfaceShell005

.\build-openvdb-09b\Debug\surface_shell_texture_demo.exe --case generated-box --voxel-mm 0.05 --shell-mm 0.10 --output output\SurfaceShell010

.\build-openvdb-09b\Debug\surface_shell_texture_demo.exe --case generated-box --voxel-mm 0.05 --shell-mm 0.20 --output output\SurfaceShell020
```

---

## 6. 自动测试

```powershell
.\build-openvdb-09b\Debug\surface_shell_texture_unit_tests.exe

.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09b

cmake --build build --config Debug

.\scripts\run_ci_quick.ps1
```

---

## 7. 验收 Checklist

- [ ] OpenVDB ON configure 成功。
- [ ] `surface_shell_texture_demo` 构建成功。
- [ ] `surface_shell_texture_unit_tests` 通过。
- [ ] generated-box level set 非空。
- [ ] shellVoxels > 0。
- [ ] interiorVoxels > 0。
- [ ] outsideColoredVoxels = 0。
- [ ] unclassifiedVoxels = 0 或有明确解释。
- [ ] shell + interior = inside。
- [ ] shell 0.20 的 shellVoxels >= shell 0.10 >= shell 0.05。
- [ ] report schema = `p0.surface_shell_texture_report.1`。
- [ ] preview PNG 存在。
- [ ] OFF `run_ci_quick.ps1` 通过。
- [ ] production RGBWSV 协议不变。
