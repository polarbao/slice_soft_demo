# DEV_09P_R2_ServiceDataContract

> 文档版本：v0.1
> 文档状态：Formal DEV / Stage 09P-R2-5
> 生成日期：2026-07-01
> 适用范围：experimental OpenVDB surface-shell pipeline hardening

---

## 1. 目的

本文件固定 09P-R2 experimental OpenVDB 路线的服务数据契约，说明各服务之间传递什么、哪些字段稳定、哪些字段可为空、`ValidationIssue` 如何传播，以及 report/UI/CI 应依赖哪些边界。

本文件不新增 OpenVDB 功能，不默认启用 OpenVDB，不替代 legacy `slicer_cli` production path，不允许 experimental path 写真实 OBJ/3MF production RGBWSV TIFF。

---

## 2. 依赖方向

当前服务链路：

```text
TriangleMeshData / SceneModel
→ OpenVdbGeometryKernelService
→ SurfaceShellTextureService
→ MaterialChannelComposer
→ ProductionAdmissionPolicy
→ ReportWriter / CLI report / UI reader
```

依赖边界：

```text
slicer_core 不依赖 Qt；
UI 只读取 report/package，不依赖 OpenVDB 类型；
OpenVDB 仅在 optional adapter/service 内部隔离；
MaterialChannelComposer 只生成 in-memory RGBWSV buffer，不写 TIFF/manifest；
ReportWriter 只负责 JSON 写出，不判断 productionAllowed。
```

---

## 3. ValidationIssue Contract

所有服务应使用稳定 `ValidationIssue` 传播错误和 warning。

| 字段 | 稳定性 | 说明 |
|---|---|---|
| `code` | 必须稳定 | UI、schema test、admission gate 和 CI 依赖该字段 |
| `severity` | 必须稳定 | `info` / `warning` / `error` |
| `message` | 可调整 | 人类可读，不作为机器契约 |
| `context` | 可扩展 | 可包含 count、阈值、路径、stats 摘要 |

关键 code：

```text
GEOMETRY_KERNEL_MESH_MISSING
OPENVDB_UNAVAILABLE
OPENVDB_LEVEL_SET_FAILED
OPENVDB_SURFACE_SHELL_FAILED
SURFACE_TEXTURE_INPUT_MISSING
SURFACE_TEXTURE_TRANSFER_FAILED
TEXTURE_MISSING
TEXTURE_UV_MISSING
TEXTURE_UV_OUT_OF_RANGE
TEXTURE_REPEAT_SAMPLED
TEXTURE_TRANSFER_DISTANCE_EXCEEDED
SURFACE_TEXTURE_QUERY_FAILED
```

拓扑 production gate code 以 `DOC_MATRIX_09P_R2_topology_admission_gate.md` 为准。

---

## 4. OpenVdbGeometryKernelService

代码入口：

```text
src/slicer_core/geometry/GeometryKernelService.h
src/slicer_core/geometry/OpenVdbGeometryKernelService.h
src/slicer_core/geometry/OpenVdbGeometryKernelService.cpp
```

### 4.1 Input DTO

| 字段 | 类型 | 是否必需 | 说明 |
|---|---|---:|---|
| `GeometryKernelRequest::mesh` | `const TriangleMeshData*` | 是 | 为空时返回 `GEOMETRY_KERNEL_MESH_MISSING` |
| `level_set_options` | `OpenVdbLevelSetOptions` | 是 | level set 构建参数 |
| `shell_options` | `OpenVdbSurfaceShellOptions` | 是 | shell/interior 分类参数 |

### 4.2 Output DTO

| 字段 | 类型 | 稳定性 | 说明 |
|---|---|---|---|
| `ok` | bool | 稳定 | level set 和 shell 分类均成功才为 true |
| `available` | bool | 稳定 | OpenVDB 编译与运行时是否可用 |
| `status` | `OpenVdbStatus` | 稳定扩展 | 编译状态、runtime、version、grid 信息 |
| `level_set` | `OpenVdbLevelSetResult` | 可扩展 | active voxels、memory、bounds、error |
| `surface_shell` | `OpenVdbSurfaceShellResult` | 可扩展 | shell/interior/outside 统计与 mask |
| `issues` | `vector<ValidationIssue>` | 稳定 | OpenVDB 或输入问题必须在此返回 |

### 4.3 Error Strategy

```text
mesh == nullptr → ok=false, GEOMETRY_KERNEL_MESH_MISSING
OpenVDB unavailable → ok=false, OPENVDB_UNAVAILABLE
level set failed → ok=false, OPENVDB_LEVEL_SET_FAILED
surface shell failed → ok=false, OPENVDB_SURFACE_SHELL_FAILED
```

服务不得因为 `USE_OPENVDB=OFF` 抛异常；必须返回稳定 issue，供 admission/report/UI 使用。

---

## 5. SurfaceShellTextureService

代码入口：

```text
src/slicer_core/materials/texture_application/SurfaceShellTextureService.h
src/slicer_core/materials/texture_application/SurfaceShellTextureService.cpp
```

### 5.1 Input DTO

| 字段 | 类型 | 是否必需 | 说明 |
|---|---|---:|---|
| `adapted_mesh` | `const AdaptedTriangleMesh*` | 是 | 提供 triangle/material/UV 映射 |
| `level_set` | `const OpenVdbLevelSetResult*` | 是 | 提供 OpenVDB level set 上下文 |
| `shell` | `const OpenVdbSurfaceShellResult*` | 是 | 提供 shell voxel 分类 |
| `transfer_options` | `SurfaceTextureTransferOptions` | 是 | sampler、UV addressing、distance threshold |

### 5.2 Output DTO

| 字段 | 类型 | 稳定性 | 说明 |
|---|---|---|---|
| `ok` | bool | 稳定 | transfer error 为空时为 true |
| `transfer` | `SurfaceTextureTransferResult` | 可扩展 | 包含 RGB/texture transfer stats |
| `preview_info` | `SurfaceShellTexturePreviewInfo` | 稳定扩展 | width/height/depth/shell voxels/unique color |
| `issues` | `vector<ValidationIssue>` | 稳定 | transfer 失败和 fallback stats 映射为 issue |

### 5.3 Issue Propagation

| 条件 | Issue code | severity |
|---|---|---|
| 输入指针缺失 | `SURFACE_TEXTURE_INPUT_MISSING` | error |
| transfer error 非空 | `SURFACE_TEXTURE_TRANSFER_FAILED` | error |
| texture 缺失 | `TEXTURE_MISSING` | warning |
| UV 缺失 | `TEXTURE_UV_MISSING` | warning |
| UV 超出范围 | `TEXTURE_UV_OUT_OF_RANGE` | warning |
| repeat 采样 | `TEXTURE_REPEAT_SAMPLED` | info |
| transfer distance 超阈值 | `TEXTURE_TRANSFER_DISTANCE_EXCEEDED` | warning |
| nearest triangle 查询失败 | `SURFACE_TEXTURE_QUERY_FAILED` | warning |

`SurfaceShellTextureService` 不直接决定 productionAllowed，只提供 stable issues 和 texture fidelity 统计给 report/admission。

---

## 6. MaterialChannelComposer

代码入口：

```text
src/slicer_core/material/MaterialChannelComposer.h
src/slicer_core/material/MaterialChannelComposer.cpp
```

### 6.1 Input DTO

| 字段 | 类型 | 是否必需 | 说明 |
|---|---|---:|---|
| `width` / `height` | int | 是 | 必须大于 0 |
| `support_mask` | `vector<uint8_t>` | 可空 | 非空时尺寸必须等于 `width * height` |
| `model_mask` | `vector<uint8_t>` | 可空 | 模型区域 mask |
| `surface_shell_mask` | `vector<uint8_t>` | 可空 | shell RGB 覆盖区域 |
| `white_mask` | `vector<uint8_t>` | 可空 | W 通道区域 |
| `varnish_mask` | `vector<uint8_t>` | 可空 | V 通道区域 |
| `surface_rgb` | `vector<array<uint8_t, 3>>` | 可空 | 非空时尺寸必须等于 `width * height` |
| `model_rgb` | `array<uint8_t, 3>` | 是 | base/model RGB fallback |
| `support_value` / `white_value` / `varnish_value` | uint8 | 是 | 当前 00B 极性下打印值通常为 0 |

### 6.2 Output DTO

| 字段 | 类型 | 稳定性 | 说明 |
|---|---|---|---|
| `width` / `height` | int | 稳定 | 与输入同步 |
| `channel_order` | `array<string, 6>` | 必须稳定 | 固定 `R G B W S V` |
| `channels` | `vector<uint8_t>` | 稳定 | in-memory RGBWSV buffer，不写生产 TIFF |
| `stats` | `MaterialChannelComposerStats` | 稳定扩展 | empty/support/model/shell RGB/W/V/conflict 计数 |
| `priority_resolver` | string | 稳定说明 | 当前优先级说明 |
| `error` | string | 可调整 | 输入尺寸错误时返回，不抛异常 |

### 6.3 Stable Rules

```text
channel count = 6
channelOrder = R G B W S V
emptyValue = 255
support/model conflict 时 model 优先，S 恢复 255
surface shell RGB 不直接清除 S/V
invalid input 返回 error，channels 可为空
```

`MaterialChannelComposer` 不写 TIFF、不写 manifest、不修改 `p0.rgbwsv.2`。

---

## 7. ProductionAdmissionPolicy

代码入口：

```text
src/slicer_core/diagnostics/ProductionAdmissionPolicy.h
src/slicer_core/diagnostics/ProductionAdmissionPolicy.cpp
```

### 7.1 Input DTO

| 字段 | 类型 | 是否必需 | 说明 |
|---|---|---:|---|
| `issues` | `vector<ValidationIssue>` | 是 | 由 geometry、texture、topology、OpenVDB 等服务汇总 |
| `AdmissionMode` | enum | 是 | `StrictClosed` / `WarnAndAttempt` / `DiagnosticOnly` / `RepairThenStrict` |

### 7.2 Output DTO

| 字段 | 稳定性 | 说明 |
|---|---|---|
| `status` | 稳定 | `production_allowed` / `non_production_only` / `diagnostic_only` / `fail_fast` |
| `productionAllowed` | 稳定 | production gate 布尔值 |
| `nonProduction` | 稳定 | 是否只能作为 non-production 输出 |
| `blockerCodes` | 稳定 | 阻断 production 的 code |
| `warningCodes` | 稳定 | warning/非阻断 code |
| `suggestedActions` | 可调整 | 人类可读建议 |

### 7.3 Stable Rules

```text
MESH_SELF_INTERSECTION_CONFIRMED + strict_closed → fail_fast
topology blocker + strict_closed → non_production_only
warn_and_attempt → non_production_only
diagnostic_only → diagnostic_only
repair_then_strict → 当前 non_production_only
```

矩阵以 `DOC_MATRIX_09P_R2_topology_admission_gate.md` 为准。

---

## 8. ReportWriter

代码入口：

```text
src/slicer_core/reports/ReportWriter.h
src/slicer_core/reports/ReportWriter.cpp
```

### 8.1 Input DTO

| 字段 | 类型 | 是否必需 | 说明 |
|---|---|---:|---|
| `path` | `filesystem::path` | 是 | 目标 JSON report 路径 |
| `value` | `Json` | 是 | 上游已构造完成的 report payload |

### 8.2 Output / Error Strategy

```text
写入成功：生成 pretty JSON，末尾换行；
写入失败：抛 runtime_error；
不修改 admission；
不补字段；
不判断 production safety。
```

ReportWriter 的职责是 IO 边界，不是 schema validator。schema 校验由脚本和 golden contract 承担。

---

## 9. Timing / Memory / Stats Contract

当前 09P-R2 report 应继续使用容器字段承载 timing/memory/stats：

| 类别 | 来源 | 稳定要求 |
|---|---|---|
| `timing` / `performance` | CLI、real-model prototype、service caller | 字段可扩展，单位必须写入字段名或文档 |
| `memory` | OpenVDB result、process stats、texture cache | 可为空或 unavailable，但不能伪造 |
| `stats` | shell、texture transfer、composer、admission summary | 用于 UI/CI/golden 趋势判断 |
| `issues` | 各服务 `ValidationIssue` 汇总 | code/severity 稳定 |

服务本身不必都直接采集 timing/memory；调用方可以在 pipeline/report 层统一采集，但 report 字段必须允许缺省并说明原因。

---

## 10. Allowed Empty Fields

| 字段 | 允许为空条件 |
|---|---|
| `GeometryKernelResult::level_set` | OpenVDB unavailable、mesh missing 或 level set failed |
| `GeometryKernelResult::surface_shell` | level set 未生成或 shell 分类失败 |
| `SurfaceShellTextureServiceResult::transfer` | 输入缺失或 transfer failed |
| `SurfaceShellTexturePreviewInfo` | 输入缺失或 shell 为空 |
| `MaterialChannelComposerResult::channels` | 输入尺寸非法 |
| `outputContract.perLayerStats` | experimental CLI 不写 production package 时可为空 |
| `timing` / `memory` 子字段 | 当前环境无法采集时可为空或标记 unavailable |

为空时必须通过 `issues`、`error`、`available`、`ok` 或 report 字段解释原因。

---

## 11. Must-Stay-Stable Fields

09P-R2 后续任务不得随意改变：

```text
p0.experimental_openvdb_shell_cli_report.1
p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
productionPackageWritten = false for experimental CLI
writeProductionRgbwsv = false for 09P-R2 experimental CLI
warn_and_attempt productionAllowed = false
repair_then_strict productionAllowed = false until explicit repair + strict recheck
```

---

## 12. Validation

R2-5 变更是服务契约文档任务，但必须保持当前构建与测试不退化：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

