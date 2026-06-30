# CODEX_PROMPT_09B_OpenVDB_SDF表面壳层纹理执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 适用阶段：09B  
> 建议提交目录：`docs/slicer/`

---

## 1. 分支

确认 09A-R2 已提交，然后切出：

```bash
git checkout spike/09-openvdb-sdf-kernel
git checkout -b spike/09B-openvdb-surface-shell-texture
```

---

## 2. 必读文档

```text
docs/slicer/REPORT_09A_R2_OpenVDB真实Smoke收口当前状态.md
docs/slicer/OPENVDB_DEPENDENCY_NOTES.md
docs/slicer/PRE_R0_DECISION_纹理壳层与光油几何策略约束.md
docs/slicer/DOC_DECISION_09B_09A_R2后进入OpenVDB_SDF表面壳层纹理原型阶段.md
docs/slicer/PRD_09B_OpenVDB_SDF表面壳层纹理原型.md
docs/slicer/DEV_09B_OpenVDB_SDF表面壳层纹理原型设计.md
docs/slicer/DEMO_09B_OpenVDB_SDF表面壳层纹理验证方案.md
docs/slicer/TASKS_09B_OpenVDB_SDF表面壳层纹理任务清单.md
```

---

## 3. 当前阶段

```text
09B：OpenVDB / SDF 表面壳层纹理原型
```

目标：

```text
1. 三角网格生成真实 OpenVDB level set；
2. 从 3D SDF 提取模型内部 outer shell；
3. 壳层写实验 RGB，内部标记 fill/base；
4. 输出 report 与 preview；
5. 保持 production slicer_cli / RGBWSV 不变。
```

---

## 4. 执行顺序

```text
09B-0：建立 09B 独立分支
09B-1：新增 TriangleMeshData 与 generated fixtures
09B-2：实现 OpenVdbLevelSetBuilder
09B-3：实现 OpenVdbSurfaceShell classifier
09B-4：实现 SurfaceShellTexturePrototype
09B-5：实现 report / preview
09B-6：新增 demo / unit tests
09B-7：新增自动测试脚本
09B-8：执行 ON prototype 和 OFF 回归
09B-9：生成 REPORT_09B
```

---

## 5. OpenVDB ON 构建

```powershell
$env:VCPKG_ROOT = "D:\vcpkg-openvdb"

.\scripts\configure_openvdb_vcpkg.ps1 `
  -VcpkgRoot $env:VCPKG_ROOT `
  -BuildDir build-openvdb-09b `
  -Triplet x64-windows

cmake --build build-openvdb-09b --config Debug --target surface_shell_texture_demo

cmake --build build-openvdb-09b --config Debug --target surface_shell_texture_unit_tests
```

---

## 6. 必须验证

```powershell
.\build-openvdb-09b\Debug\surface_shell_texture_demo.exe `
  --case generated-box `
  --voxel-mm 0.05 `
  --shell-mm 0.10 `
  --texture-source checker `
  --output output\SurfaceShellTextureBox

.\build-openvdb-09b\Debug\surface_shell_texture_unit_tests.exe

.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09b

cmake --build build --config Debug

.\scripts\run_ci_quick.ps1
```

---

## 7. 必须保持

```text
p0.rgbwsv.2 不变
production slicer_cli 默认路径不变
SupportShapePipeline 不替换
MaterialPolicy 默认行为不变
USE_OPENVDB=OFF 默认构建通过
```

不要做：

```text
不要接入 production RGBWSV TIFF；
不要替换 full-volume texture；
不要实现 compensated varnish；
不要同时展开 support clearance；
不要宣称真实 OBJ/3MF UV texture transfer 已完成，除非有明确 fixture 和测试。
```

---

## 8. 完成后生成

```text
docs/slicer/REPORT_09B_OpenVDB_SDF表面壳层纹理原型当前状态.md
```

报告必须包含：

```text
分支信息
OpenVDB version
voxel size
shell thickness
level set active voxels
inside/shell/interior voxels
outsideColoredVoxels
unclassifiedVoxels
thickness monotonic result
preview 输出
OFF CI quick 结果
production pipeline 影响结论
是否进入 09B-R1 / 09C / 09P
```
