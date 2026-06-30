# REPORT_09P_R1_OpenVDB表面壳层纹理实验生产管线接入当前状态

> 生成日期：2026-06-30
> 当前分支：`spike/09P-openvdb-experimental-pipeline`
> 当前基线：`spike/09B-R3-shell-production-readiness`
> 阶段结论：09P-R1 已建立 experimental OpenVDB pipeline 接入边界，但未替代 legacy production path。

## 1. 当前分支与基线

当前工作位于：

```text
spike/09P-openvdb-experimental-pipeline
```

09P-R1 的基线来自：

```text
spike/09B-R3-shell-production-readiness
```

09P-R1 的定位是：

```text
feature flag / experimental path / diagnostic report / service abstraction
```

不属于本阶段完成范围：

```text
OpenVDB 默认启用
替代 legacy slicer_cli production path
真实 OBJ/3MF 直接写 production RGBWSV TIFF
修改 p0.rgbwsv.2
修改 RGBWSV 通道顺序、uint8 位深或 black_is_print 极性
将 warn_and_attempt 输出视为 production-safe
```

## 2. 已完成任务列表

已完成 09P-R1 Task 01-10：

```text
Task 01：修正文档中的当前阶段基线。
Task 02：新增 09P 阶段文档骨架。
Task 03：新增 ProductionAdmissionPolicy 模块。
Task 04：把 admission decision 接入 R3 report/diagnostic 层，但未接 slicer_cli production。
Task 05：新增 experimental.openvdbPipeline 配置字段，默认关闭。
Task 06：新增 OpenVdbGeometryKernelService 抽象层。
Task 07：新增 SurfaceShellTextureService 抽象层。
Task 08：新增 MaterialChannelComposer bridge 的最小实现。
Task 09：给 slicer_cli 增加 guarded experimental flag，只输出 diagnostic/report。
Task 10：新增 09P experimental pipeline 验证脚本。
```

本报告对应 Task 11。

## 3. 新增模块列表

新增 production admission：

```text
src/slicer_core/diagnostics/ProductionAdmissionPolicy.h
src/slicer_core/diagnostics/ProductionAdmissionPolicy.cpp
tests/unit/production_admission_policy/main.cpp
```

新增 experimental config 测试：

```text
tests/unit/experimental_config/main.cpp
samples/configs/openvdb/experimental_openvdb_pipeline_disabled.json
```

新增 OpenVDB geometry service：

```text
src/slicer_core/geometry/GeometryKernelService.h
src/slicer_core/geometry/OpenVdbGeometryKernelService.h
src/slicer_core/geometry/OpenVdbGeometryKernelService.cpp
tests/unit/geometry_kernel_service/main.cpp
```

新增 surface shell texture service：

```text
src/slicer_core/materials/texture_application/SurfaceShellTextureService.h
src/slicer_core/materials/texture_application/SurfaceShellTextureService.cpp
tests/unit/surface_shell_texture_service/main.cpp
```

新增 material channel composer bridge：

```text
src/slicer_core/material/MaterialChannelComposer.h
src/slicer_core/material/MaterialChannelComposer.cpp
tests/unit/material_channel_composer/main.cpp
```

新增 CLI / 验证脚本：

```text
scripts/run_09p_cli_experimental_tests.ps1
scripts/run_09p_experimental_pipeline_tests.ps1
```

## 4. 新增配置字段

新增配置命名空间：

```json
{
  "experimental": {
    "openvdbPipeline": {
      "enabled": false,
      "engine": "legacy",
      "admissionMode": "strict_closed",
      "failurePolicy": "fail_fast",
      "allowNonProductionOutput": false,
      "writeProductionRgbwsv": false
    }
  }
}
```

默认值全部为 safe-off：

```text
enabled = false
engine = legacy
admissionMode = strict_closed
failurePolicy = fail_fast
allowNonProductionOutput = false
writeProductionRgbwsv = false
```

OpenVDB 不可用时，experimental diagnostic 返回稳定 issue code：

```text
OPENVDB_UNAVAILABLE
```

显式请求 `writeProductionRgbwsv=true` 时，只产生准入警告，不允许绕过 admission：

```text
EXPERIMENTAL_RGBWSV_REQUIRES_ADMISSION
```

## 5. 新增 CLI Experimental Flag

`slicer_cli` 新增显式 experimental diagnostic path：

```text
--experimental-openvdb-shell
--admission-mode strict_closed|warn_and_attempt|diagnostic_only
--no-production-rgbwsv
--experimental-report <path>
```

行为：

```text
未传 --experimental-openvdb-shell：完全走 legacy path。
传入 --experimental-openvdb-shell：只写 diagnostic report，不调用 run_slicer，不写 production package。
USE_OPENVDB=OFF：report 包含 OPENVDB_UNAVAILABLE。
diagnostic_only：productionAdmission.status = diagnostic_only。
warn_and_attempt：productionAllowed=false，nonProduction=true。
strict_closed：有 blocker 时 productionAllowed=false，且不写 package。
```

该 CLI experimental path 始终写：

```text
productionPackageWritten = false
legacyPathExecuted = false
writeProductionRgbwsv = false
```

## 6. Admission Policy 行为

`ProductionAdmissionPolicy` 当前支持：

```text
strict_closed
warn_and_attempt
diagnostic_only
repair_then_strict
```

`strict_closed` 下阻断 production 的稳定 code 包括：

```text
MESH_BOUNDARY_EDGES
MESH_SELF_INTERSECTION_CONFIRMED
MESH_NON_MANIFOLD_EDGES
MESH_DUPLICATE_FACES
MESH_OPPOSITE_DUPLICATE_FACES
MESH_LOCAL_WINDING_INCONSISTENCY
OPENVDB_LEVEL_SET_FAILED
OPENVDB_UNAVAILABLE
```

关键规则：

```text
confirmed self-intersection => fail_fast
warn_and_attempt => 永远不得 productionAllowed
diagnostic_only => 只输出 diagnostic
repair_then_strict => 当前是占位，未实现 repair 前不得 productionAllowed
```

## 7. 已运行验证命令

Task 05-10 已运行并通过的主要验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

09P CLI experimental smoke：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_cli_experimental_tests.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_cli_experimental_tests.ps1 -BuildDir build-openvdb-09p
```

09P 总验证脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_experimental_pipeline_tests.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_experimental_pipeline_tests.ps1 -OpenVdbBuildDir build-openvdb-09p
```

OpenVDB 环境验证：

```powershell
$env:VCPKG_ROOT='D:\vcpkg-openvdb'
.\scripts\configure_openvdb_vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT -BuildDir build-openvdb-09p -Triplet x64-windows
cmake --build build-openvdb-09p --config Debug --target surface_shell_robustness_unit_tests
cmake --build build-openvdb-09p --config Debug --target geometry_kernel_service_unit_tests
.\build-openvdb-09p\Debug\geometry_kernel_service_unit_tests.exe
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
.\scripts\run_surface_shell_real_model_tests.ps1 -BuildDir build-openvdb-09p
.\scripts\run_surface_shell_texture_tests.ps1 -BuildDir build-openvdb-09p
```

验证说明：

```text
OpenVDB vcpkg 环境可用，使用 D:\vcpkg-openvdb。
configure 阶段存在 Boost CMP0167 dev warning，不影响本阶段构建和测试结果。
run_ci_quick.ps1 会重写 5 个 3MF 样例文件；这些属于验证副作用，提交前均已恢复。
```

## 8. 未运行验证命令及原因

未运行 Release benchmark：

```powershell
.\scripts\run_surface_shell_benchmarks.ps1 -BuildDir build-openvdb-09p -Config Release
```

原因：

```text
Task 10 中 benchmark 被设计为 -RunBenchmarks 可选项。
09P-R1 当前目标是 experimental boundary 与 smoke/regression，不以性能基线作为提交前强制门槛。
```

## 9. Production 禁止事项

当前仍然禁止：

```text
默认启用 OpenVDB。
让 OpenVDB 成为全项目强制依赖。
用 OpenVDB experimental path 替代 legacy slicer_cli production path。
从 experimental OpenVDB path 写真实 OBJ/3MF production RGBWSV TIFF。
修改 p0.rgbwsv.2。
修改 RGBWSV channel order = R G B W S V。
修改 bitDepth = 8。
修改 polarity = black_is_print。
把 warn_and_attempt 输出标记为 productionAllowed。
```

## 10. 当前仍不可 Production-Safe 的输入类型

真实 OBJ/3MF 当前仍不得直接视为 production-safe。

原因：

```text
真实模型仍可能存在 boundary edges、non-manifold、duplicate/opposite duplicate、local winding、multi-component 等生产准入 blocker。
09P-R1 没有实现 mesh repair。
09P-R1 没有把 OpenVDB shell texture 结果写入 production RGBWSV package。
```

允许的当前用途：

```text
diagnostic report
nonProduction 实验链路验证
OpenVDB smoke / robustness / texture transfer 测试
Qt/UI 后续读取 report 的基础数据源
```

不允许的当前用途：

```text
作为正式生产切片包直接下发。
作为 RIP/设备端最终 production TIFF。
绕过 strict admission 生成 production RGBWSV。
```

## 11. 下一阶段建议

建议下一阶段进入：

```text
09P-R2 hardening
```

优先事项：

```text
1. 将 experimental report schema 固化为稳定文档。
2. 为真实 OBJ/3MF 增加更完整的 topology admission gate 与 mesh repair 前置判断。
3. 将 OpenVDB service / texture service / MaterialChannelComposer 的中间数据契约进一步收敛。
4. 将 Qt Debug UI 对接 experimental report，而不是直接接 production package。
5. 评估是否独立启动 mesh repair/admission gate 阶段。
6. 若需要性能目标，再启用 Release benchmark 作为 nightly 或专项门槛。
```

保守结论：

```text
09P-R1 已建立 experimental OpenVDB pipeline 接入边界。
legacy production path 未被替代。
OpenVDB 默认仍关闭。
真实 OBJ/3MF 仍不得直接 production RGBWSV 输出。
warn_and_attempt 仍然只能 nonProduction。
下一阶段进入 09P-R2 hardening 或单独 mesh repair/admission gate 阶段。
```
