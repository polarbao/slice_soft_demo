# REPORT_09P_R2_OpenVDB实验生产管线Hardening当前状态

> 文档版本：v0.1
> 文档状态：Stage Report / 09P-R2
> 生成日期：2026-07-01
> 分支：`spike/09P-openvdb-experimental-pipeline`

---

## 1. 当前分支与基线

当前分支：

```text
spike/09P-openvdb-experimental-pipeline
```

报告生成前 HEAD：

```text
f59bd9c build(09P): 建立 R2 CI matrix 脚本
```

本阶段从 09P-R1 experimental production-pipeline 接入后的 hardening 继续推进，目标不是新增生产切片能力，而是让 experimental OpenVDB surface-shell path 可解释、可回归、可准入判断、可 UI 展示。

---

## 2. 已完成任务

| Task | 状态 | 主要提交/产物 |
|---|---|---|
| 09P-R2-0 文档状态同步 | 已完成 | `945f097` / `cd8742b` |
| 09P-R2-1 阶段 PRD/DEV/DEMO/CODEX_PROMPT | 已完成 | `1670110` / `a6c5765` |
| 09P-R2-2 experimental report schema | 已完成 | `aa5b3be` |
| 提交规范沉淀 | 已完成 | `2a4b08a` |
| 09P-R2-3 topology admission gate | 已完成 | `e6dc0bd` |
| 09P-R2-4 mesh repair 前置判断 | 已完成 | `3b573ad` |
| 09P-R2-5 service data contract | 已完成 | `bb81fdd` |
| 09P-R2-6 experimental golden / output contract / texture fidelity | 已完成 | `2576307` |
| 09P-R2-7 Qt UI 读取 experimental report | 已完成 | `b792ad1` |
| 09P-R2-8 OpenVDB OFF / ON CI matrix | 已完成 | `f59bd9c` |
| 09P-R2-9 本报告 | 本轮完成 | `REPORT_09P_R2_OpenVDB实验生产管线Hardening当前状态.md` |

---

## 3. 新增/修改文件

核心新增文件：

```text
docs/slice/DOC/DOC_SCHEMA_09P_R2_experimental_openvdb_shell_report.md
docs/slice/DOC/DOC_MATRIX_09P_R2_topology_admission_gate.md
docs/slice/DOC/DOC_DECISION_09P_R2_mesh_repair_admission_gate.md
docs/slice/DEV/DEV_09P_R2_ServiceDataContract.md
docs/slice/DEMO/DEMO_09P_R2_experimental_golden_rip_compatibility.md
docs/slice/DEMO/DEMO_09P_R2_CI_Matrix验证方案.md
docs/slice/REPORT/REPORT_09P_R2_OpenVDB实验生产管线Hardening当前状态.md
scripts/run_09p_schema_tests.ps1
scripts/run_09p_golden_tests.ps1
scripts/run_09p_r2_ci_matrix.ps1
tests/golden/expected/09p_experimental_report_schema.json
tests/golden/expected/09p_experimental_output_contract.json
```

核心修改文件：

```text
apps/slicer_cli/main.cpp
apps/slicer_debug_ui/main.cpp
apps/slicer_debug_ui/services/ReportLoader.cpp
apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp
apps/slicer_debug_ui/services/UiSmokeTestRunner.h
tests/unit/production_admission_policy/main.cpp
docs/slice/README.md
docs/slice/DOC/README.md
docs/slice/DEV/README.md
docs/slice/DEMO/README.md
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md
```

---

## 4. 当前实现状态

### 4.1 Report Schema

已固化：

```text
p0.experimental_openvdb_shell_cli_report.1
```

已覆盖字段：

```text
input
configSnapshot
openvdb
surfaceShell
diagnostics
productionAdmission
textureTransfer
materialComposer
outputContract
legacyPath
timing
memory
stats
```

experimental CLI 安全不变量：

```text
legacyPathExecuted = false
productionPackageWritten = false
writeProductionRgbwsv = false
productionAdmission.productionAllowed = false
productionAdmission.nonProduction = true
```

### 4.2 Admission Gate

已测试并文档化四种 mode：

```text
strict_closed
warn_and_attempt
diagnostic_only
repair_then_strict
```

已覆盖 blocker：

```text
MESH_BOUNDARY_EDGES
MESH_SELF_INTERSECTION_CONFIRMED
MESH_NON_MANIFOLD_EDGES
MESH_DUPLICATE_FACES
MESH_OPPOSITE_DUPLICATE_FACES
MESH_LOCAL_WINDING_INCONSISTENCY
OPENVDB_UNAVAILABLE
OPENVDB_LEVEL_SET_FAILED
```

关键规则：

```text
confirmed self-intersection => fail_fast
warn_and_attempt => non_production_only
repair_then_strict => 当前 non_production_only
```

### 4.3 Mesh Repair 前置判断

已完成决策，不实现自动 repair。

当前结论：

```text
confirmed self-intersection 必须 reject / fail_fast
OpenVDB unavailable / level set failed 不是 mesh repair 问题
duplicate faces 可作为未来 repair 候选
opposite duplicate / local winding / boundary / non-manifold 只能 conditional repair
repair 后必须重新 topology diagnostics、OpenVDB 检查和 strict_closed admission
repair 前后必须记录 hash 和 repair report
```

### 4.4 Service Data Contract

已定义以下服务的数据契约：

```text
OpenVdbGeometryKernelService
SurfaceShellTextureService
MaterialChannelComposer
ProductionAdmissionPolicy
ReportWriter
```

契约内容包括：

```text
输入 DTO
输出 DTO
ValidationIssue 传播
timing / memory / stats
允许为空字段
必须稳定字段
```

### 4.5 Golden / Downstream Compatibility

已新增：

```text
tests/golden/expected/09p_experimental_output_contract.json
scripts/run_09p_golden_tests.ps1
```

明确：

```text
report golden / diagnostic golden / output contract golden / texture fidelity golden
downstream compatibility 不等于 production-safe
memory/timing/stats 只做趋势字段
```

### 4.6 Qt UI

Qt Debug UI 已能读取 `p0.experimental_openvdb_shell_cli_report.1` 并展示：

```text
OpenVDB availability
productionAdmission.status
productionAllowed
nonProduction
blockerCodes
warningCodes
legacyPathExecuted
productionPackageWritten
```

UI 只读取 JSON report，不依赖 OpenVDB 内部类型，不触发 production package。

### 4.7 CI Matrix

已新增：

```text
scripts/run_09p_r2_ci_matrix.ps1
```

默认 lane：

```text
OpenVDB OFF/default：build + ctest + run_ci_quick + 09P CLI smoke + schema + golden
```

显式 lane：

```text
OpenVDB ON：-RunOpenVdbOn -OpenVdbBuildDir <dir>
Benchmark：-RunBenchmarks -OpenVdbBuildDir <dir>
```

---

## 5. 实际验证命令

本阶段实际运行并通过：

```powershell
cmake --build build --config Debug --target production_admission_policy_unit_tests
.\build\Debug\production_admission_policy_unit_tests.exe
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Debug
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_golden_tests.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_experimental_pipeline_tests.ps1
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_r2_ci_matrix.ps1
git diff --check
git diff --cached --check
```

验证结果摘要：

```text
production_admission_policy_unit_tests 通过
ctest 5/5 通过
Debug build 通过
slicer_debug_ui --self-test 通过：PASS startup / PASS experimental-report-summary
run_09p_golden_tests.ps1 通过
run_09p_experimental_pipeline_tests.ps1 通过
run_09p_r2_ci_matrix.ps1 默认 OFF lane 通过
```

说明：

```text
run_ci_quick / CI matrix 会重新生成 tracked 3MF 样例，本轮已在提交前恢复这些验证副作用。
git diff --check / --cached --check 仅出现 Windows CRLF 转换提示。
```

---

## 6. 未运行验证及原因

未运行：

```text
OpenVDB ON lane
Benchmark Release lane
真实 OpenVDB ON surface shell / real-model 全量验证
```

原因：

```text
09P-R2 默认轨道必须保持 OpenVDB OFF；
本轮未传入 -RunOpenVdbOn -OpenVdbBuildDir；
Benchmark 为手动/可选轨道，不应阻塞默认 Debug hardening；
OpenVDB ON 仍建议使用独立无空格依赖根，例如 D:\vcpkg-openvdb。
```

---

## 7. Production 禁止事项保持情况

本阶段保持：

```text
未默认启用 OpenVDB
未让 OpenVDB 成为强制依赖
未替代 legacy slicer_cli production path
未从 experimental path 写真实 OBJ/3MF production RGBWSV TIFF
未修改 p0.rgbwsv.2
未修改 channelOrder = R G B W S V
未修改 bitDepth = 8
未修改 polarity = black_is_print
未把 warn_and_attempt 视为 production-safe
未实现自动 mesh repair
未实现 RIP 半色调、设备通信或喷头 bitstream
```

---

## 8. 仍不可 Production-Safe 的输入

以下输入/状态仍不可 production-safe：

```text
confirmed self-intersection
boundary edges
non-manifold edges
duplicate faces
opposite duplicate faces
local winding inconsistency
OpenVDB unavailable
OpenVDB level set failed
repair_then_strict 未实现 repair 的模型
warn_and_attempt 输出
只通过 downstream compatibility / golden 的 experimental report
```

真实复杂 OBJ/3MF 即使能生成 experimental diagnostic report，也仍需通过 strict admission 或未来 explicit repair_then_strict + 复诊断，才可进入 production candidate 判断。

---

## 9. 是否进入 09P-R3

建议可以进入 09P-R3，但进入条件应限定为：

```text
09P-R3 做 UI/report/profile 工程化；
不把 experimental OpenVDB path 升级为 production path；
不默认启用 OpenVDB；
不写真实 production RGBWSV；
继续通过 report/schema/admission gate 展示状态。
```

---

## 10. 是否需要 Mesh Repair / Admission Gate 专项

需要保留专项。

原因：

```text
真实模型 topology blocker 仍会阻断 strict admission；
duplicate / winding / non-manifold 等 repair 可行性尚未实现；
repair_then_strict 当前只能 nonProduction；
生产候选前必须有 repair report、pre/post hash 和复诊断测试。
```

建议：

```text
09P-R3 可继续 UI/report/profile；
mesh repair/admission gate 专项可以作为并行或 09P-R4 前置；
若后续真实模型验收继续被 blocker 阻断，应优先启动 repair 专项。
```
