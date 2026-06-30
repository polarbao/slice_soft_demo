# CODEX_PROMPT_09B_R2_鲁棒性性能与多材质策略执行指令

> 文档版本：v0.1
> 用途：复制给 VS Code Codex
> 适用阶段：09B-R2
> 建议提交目录：`docs/slicer/`

---

## 1. 分支操作

先确认 09B-R1 已提交：

```bash
git checkout spike/09B-R1-real-model-shell-texture
git status
git add .
git commit -m "feat(openvdb): complete 09B-R1 real-model shell texture validation"
git push -u origin spike/09B-R1-real-model-shell-texture
git checkout -b spike/09B-R2-shell-robustness-performance
```

---

## 2. 必读文档

```text
docs/slicer/REPORT_09B_R1_真实OBJ_3MF壳层纹理验证当前状态.md
docs/slicer/DOC_DECISION_09B_R2_09B_R1后进入真实模型鲁棒性性能与多材质策略收口.md
docs/slicer/PRD_09B_R2_表面壳层纹理鲁棒性性能与多材质策略收口.md
docs/slicer/DEV_09B_R2_拓扑诊断多材质Seam与性能基线设计.md
docs/slicer/DEMO_09B_R2_真实指甲复杂拓扑与性能验证方案.md
docs/slicer/SURFACE_SHELL_R2_ROBUSTNESS_PERFORMANCE_CHECKLIST.md
docs/slicer/TASKS_09B_R2_鲁棒性性能与多材质策略任务清单.md
```

---

## 3. 当前阶段

```text
09B-R2：真实模型鲁棒性、性能/内存与多材质策略收口
```

本阶段不接入 production pipeline。

---

## 4. 执行顺序

```text
09B-R2-0：建立分支与 golden 基线
09B-R2-1：准备真实指甲/浮雕 fixtures
09B-R2-2：实现 scale-aware tolerance
09B-R2-3：扩展 topology/robustness diagnostics
09B-R2-4：实现 stable tie-break 和 seam policy
09B-R2-5：增加 BVH/texture cache instrumentation
09B-R2-6：增加内存统计和 benchmark fixtures
09B-R2-7：执行 voxel/thickness matrix
09B-R2-8：增加 report/golden
09B-R2-9：执行全部 ON/OFF regression
09B-R2-10：生成 REPORT_09B_R2
```

---

## 5. 必须复用

```text
SceneModelTriangleMeshAdapter
MeshTopologyDiagnostics
NearestTriangleQuery
SurfaceTextureTransfer
OpenVdbLevelSetBuilder
OpenVdbSurfaceShell
load_model_report
load_texture_image
sample_texture_rgb
```

不要重写 OBJ/3MF importer。

---

## 6. OpenVDB ON 构建

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"

.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-09b-r2 `
  -Triplet x64-windows
```

---

## 7. 必须验证

```powershell
cmake --build build-openvdb-09b-r2 --config Debug --target surface_shell_robustness_demo

cmake --build build-openvdb-09b-r2 --config Debug --target surface_shell_robustness_unit_tests

.\build-openvdb-09b-r2\Debug\surface_shell_robustness_unit_tests.exe

.\scripts\run_surface_shell_robustness_tests.ps1 `
  -BuildDir build-openvdb-09b-r2

.\scripts\run_surface_shell_benchmarks.ps1 `
  -BuildDir build-openvdb-09b-r2-release `
  -Config Release

cmake --build build --config Debug

.\scripts\run_ci_quick.ps1
```

---

## 8. 硬性要求

```text
真实指甲 OBJ/3MF golden 通过
多 material/texture 和 UV seam 通过
duplicate/local winding/self-intersection 有诊断
stable nearest-hit tie-break 通过
10k+ triangle Release benchmark 完成
主要内存对象可统计
voxel/thickness matrix 完成
OFF CI quick 通过
production RGBWSV 未修改
```

---

## 9. 红线

```text
不要接入 production slicer_cli
不要写 production RGBWSV TIFF
不要新增正式 surface_shell config
不要实现 compensated varnish
不要把 warn_and_attempt 结果宣称为 production safe
不要把采样 self-intersection 检查宣称为完整检查
不要用机器相关绝对时间做严格 golden equality
```

---

## 10. 完成后生成

```text
docs/slicer/REPORT_09B_R2_壳层纹理鲁棒性性能与多材质策略当前状态.md
```

报告必须包含：

```text
fixtures
topology/robustness
epsilon
seam/tie-break
OpenVDB stats
texture source/cache
BVH query stats
Release performance
memory
voxel/thickness matrix
golden
OFF CI
production impact
下一阶段判断
```
