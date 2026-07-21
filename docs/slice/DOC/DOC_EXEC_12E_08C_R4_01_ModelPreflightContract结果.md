# DOC_EXEC_12E-08C-R4-01 Model Preflight Contract 结果

> 任务状态：COMPLETE
> 日期：2026-07-21
> 生产边界：合同与诊断报告，不接 UI、不启动切片、不写生产包

## 1. 完成范围

R4-01 已建立 backend-neutral Model Preflight 合同：

```text
ModelPreflightStatus；
ModelPreflightPipelineMode；
ModelPreflightAdmissionStatus；
ModelPreflightIssueSeverity；
ModelPreflightErrorCode 与 E_12E_PREFLIGHT_* 稳定名称；
ModelPreflightIssue；
ModeAdmissionResult；
ModelPreflightCacheIdentity；
ModelPreflightResult；
确定性 SHA-256 cache key；
slicesoft.model_preflight.12e_08c_r4.1 report skeleton。
```

## 2. 代码落点

```text
src/slicer_core/preflight/ModelPreflightTypes.*
src/slicer_core/preflight/ModelPreflightCacheIdentity.*
src/slicer_core/diagnostics/ModelPreflightReport.*
tests/unit/model_preflight_contract/main.cpp
tests/golden/expected/12e_r4_model_preflight_report_skeleton.json
CMakeLists.txt
```

cache key 复用现有 `ComputeMeshRepairSha256`，没有复制哈希实现。合同仅使用 STL、项目 `Json` 和 core
DTO，不依赖 Qt、filesystem writer、pipeline 或 OpenVDB。

## 3. 合同结果

```text
schema=slicesoft.model_preflight.12e_08c_r4.1；
identity=source/resource/transform/options/algorithmVersion；
legacyAdmission 与 globalAdmission 可同时存在；
同一诊断可表达 legacy=warning、global=blocked；
source/resource/transform/options/algorithmVersion 任一变化都会改变 cache key；
report skeleton productionOutputWritten=false。
```

## 4. TDD 与验证证据

RED：首次构建定向 target 因 `ModelPreflightReport.h` 尚不存在而按预期失败，证明测试先于实现。

GREEN：

```text
cmake --build build --config Debug --target model_preflight_contract_unit_tests：PASS；
ctest -R ^model_preflight_contract_unit_tests$：1/1 PASS；
production_admission + mesh_repair_contract + model_preflight_contract：3/3 PASS；
cmake --build build --config Debug：PASS；
slicer_debug_ui --self-test：PASS startup / experimental-report-summary。
```

`scripts/run_ci_quick.ps1` 未通过，失败点为既有 golden `material_process_top2 widthPx expected=48 actual=226`。
本任务未修改模型、缩放、golden 场景或 legacy raster；该失败继续作为既有 Quick CI baseline blocker，不能
写成 R4-01 PASS，也不由本原子任务扩大范围修复。

## 5. 边界确认

```text
未接入 UI；
未实现 fast/full preflight service；
未接入 CLI/pipeline gate；
未修改或修复模型；
未写 TIFF、preview 或 production package；
未修改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
OpenVDB 默认状态和 legacy 默认生产路径不变。
```

## 6. 后续结论

R4-01 合同已可供 R4-02 Two-stage Preflight Service 使用。R4-02 在开发前仍需冻结 importer/transform
输入边界、fast/full 阶段结果合并、cache/stale/cancel 行为和定向 fixture，故先进入 R4-02 准备收口。
