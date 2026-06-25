# DEMO_09B_R2_真实指甲复杂拓扑与性能验证方案

> 文档版本：v0.1
> 文档状态：Demo / 验证方案
> 适用阶段：09B-R2
> 建议提交目录：`docs/slicer/`

---

## 1. 分支准备

```bash
git checkout spike/09B-R1-real-model-shell-texture
git checkout -b spike/09B-R2-shell-robustness-performance
```

---

## 2. OpenVDB ON 配置

Debug：

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"

.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-09b-r2 `
  -Triplet x64-windows
```

Release：

```powershell
.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-09b-r2-release `
  -Triplet x64-windows
```

---

## 3. Correctness 构建

```powershell
cmake --build build-openvdb-09b-r2 --config Debug --target surface_shell_robustness_demo

cmake --build build-openvdb-09b-r2 --config Debug --target surface_shell_robustness_unit_tests
```

---

## 4. 自动正确性验证

```powershell
.\build-openvdb-09b-r2\Debug\surface_shell_robustness_unit_tests.exe

.\scripts\run_surface_shell_robustness_tests.ps1 `
  -BuildDir build-openvdb-09b-r2
```

---

## 5. 真实模型验证

至少运行：

```powershell
.\build-openvdb-09b-r2\Debug\surface_shell_robustness_demo.exe `
  --config samples\configs\openvdb\surface_shell_nail_obj_golden.json `
  --voxel-mm 0.05 `
  --shell-mm 0.10 `
  --output output\SurfaceShellNailObjGolden

.\build-openvdb-09b-r2\Debug\surface_shell_robustness_demo.exe `
  --config samples\configs\openvdb\surface_shell_nail_3mf_golden.json `
  --voxel-mm 0.05 `
  --shell-mm 0.10 `
  --output output\SurfaceShellNail3MfGolden
```

---

## 6. Voxel/Thickness Matrix

```powershell
.\scripts\run_surface_shell_robustness_tests.ps1 `
  -BuildDir build-openvdb-09b-r2 `
  -RunMatrix
```

矩阵：

```text
voxel = 0.10 / 0.05 / 0.025 mm
shell = 0.05 / 0.10 / 0.20 mm
```

---

## 7. Release Benchmark

```powershell
cmake --build build-openvdb-09b-r2-release --config Release --target surface_shell_robustness_demo

.\scripts\run_surface_shell_benchmarks.ps1 `
  -BuildDir build-openvdb-09b-r2-release `
  -Config Release
```

---

## 8. OFF 回归

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

---

## 9. 验收 Checklist

- [ ] 真实指甲 OBJ golden 通过。
- [ ] 真实指甲 3MF golden 通过。
- [ ] 多 material/texture fixture 通过。
- [ ] UV seam fixture 通过。
- [ ] duplicate face 被诊断。
- [ ] local winding inconsistency 被诊断。
- [ ] self-intersection 被诊断或明确 sampled。
- [ ] strict_closed 拒绝不可接受拓扑。
- [ ] thin wall / sharp feature 有 report 与 preview。
- [ ] stable tie-break 测试通过。
- [ ] BVH 与 brute-force 小样本一致。
- [ ] 10k+ triangle Release benchmark 完成。
- [ ] 内存主要字段已记录。
- [ ] voxel/thickness matrix 完成。
- [ ] golden baseline 通过。
- [ ] OFF CI quick 通过。
- [ ] production RGBWSV 不变。
