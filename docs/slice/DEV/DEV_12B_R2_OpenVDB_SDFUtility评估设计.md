# DEV_12B_R2 OpenVDB SDF Utility 评估设计

> 文档版本：v0.1
> 文档状态：DEV / Stage 12B-R2
> 生成日期：2026-07-08
> 对应 PRD：PRD_12B_R2_OpenVDB_SDFUtility定位.md

## 1. Goal

12B-R2 的技术目标是把 OpenVDB 从“候选生产切片引擎”重新定位为“可选 SDF utility 模块”，并用工程化证据判断它是否适合服务以下局部能力：

```text
outer varnish shell offset / thickness diagnostic；
clearance / distance diagnostic；
complex topology diagnostic；
material closure gap analysis assist。
```

R2 不实现 production engine replacement，不改变 legacy output composer。

## 2. Current Code Reality

当前代码已有以下 OpenVDB / surface-shell 相关入口：

```text
CMake option USE_OPENVDB，默认 OFF；
src/slicer_core/geometry/OpenVdbSurfaceShell.*；
src/slicer_core/materials/texture_application/SurfaceShellTextureService.*；
src/slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.*；
apps/surface_shell_texture_demo；
apps/surface_shell_real_model_demo；
apps/surface_shell_robustness_demo；
scripts/run_openvdb_smoke.ps1；
scripts/run_09p_experimental_pipeline_tests.ps1；
scripts/run_surface_shell_texture_tests.ps1；
scripts/run_surface_shell_robustness_tests.ps1。
```

当前 production path：

```text
apps/slicer_cli --config <config>
src/slicer_core::run_slicer(...)
legacy RGBWSV package writer
```

R2 不允许让 OpenVDB utility 反向依赖或替换上述 production path。

## 3. Architecture Boundary

推荐边界：

```text
OpenVdbSdfUtilityProbe
  只负责读取 SceneModel / Mesh DTO 和 utility config，输出 SDF utility report DTO。

OpenVdbSdfUtilityReport
  记录 utility 能力、可用性、voxel 参数、统计和 warnings/blockers。

LegacyProductionComposer
  不读取 OpenVDB internal grid，不依赖 OpenVDB types。

Qt Debug UI
  只读取 report JSON，不访问 OpenVDB C++ 类型。
```

依赖方向：

```text
OpenVDB utility -> geometry / scene / diagnostics
reports -> utility DTO
UI -> report JSON
production output -> 不依赖 OpenVDB utility
```

禁止：

```text
slicer_core public API 暴露 OpenVDB types；
UI 直接 include OpenVDB header；
OpenVDB utility 写 production TIFF；
OpenVDB unavailable 时阻断 legacy build。
```

## 4. R2 Report Schema 草案

R2 建议输出独立 utility report：

```text
schema = slicesoft.openvdb_sdf_utility.12b_r2.1
```

正式 schema 文档：

```text
docs/slice/DOC/DOC_SCHEMA_12B_R2_OpenVDBSdfUtilityReport.md
```

根结构：

```json
{
  "schema": "slicesoft.openvdb_sdf_utility.12b_r2.1",
  "generatedAt": "2026-07-08T00:00:00+08:00",
  "build": {
    "useOpenVdb": true,
    "buildType": "Release",
    "openVdbAvailable": true
  },
  "input": {
    "modelPath": "samples/models/openvdb/...",
    "format": "obj",
    "admissionMode": "strict_closed"
  },
  "utilities": {
    "outerVarnishShell": {},
    "clearance": {},
    "topology": {},
    "materialClosureAssist": {}
  },
  "decision": {
    "openVdbRole": "sdf_utility_candidate",
    "productionReplacementAllowed": false,
    "recommendedNextStep": "keep_experimental"
  },
  "issues": []
}
```

Utility item 规则：

```text
available：当前构建和输入是否可运行；
executed：本次是否执行；
status：pass / fail / unavailable / blocked / skipped；
promoteDecision：promote / keep_experimental / reject / not_evaluated；
metrics：按 utility 输出统计；
blockers：不可推进原因。
```

## 5. Capability Matrix

R2 应维护以下矩阵：

| Utility | 输入要求 | 输出 | 生产影响 | 初始状态 |
|---|---|---|---|---|
| OuterVarnishShellOffset | strict closed 或可诊断 mesh | shell candidate metrics | 不改生产 TIFF | candidate |
| ClearanceDistance | SDF grid 可构建 | min/max/near-surface distance | diagnostic only | candidate |
| TopologyDiagnostic | mesh diagnostics | blockers/warnings/admission | gate only | existing |
| MaterialClosureAssist | semantic masks + optional SDF | gap assist metrics | 不单独判 PASS | research |

## 6. Task Design

### R2-00 文档准入和阶段启动

内容：

```text
补齐 PRD / DEV / DEMO / TASKS / CODEX_PROMPT；
更新 docs/slice 和 docs/codex_task 入口；
输出 R2 启动状态报告。
```

### R2-01 当前 OpenVDB utility 代码盘点

内容：

```text
列出当前 OpenVDB apps/scripts/core APIs；
判断哪些已经可复用为 utility；
判断哪些仍是 prototype/demo only。
```

输出：

```text
docs/slice/DOC/DOC_AUDIT_12B_R2_OpenVDB_SDFUtility代码盘点.md
```

### R2-02 Utility Report Schema 固化

内容：

```text
新增 slicesoft.openvdb_sdf_utility.12b_r2.1 schema；
明确 unavailable / blocked / executed / promoteDecision 字段。
```

### R2-03 OpenVDB OFF 默认构建保护

内容：

```text
验证 USE_OPENVDB=OFF 下 slicer_cli / slicer_debug_ui / 12B benchmark 不受影响；
OpenVDB utility 只输出 unavailable，不影响 legacy。
```

### R2-04 OpenVDB ON utility smoke

内容：

```text
在 build-openvdb-09p 或明确 OpenVDB ON build 中运行 smoke；
记录 OpenVDB 可用性、版本、activeVoxels。
```

### R2-05 四类 utility 评估矩阵

内容：

```text
outer varnish / clearance / topology / material closure assist 分别给出 promoteDecision；
禁止在矩阵完成前写 production package。
```

### R2-07 生成 R2 状态报告

内容：

```text
生成 REPORT_12B_R2_OpenVDB_SDFUtility当前状态.md；
明确是否需要 12B-R2-followup 或转回 legacy 性能优化。
```

## 7. Validation

文档级验证：

```powershell
占位标记扫描：目标为 R2 PRD / DEV / DEMO / TASKS / CODEX_PROMPT
git diff --check
```

默认 OFF 验证：

```powershell
cmake --build build --config Debug --target slicer_cli slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
powershell -ExecutionPolicy Bypass -File .\scripts\run_12b_core_benchmark.ps1 -Engine legacy -BuildType Release -NoImageWrite -Output output\benchmarks\core_benchmark_12b_r2_off_guard.json
```

OpenVDB ON 验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
```

说明：

```text
ON lane 只有在本机 OpenVDB build 已配置时运行；
ON lane 失败不能阻断 legacy OFF 默认轨道，但必须记录为 R2 blocker。
```

## 8. Risks

| 风险 | 影响 | 缓解 |
|---|---|---|
| 把 utility 误接入 production composer | 破坏生产输出 | R2 禁止写 production TIFF |
| OpenVDB ON 环境不稳定 | 验证不可复现 | OFF 必须稳定，ON 作为可选 lane |
| 真实模型 topology blocker 多 | utility 难以推广 | 保留 diagnostic，不绕过 strict blocker |
| SDF 指标与像素语义不一致 | 错误判定材料闭环 | 12D 仍以 RGBWSV/semantic masks 为真源 |

## 9. Rollback

R2 任意改动必须可回退：

```text
USE_OPENVDB=OFF 默认构建保持可用；
utility report 缺失不影响 legacy package；
失败时不写 production output；
R2 失败时保留 R0/R1 benchmark 结论，OpenVDB 继续停留 experimental。
```
