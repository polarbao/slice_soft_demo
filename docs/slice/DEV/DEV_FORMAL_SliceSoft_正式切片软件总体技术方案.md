# DEV_FORMAL_SliceSoft_正式切片软件总体技术方案

> 文档版本：v0.1
> 文档状态：Formal DEV / Current Architecture Source
> 生成日期：2026-06-30
> 当前阶段：Stage 12E-09C、09A-01/02 已完成；Stage 13 P0 设计和原子任务准备完成，13A-01..05/13B-01/02 已实现，下一任务 13B-03
> 适用项目：SliceSoft / UV 彩色多材料 3D 打印切片软件

---

## 1. 技术目标

正式技术目标是把现有 demo / experimental 资产整理为可持续演进的切片软件架构：

```text
1. 保留 legacy RGBWSV production path；
2. 将 OpenVDB/SDF surface shell 作为显式 experimental path；
3. 用 stable issue code 和 production admission 管住真实模型准入；
4. 用 service boundary 收束 geometry / texture / material / output；
5. 用 config schema / report schema / golden / CI 支撑长期维护；
6. 为下游 RIP 工程团队保留清晰输出契约，但本项目不实现 RIP、设备通信或喷头 bitstream。
```

---

## 2. 当前架构事实

### 2.1 已存在的核心 target

```text
slicer_core
slicer_cli
rip_reader_test
slicer_debug_ui
geometry_kernel_demo
surface_shell_texture_demo
surface_shell_real_model_demo
surface_shell_robustness_demo
unit test targets
```

### 2.2 当前主要模块

```text
config/
diagnostics/
geometry/
importers/
material/
materials/
output/
pipeline/
raster/
reports/
scene/
support/
system/
texture_image
tiff_io
rip_reader
slicer legacy flow
```

### 2.3 09P-R1 新增关键边界

```text
ProductionAdmissionPolicy
OpenVdbGeometryKernelService
SurfaceShellTextureService
MaterialChannelComposer
experimental.openvdbPipeline config
slicer_cli experimental diagnostic path
run_09p_experimental_pipeline_tests.ps1
```

---

## 3. 目标分层架构

正式项目建议稳定为 10 层：

```text
[1] Application Layer
    slicer_cli
    rip_reader_test
    slicer_debug_ui
    demo / test apps

[2] Config/Profile Layer
    SliceConfig
    ConfigSchema
    ConfigMigration
    NormalizedConfig
    MaterialProcessProfile
    ExperimentalOpenVdbPipelineConfig

[3] Import Layer
    OBJ / MTL
    STL
    3MF
    Texture resource loading

[4] Scene Layer
    SceneModel
    TriangleMeshData
    MaterialInfo
    TriangleTextureInfo

[5] Geometry Layer
    Legacy raster geometry
    OpenVDB level set
    SDF shell / interior
    Mesh topology diagnostics
    Nearest triangle query

[6] Feature Policy Layer
    TextureApplicationPolicy
    MaterialPolicy
    MaterialRoleMapping
    SupportPolicy
    VarnishGeometryPolicy
    ProductionAdmissionPolicy

[7] Pipeline Layer
    SlicePipeline
    Legacy pipeline
    OpenVDB experimental pipeline
    PipelineContext
    PipelineStepResult

[8] Output Layer
    MaterialChannelComposer
    RGBWSV package
    TIFF writer / reader
    manifest
    RIP reader

[9] Diagnostics Layer
    ValidationIssue
    ReportBase
    ReportSchema
    ReportWriter
    Preview
    Golden

[10] Engineering Layer
    CMake
    PowerShell scripts
    unit tests
    schema / golden / smoke / benchmark
    docs/slice + docs/codex_task + docs/archive
```

---

## 4. 依赖方向

允许依赖：

```text
apps -> public slicer_core APIs
importers -> scene
pipeline -> scene + geometry + materials + support + output + reports
geometry -> scene / mesh DTO / diagnostics
texture transfer -> geometry query + texture sampler + diagnostics
materials -> policy DTO / composition inputs
output -> RGBWSV DTO / TIFF / manifest
reports -> diagnostics / stats / config snapshot
UI -> process/report/package service, not slicer.cpp internals
```

禁止依赖：

```text
slicer_core -> Qt
importers -> TIFF writer
geometry -> material channel composition
texture transfer -> W/S/V policy decision
materials -> OpenVDB concrete type
support -> report file writing
reports -> business policy decisions
output -> topology admission decisions
experimental path -> implicit production package writing
```

---

## 5. 两条主 pipeline

### 5.1 Legacy production path

默认路径必须继续可用：

```text
SliceConfig
→ load model / scene
→ legacy raster / relief / support
→ texture/material policy
→ RGBWSV channel composition
→ TIFF package / manifest
→ reports
→ rip_reader_test
```

约束：

```text
默认走 legacy；
不依赖 OpenVDB；
保持 p0.rgbwsv.2；
保持当前样例和 regression；
OpenVDB 变更不得破坏此路径。
```

### 5.2 OpenVDB experimental path

当前 09P-R1 路径：

```text
SliceConfig / CLI flag
→ experimental.openvdbPipeline
→ ProductionAdmissionPolicy
→ OpenVdbGeometryKernelService
→ SurfaceShellTextureService
→ MaterialChannelComposer bridge
→ diagnostic/report
```

约束：

```text
显式开启；
默认不写 production package；
输出 nonProduction / productionAdmission；
OpenVDB 不可用时返回 OPENVDB_UNAVAILABLE；
warn_and_attempt 不得 productionAllowed。
```

### 5.3 OpenVDB production candidate path

目标候选路径，09P-R2/R3/R4 后才能逐步接近：

```text
NormalizedConfig
→ SceneModel
→ ProductionAdmissionPolicy strict_closed
→ OpenVdbGeometryKernelService
→ SurfaceShellTextureService
→ MaterialChannelComposer
→ RGBWSV package candidate
→ RIP reader compatibility
→ golden / report / UI / CI
```

此路径当前不是默认生产路径。

---

## 6. 核心模块职责

### 6.1 ProductionAdmissionPolicy

职责：

```text
输入 stable ValidationIssue
输出 AdmissionDecision
判断 productionAllowed / nonProduction / fail_fast / diagnostic_only
提供 blockerCodes / warningCodes / suggestedActions
```

关键规则：

```text
confirmed self-intersection => FailFast
non-manifold => strict blocker
duplicate/opposite duplicate => strict blocker
local winding inconsistency => strict blocker
OPENVDB_UNAVAILABLE => OpenVDB path blocker
warn_and_attempt => NonProductionOnly
repair_then_strict => 未实现 repair 前不得 ProductionAllowed
```

### 6.2 OpenVdbGeometryKernelService

职责：

```text
封装 OpenVDB level set 和 shell/interior 分类
输出 geometry stats
输出 ValidationIssue
USE_OPENVDB=OFF 时返回 OPENVDB_UNAVAILABLE
```

不负责：

```text
纹理采样
材料策略
RGBWSV 写出
UI 展示
```

### 6.3 SurfaceShellTextureService

职责：

```text
shell voxel -> nearest triangle
triangle -> UV/material/texture
sample texture or fallback
统计 sampled/fallback/missing/uv out of range
保持 seam 策略
```

seam 策略：

```text
命中哪个 triangle，就使用该 triangle 的 UV；
不跨 UV seam 平均；
命中 triangle 的 material 是唯一材料来源；
不跨 material seam 混色。
```

### 6.4 MaterialChannelComposer

职责：

```text
接收 model/interior/shell/support/white/varnish 中间结果
组合 in-memory RGBWSV result
固定 channel order = R G B W S V
明确 priority resolver
```

09P-R1 只建立 bridge。09P-R2 后可逐步接入 experimental golden，但仍不能绕过 production admission。

---

## 7. Config 技术路线

当前有：

```text
legacy SliceConfig
slicer.config.1 wrapper / migration
experimental.openvdbPipeline safe-off config
```

09P-R2 应补强：

```text
1. experimental.openvdbPipeline schema 文档；
2. admissionMode / failurePolicy 枚举校验；
3. writeProductionRgbwsv 与 admission gate 的关系；
4. OpenVDB unavailable diagnostic；
5. old config 兼容性测试；
6. UI 可读配置摘要。
```

建议未来配置分层：

```json
{
  "schema": "slicer.config.1",
  "input": {},
  "output": {},
  "geometry": {},
  "texture": {},
  "materials": {},
  "support": {},
  "experimental": {
    "openvdbPipeline": {}
  },
  "diagnostics": {}
}
```

---

## 8. Report 技术路线

当前已有多种 report，但 schema 还不统一。09P-R2 应把 experimental report 固化为明确 schema。

建议 report 基础字段：

```json
{
  "schema": "...",
  "source": {},
  "configSnapshot": {},
  "engine": {},
  "diagnostics": [],
  "productionAdmission": {},
  "stats": {},
  "warnings": [],
  "errors": [],
  "timings": {},
  "memory": {}
}
```

09P-R2 重点 schema：

```text
p0.experimental_openvdb_shell_cli_report.1
p0.surface_shell_texture_report.2
productionAdmission block
ValidationIssue block
OpenVDB status block
TextureTransferStats block
MaterialChannelComposer stats block
```

---

## 9. Testing / CI 技术路线

当前已有：

```text
run_ci_quick.ps1
run_09p_experimental_pipeline_tests.ps1
run_09p_cli_experimental_tests.ps1
run_openvdb_smoke.ps1
run_surface_shell_real_model_tests.ps1
run_surface_shell_texture_tests.ps1
unit tests
golden expected summaries
```

09P-R2 应形成：

```text
ci_legacy_off:
  USE_OPENVDB=OFF
  build
  ctest
  run_ci_quick
  legacy slicer_cli smoke

ci_openvdb_on:
  USE_OPENVDB=ON
  openvdb smoke
  geometry service unit
  surface shell tests
  real model diagnostic
  experimental CLI smoke

ci_benchmark_optional:
  Release benchmark
  non-blocking or manual
```

---

## 10. Qt Debug UI 技术路线

Qt UI 不能直接接 OpenVDB 内部算法。它应读取：

```text
config
package summary
manifest
reports
preview files
experimental OpenVDB report
productionAdmission block
ValidationIssue block
```

09P-R2 UI 最小目标：

```text
1. 能加载 experimental report；
2. 显示 OpenVDB availability；
3. 显示 productionAdmission status；
4. 显示 blockerCodes / warningCodes；
5. 显示 nonProduction；
6. 不直接触发 production package 写出。
```

---

## 11. 正式化改造步骤

正式化应按以下顺序推进：

```text
1. 文档入口和状态治理；
2. report schema hardening；
3. admission gate hardening；
4. service data contract hardening；
5. experimental golden / downstream output contract / texture fidelity compatibility 设计；
6. Qt UI report integration；
7. CI matrix；
8. production candidate decision；
9. mesh repair / admission gate 专项；
10. 09C / 09D / 10 / 11。
```

---

## 12. 技术风险

| 风险 | 控制方式 |
|---|---|
| OpenVDB 对复杂拓扑不稳定 | strict_closed + stable issue code + nonProduction |
| 真实 OBJ/3MF 被误认为 production-safe | ProductionAdmissionPolicy 阻断 |
| report 字段随意变化 | schema 文档 + schema tests |
| UI 直接依赖内部类型 | UI 只读 report/package |
| OpenVDB 成为强制依赖 | USE_OPENVDB=OFF 默认 |
| legacy path 被破坏 | run_ci_quick + legacy smoke |
| 性能不可控 | optional Release benchmark |

---

## 13. 09P-R2 技术结论

09P-R2 应定义为：

```text
OpenVDB experimental pipeline hardening 阶段。
```

它的目标是让 09P-R1 的边界变得稳定、可读、可测、可 UI 展示，而不是立即把真实 OBJ/3MF 写成 production RGBWSV。

09P-R2 完成后，应能回答：

```text
1. experimental report 是否稳定；
2. admission gate 是否足以阻断真实模型 blocker；
3. service boundary 是否足以支撑后续 production candidate；
4. Qt UI 是否能解释 OpenVDB experimental 输出；
5. 是否需要先做 mesh repair/admission gate 专项；
6. 是否可以进入 09P-R3。
```

---

## 14. Stage 13 模型场景技术方向

Stage 13 增加正式 scene 边界：

```text
ModelSource -> modelId/resourceScope；
ModelInstance -> instanceId/transform/admission；
MultiModelScene -> buildVolume/layout/revision；
GridLayoutPolicy -> 11x2/edge clearance；
MultiModelSliceOrchestrator -> 逐实例生产层到全局 raster；
SceneLayerComposer -> 单层 RGBWSV 合成；
TiffLayerSource/MaterialPreviewComposer -> TIFF 原生 UI 预览。
```

依赖方向：

```text
Qt scene editor -> core public DTO；
layout -> scene/bounds；
pipeline -> transformed instances/material/support/raster/output；
preview UI -> TIFF reader/domain DTO；
core 不依赖 Qt；
UI 不访问 slicer.cpp 临时结构。
```

Stage 13 保持：

```text
p0.rgbwsv.2；
R G B W S V；
uint8；
black_is_print；
Legacy 默认；
Global 显式 opt-in；
strict geometry admission；
联合切片失败不 silent fallback。
```

中期 3D 显示后端必须通过 VTK/Qt3D/QOpenGLWidget 技术 Spike 后决策，不直接把大型依赖加入默认构建。
