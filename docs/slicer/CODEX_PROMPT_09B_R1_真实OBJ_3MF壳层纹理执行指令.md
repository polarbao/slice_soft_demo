# CODEX_PROMPT_09B_R1_真实OBJ_3MF壳层纹理执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 适用阶段：09B-R1  
> 建议提交目录：`docs/slicer/`

---

## 1. 分支操作

先确认 09B 已提交，然后执行：

```bash
git checkout spike/09B-openvdb-surface-shell-texture
git checkout -b spike/09B-R1-real-model-shell-texture
```

---

## 2. 必读文件

```text
docs/slicer/REPORT_09B_OpenVDB_SDF表面壳层纹理原型当前状态.md
docs/slicer/PRE_R0_DECISION_纹理壳层与光油几何策略约束.md
docs/slicer/DOC_DECISION_09B_R1_09B后进入真实OBJ_3MF壳层纹理验证与鲁棒性收口.md
docs/slicer/PRD_09B_R1_真实OBJ_3MF表面壳层纹理验证.md
docs/slicer/DEV_09B_R1_SceneModel_OpenVDB壳层与UV纹理转移设计.md
docs/slicer/DEMO_09B_R1_真实OBJ_3MF壳层纹理验证方案.md
docs/slicer/SURFACE_SHELL_TEXTURE_REAL_MODEL_CHECKLIST.md
docs/slicer/TASKS_09B_R1_真实OBJ_3MF壳层纹理任务清单.md
```

---

## 3. 当前阶段

```text
09B-R1：真实 OBJ/3MF 纹理模型壳层验证与鲁棒性收口
```

目标：

```text
SceneModel → TriangleMeshData + UV/material mapping
→ OpenVDB SDF shell
→ nearest source triangle
→ barycentric UV
→ texture/diffuse/fallback RGB
→ report v2 / preview
```

---

## 4. 执行顺序

```text
09B-R1-0：建立独立分支
09B-R1-1：实现 SceneModelTriangleMeshAdapter
09B-R1-2：实现 MeshTopologyDiagnostics
09B-R1-3：实现 NearestTriangleQuery / BVH
09B-R1-4：实现 SurfaceTextureTransfer
09B-R1-5：实现 report v2
09B-R1-6：准备 OBJ/3MF 与负向 fixtures
09B-R1-7：新增 demo/unit tests/script
09B-R1-8：执行 ON tests 与 OFF regression
09B-R1-9：生成 REPORT_09B_R1
```

---

## 5. 必须复用

```text
load_slice_config
load_model_report
SceneModel / ModelReport
TriangleTextureInfo
MaterialInfo
load_texture_image
sample_texture_rgb
OpenVdbLevelSetBuilder
OpenVdbSurfaceShell
```

不要重新实现 OBJ/MTL/3MF parser。

---

## 6. OpenVDB ON 构建

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"

.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-09b-r1 `
  -Triplet x64-windows

cmake --build build-openvdb-09b-r1 --config Debug --target surface_shell_real_model_demo

cmake --build build-openvdb-09b-r1 --config Debug --target surface_shell_real_model_unit_tests
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

## 8. 硬性验收

```text
OBJ/3MF real fixture level set 成功
shellVoxels > 0
interiorVoxels > 0
outsideColoredVoxels = 0
sampledTextureVoxels > 0
shell + interior = inside
strict_closed 对 open/non-manifold 输入失败
report schema = p0.surface_shell_texture_report.2
preview 输出真实纹理颜色
OFF CI quick 通过
production RGBWSV 不变
```

---

## 9. 红线

```text
不要接入 production slicer_cli
不要写 production RGBWSV TIFF
不要新增正式 surface_shell config 入口
不要实现 compensated varnish
不要替换现有 texture pipeline
不要用无限制 shellVoxel × triangle 暴力搜索
不要宣称所有非流形模型已被支持
```

---

## 10. 完成后生成

```text
docs/slicer/REPORT_09B_R1_真实OBJ_3MF壳层纹理验证当前状态.md
```

报告必须包含：

```text
分支
fixtures
mesh topology
OpenVDB stats
shell stats
transfer source counts
fallback counts
UV out-of-range
max transfer distance
performance
preview
OFF CI
production pipeline 影响
下一阶段判断
```
