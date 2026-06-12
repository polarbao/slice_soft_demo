# DEMO_09B_R1_真实OBJ_3MF壳层纹理验证方案

> 文档版本：v0.1  
> 文档状态：Demo / 验证方案  
> 适用阶段：09B-R1  
> 建议提交目录：`docs/slicer/`

---

## 1. 分支准备

```bash
git checkout spike/09B-openvdb-surface-shell-texture
git checkout -b spike/09B-R1-real-model-shell-texture
```

---

## 2. Fixture 准备

至少准备：

```text
samples/configs/openvdb/surface_shell_obj_real.json
samples/configs/openvdb/surface_shell_3mf_real.json
samples/configs/openvdb/surface_shell_obj_missing_texture.json
samples/configs/openvdb/surface_shell_obj_no_uv.json
samples/configs/openvdb/surface_shell_open_mesh.json
```

对应资源必须提交或使用仓库现有稳定样例。

---

## 3. OpenVDB ON 配置

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"

.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-09b-r1 `
  -Triplet x64-windows
```

---

## 4. 构建

```powershell
cmake --build build-openvdb-09b-r1 --config Debug --target surface_shell_real_model_demo

cmake --build build-openvdb-09b-r1 --config Debug --target surface_shell_real_model_unit_tests
```

---

## 5. OBJ 验证

```powershell
.\build-openvdb-09b-r1\Debug\surface_shell_real_model_demo.exe `
  --config samples\configs\openvdb\surface_shell_obj_real.json `
  --voxel-mm 0.05 `
  --shell-mm 0.10 `
  --mesh-policy strict_closed `
  --output output\SurfaceShellObjReal
```

---

## 6. 3MF 验证

```powershell
.\build-openvdb-09b-r1\Debug\surface_shell_real_model_demo.exe `
  --config samples\configs\openvdb\surface_shell_3mf_real.json `
  --voxel-mm 0.05 `
  --shell-mm 0.10 `
  --mesh-policy strict_closed `
  --output output\SurfaceShell3MfReal
```

---

## 7. 自动验证

```powershell
.\build-openvdb-09b-r1\Debug\surface_shell_real_model_unit_tests.exe

.\scripts\run_surface_shell_real_model_tests.ps1 `
  -BuildDir build-openvdb-09b-r1

cmake --build build --config Debug

.\scripts\run_ci_quick.ps1
```

---

## 8. 验收 Checklist

- [ ] OBJ fixture 导入成功。
- [ ] 3MF fixture 导入成功。
- [ ] OBJ/3MF level set activeVoxels > 0。
- [ ] shellVoxels > 0。
- [ ] interiorVoxels > 0。
- [ ] outsideColoredVoxels = 0。
- [ ] sampledTextureVoxels > 0。
- [ ] 完整纹理 fixture 的 fallback 数符合预期。
- [ ] report schema = `p0.surface_shell_texture_report.2`。
- [ ] composite preview 有多种纹理颜色。
- [ ] OBJ/3MF 同几何统计差异在约定容差内。
- [ ] missing texture 按策略 fallback。
- [ ] no UV 按策略 fallback。
- [ ] open mesh 在 strict_closed 下失败。
- [ ] OFF CI quick 通过。
- [ ] production RGBWSV 不变。
