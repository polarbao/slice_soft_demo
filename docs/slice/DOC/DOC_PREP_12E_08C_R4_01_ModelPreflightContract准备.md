# DOC_PREP_12E-08C-R4-01 Model Preflight Contract 准备

> 文档状态：READY FOR DEVELOPMENT  
> 日期：2026-07-21  
> 原子任务：12E-08C-R4-01  
> 前置证据：`REPORT_12E_08C_R4_模型资产预检清单.md`

## 1. 当前代码现实

当前仓库已经具备 `MeshRepairPreflight`、完整自相交分析、repair eligibility、canonical hash、
`ProductionAdmissionPolicy` 和 JSON report 基础，但还没有面向导入/UI/双模式的统一 `ModelPreflight` 合同。

现有结构可复用：

```text
src/slicer_core/geometry/repair/MeshRepairTypes.*
src/slicer_core/geometry/repair/MeshRepairPreflight.*
src/slicer_core/geometry/repair/MeshRepairHash.*
src/slicer_core/diagnostics/MeshRepairReport.*
src/slicer_core/diagnostics/ProductionAdmissionPolicy.*
tests/unit/mesh_repair_contract/main.cpp
tests/golden/expected/12e_mesh_repair_report_skeleton.json
```

不得把 `ModelPreflight` 直接塞入 Qt，也不得让新合同依赖 writer 或启动实际切片。

## 2. R4-01 冻结合同

R4-01 只新增以下 backend-neutral 合同：

```text
ModelPreflightStatus：not_run/pending/running/passed/warning/blocked/stale；
ModelPreflightIssue：code/category/severity/count/summaryKey/recommendationKey/context；
ModelPreflightPipelineMode：legacy/global_surface_shell；
ModeAdmissionResult：mode/status/blockerCodes/warningCodes；
ModelPreflightCacheIdentity：source/resource/transform/options/algorithmVersion hash；
ModelPreflightResult：schema、identity、状态、两种 mode admission、issues、productionOutputWritten=false；
ModelPreflightErrorCode：E_12E_PREFLIGHT_* 稳定名称；
slicesoft.model_preflight.12e_08c_r4.1 report skeleton。
```

本任务只定义和序列化合同，不实现文件扫描、异步缓存、诊断执行或 pipeline gate。

## 3. 计划文件

```text
src/slicer_core/preflight/ModelPreflightTypes.h
src/slicer_core/preflight/ModelPreflightTypes.cpp
src/slicer_core/preflight/ModelPreflightCacheIdentity.h
src/slicer_core/preflight/ModelPreflightCacheIdentity.cpp
src/slicer_core/diagnostics/ModelPreflightReport.h
src/slicer_core/diagnostics/ModelPreflightReport.cpp
tests/unit/model_preflight_contract/main.cpp
tests/golden/expected/12e_r4_model_preflight_report_skeleton.json
CMakeLists.txt
```

如实现时发现 `ModelPreflightCacheIdentity` 只有纯数据而无独立行为，可合并进 `ModelPreflightTypes.*`，但不得
与 `MeshRepairTypes` 混合，也不得复制 SHA-256 实现；hash 复用现有 canonical hash 工具或抽取通用 helper。

## 4. 必测合同

```text
所有 enum 均有稳定 snake_case 名称；
所有 E_12E_PREFLIGHT_* 名称与 PRD/DEV 一致；
同一 identity 输入生成相同 cache key；
source/resource/transform/options/algorithmVersion 任一变化均改变 cache key；
legacy warning 与 global blocked 可同时存在于同一 result；
report skeleton 的 schema、字段、排序投影与 golden 一致；
productionOutputWritten 固定 false；
合同层无 Qt 类型、无文件写入、无 pipeline 启动。
```

## 5. 验证命令

```powershell
cmake --build build --config Debug --target model_preflight_contract_unit_tests
ctest --test-dir build -C Debug -R "^model_preflight_contract_unit_tests$" --output-on-failure
git diff --check
```

## 6. 后续依赖

R4-01 PASS 后，R4-02 才能实现 two-stage service、cache/stale 和取消；R4-03 才能把两种 admission 接入
CLI/pipeline；R4-04 才能接 Qt。已验证的 7 个 clean OBJ 作为 R4-02/R4-05 真实输入候选，不进入 R4-01
unit/golden，避免合同测试依赖大型用户资产。

## 7. 启动结论

R4-01 的边界、文件、复用点、测试和停止条件均已明确，可在用户明确下达 R4-01 开发指令后实施。
