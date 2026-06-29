# CODEX_PROMPT_09B_R3_R2后续执行指令

> 文档版本：v0.1
> 用途：复制给本地 VS Code Codex
> 建议分支：`spike/09B-R3-shell-production-readiness`

---

## 1. 当前背景

当前项目已完成 09B-R2，并已同步到 GitHub。

09B-R2 结论：

```text
OpenVDB 表面壳层纹理鲁棒性/性能基线已完成一轮实现与验证；
但尚不能进入 production RGBWSV 输出或 09P 生产化。
```

当前必须进入：

```text
09B-R3：壳层纹理生产准入前的拓扑精确诊断、稳定错误码、纹理边界与内存峰值收口
```

---

## 2. 分支操作

请从 R2 分支切出 R3：

```bash
git checkout spike/09B-R2-shell-robustness-performance
git pull
git checkout -b spike/09B-R3-shell-production-readiness
```

如果 R3 分支已存在：

```bash
git checkout spike/09B-R3-shell-production-readiness
git pull
```

---

## 3. 必读文件

先阅读：

```text
docs/slicer/REPORT_09B_R2_壳层纹理鲁棒性性能与多材质策略当前状态.md
docs/slicer/DOC_09B_R2_当前状态判断与后续操作路线.md
docs/slicer/TASKS_09B_R3_R2后续原子任务与阶段任务清单.md
docs/slicer/PRD_MASTER_SliceSoft_正式切片软件产品需求总览.md
docs/slicer/DEV_MASTER_SliceSoft_正式切片软件总体架构与实现路线.md
```

如果 MASTER PRD / DEV 不存在，请先提示用户补充或从当前文档包提交到 `docs/slicer/`。

---

## 4. 本阶段目标

只做以下 5 类收口：

```text
1. narrow-phase triangle-triangle self-intersection；
2. ValidationErrorCode / WarningCode；
3. repeat/wrap texture fixture；
4. Windows process peak working set；
5. 真实模型 topology production admission 策略。
```

---

## 5. 禁止事项

本阶段禁止：

```text
不要接入 production slicer_cli；
不要写 production RGBWSV TIFF；
不要修改 p0.rgbwsv.2；
不要修改 RGBWSV channel order；
不要替换 legacy texture path；
不要把 warn_and_attempt 结果宣称为 production safe；
不要实现 compensated varnish；
不要实现 support clearance；
不要做设备/RIP 工艺联调。
```

---

## 6. 执行顺序

```text
09B-R3-0：分支与文档同步
09B-R3-1：narrow-phase self-intersection
09B-R3-2：ValidationErrorCode / WarningCode
09B-R3-3：repeat/wrap texture fixture
09B-R3-4：Windows process peak working set
09B-R3-5：真实模型拓扑生产准入策略
09B-R3-6：Benchmark 扩展
09B-R3-7：全量回归
09B-R3-8：生成 REPORT_09B_R3
```

---

## 7. 重点实现要求

### 7.1 self-intersection

新增：

```text
TriangleIntersectionQuery.h/.cpp
```

必须区分：

```text
AABB candidate
confirmed intersection
coplanar overlap
touching only
false positive candidate
sampled
```

### 7.2 stable issue code

新增稳定 code，不再让脚本依赖字符串：

```text
MESH_BOUNDARY_EDGES
MESH_NON_MANIFOLD_EDGES
MESH_DUPLICATE_FACES
MESH_OPPOSITE_DUPLICATE_FACES
MESH_LOCAL_WINDING_INCONSISTENCY
MESH_SELF_INTERSECTION_CONFIRMED
MESH_SELF_INTERSECTION_SAMPLED
MESH_THIN_FEATURE_EDGE
MESH_THIN_FEATURE_AREA
TEXTURE_MISSING
TEXTURE_UV_MISSING
TEXTURE_UV_OUT_OF_RANGE
OPENVDB_UNAVAILABLE
OPENVDB_LEVEL_SET_FAILED
```

### 7.3 repeat/wrap fixture

新增：

```text
samples/configs/openvdb/surface_shell_repeat_texture.json
samples/models/openvdb/surface_shell_repeat_texture.obj
```

确保：

```text
uvAddressMode = repeat
uvOutOfRangeVoxels > 0
sampledTextureVoxels > 0
repeat 与 clamp 有可见差异
```

### 7.4 process memory

Windows 下实现：

```text
processPeakWorkingSetAvailable = true
processPeakWorkingSetBytes > 0
```

非 Windows 不失败。

---

## 8. 验证命令

必须执行：

```powershell
$env:VCPKG_ROOT='D:\vcpkg-openvdb'

.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09b-r3 -Triplet x64-windows

.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3 -RunMatrix
.\scripts\run_surface_shell_robustness_tests.ps1 -BuildDir build-openvdb-09b-r3 -RunRealModels
.\scripts\run_surface_shell_real_model_tests.ps1 -BuildDir build-openvdb-09b-r3
.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09b-r3
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09b-r3

.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09b-r3-release -Triplet x64-windows
.\scripts\run_surface_shell_benchmarks.ps1 -BuildDir build-openvdb-09b-r3-release -Config Release

cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

---

## 9. 完成后生成

```text
docs/slicer/REPORT_09B_R3_壳层纹理生产准入前诊断策略收口当前状态.md
```

报告必须判断：

```text
是否进入 09P；
是否需要 09B-R4；
是否可以并行 09C；
production RGBWSV 是否仍未被修改。
```
