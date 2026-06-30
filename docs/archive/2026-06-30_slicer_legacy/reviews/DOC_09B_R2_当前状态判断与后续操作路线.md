# DOC_09B_R2_当前状态判断与后续操作路线

> 文档版本：v0.1
> 文档状态：R2 后续操作判断 / Codex 必读
> 适用分支：`spike/09B-R2-shell-robustness-performance`
> 建议提交目录：`docs/slicer/`

---

## 1. 当前阶段判断

当前项目处于：

```text
09B-R2：真实模型鲁棒性、性能/内存与多材质策略收口
```

09B-R2 已完成一轮实验链路实现与验证，可以作为 OpenVDB 表面壳层纹理鲁棒性/性能基线。

但当前不应进入：

```text
09P：production pipeline 接入
```

原因：

```text
1. 真实 OBJ/3MF 虽然能跑通，但拓扑诊断没有达到 strict_closed 生产准入；
2. 当前真实模型仍以 warn_and_attempt + nonProduction=true 记录；
3. 自相交检测仍是 AABB broad-phase candidate / sampled，不是完整 narrow-phase；
4. warnings/errors 仍以字符串为主，尚未固化稳定 code；
5. repeat/wrap texture fixture 尚未建立；
6. process peak working set 尚未接入；
7. 当前结果没有写入 production RGBWSV TIFF。
```

因此 R2 后续应进入：

```text
09B-R3：壳层纹理生产准入前的拓扑精确诊断、稳定错误码、纹理边界与内存峰值收口
```

---

## 2. 当前是否仍是 Demo 版本

结论：

```text
整体切片软件不是单纯 Demo；
OpenVDB 表面壳层纹理链路仍是实验 / demo / diagnostic path。
```

当前已有稳定基础能力：

```text
1. legacy slicer_cli；
2. RGBWSV package；
3. TIFF writer / reader；
4. RIP reader test；
5. OBJ/MTL/PNG 与 3MF 输入基础；
6. MaterialPolicy；
7. SupportShapePipeline；
8. Qt Debug UI 基础；
9. CI quick；
10. OpenVDB ON/OFF 构建分离。
```

但 OpenVDB 壳层纹理当前只服务于：

```text
surface_shell_texture_demo
surface_shell_real_model_demo
surface_shell_robustness_demo
report
preview
benchmark
```

尚未进入：

```text
production slicer_cli
production RGBWSV TIFF
production MaterialPolicy 默认链路
Qt UI production workflow
```

---

## 3. 09B-R2 已完成需求

### 3.1 真实模型 Golden

已完成：

```text
真实指甲 OBJ golden
真实指甲 3MF golden
multimaterial seam
thin wall
duplicate face
local reversed face
self-intersection candidate
```

真实模型结果：

```text
OBJ golden：
  triangles = 70262
  materials = 1
  textures = 1
  inside/shell/interior = 338713 / 112436 / 226277
  result = PASS, nonProduction=true

3MF golden：
  triangles = 75596
  materials = 3
  textures = 3
  inside/shell/interior = 373358 / 116234 / 257124
  result = PASS, nonProduction=true
```

### 3.2 Scale-aware Tolerance

已完成：

```text
MeshScaleTolerance
positionEpsilonMm
areaEpsilonMm2
tieEpsilonMm
selfIntersectionEpsilonMm
```

### 3.3 拓扑鲁棒性诊断

已完成：

```text
connected components
duplicate faces
opposite duplicates
local winding inconsistency
zero-volume components 字段
min edge / min area / max aspect ratio
thin feature warning
AABB broad-phase self-intersection candidate / sampled 统计
```

尚未完成：

```text
完整 narrow-phase triangle-triangle self-intersection
稳定 error/warning code
zero-volume 专用触发 fixture
```

### 3.4 Seam 与多材质策略

已完成策略：

```text
1. shell voxel 使用 BVH 找最近三角；
2. tie-break = distance → barycentric interior margin → triangle index；
3. 命中三角后只使用该 triangle 的 UV / material / texture；
4. 不跨 UV seam 平均；
5. 不跨 material seam 混色。
```

尚未完成：

```text
repeat/wrap texture fixture
repeat 模式边界行为验证
```

### 3.5 性能与内存基线

已完成 Release benchmark：

```text
bench_1k
bench_10k
bench_50k
```

当前统计：

```text
levelSetMs
bvhBuildMs
transferMs
peakEstimatedBytes
bvh nodes
tested triangles
```

尚未完成：

```text
processPeakWorkingSetBytes
100k fixture
OS 级内存峰值
```

### 3.6 回归验证

已执行：

```powershell
surface_shell_robustness_unit_tests
run_surface_shell_robustness_tests.ps1
run_surface_shell_robustness_tests.ps1 -RunMatrix
run_surface_shell_robustness_tests.ps1 -RunRealModels
run_surface_shell_real_model_tests.ps1
run_surface_shell_texture_tests.ps1
run_openvdb_smoke.ps1
run_surface_shell_benchmarks.ps1
cmake --build build --config Debug
run_ci_quick.ps1
```

---

## 4. 当前风险

| 风险 | 影响 | 处理阶段 |
|---|---|---|
| 自相交只有 AABB candidate | 不能作为生产拒绝依据 | 09B-R3 |
| warning/error 仍为字符串 | 自动化不稳定 | 09B-R3 |
| repeat/wrap 未覆盖 | 纹理边界策略不完整 | 09B-R3 |
| 无 OS 级 peak working set | 生产性能评估不足 | 09B-R3 |
| 真实模型 strict_closed 未通过 | 不能进入 production | 09B-R3 / 09P 前置 |
| 尚未接 production RGBWSV | 不能对外宣称 production shell texture | 09P-R1 |

---

## 5. 后续路线

推荐路线：

```text
09B-R2
→ 09B-R3：生产准入前的诊断与策略收口
→ 09P：OpenVDB production pipeline 接入设计
→ 09P-R1：experimental production path
→ 09P-R2：production hardening
→ 09C：SDF compensated varnish prototype
→ 09D：SDF support clearance / overhang diagnostics
→ 10：RIP / 设备 / 工艺联调
```

不建议：

```text
09B-R2 → 09P production 直接接入
09B-R2 → 直接写 production RGBWSV TIFF
09B-R2 → 直接替换 legacy texture path
```

---

## 6. 09B-R3 进入条件

09B-R3 可立即开始，因为 09B-R2 报告已经明确建议先进入 09B-R3。

09B-R3 的目标不是新增大功能，而是收口 R2 中暴露的生产准入问题：

```text
narrow-phase self-intersection
ValidationErrorCode / WarningCode
repeat/wrap texture fixture
Windows process peak working set
真实模型拓扑修复策略评估
```
