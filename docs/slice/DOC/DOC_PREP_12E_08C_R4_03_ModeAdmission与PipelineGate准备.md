# DOC_PREP_12E-08C-R4-03 Mode Admission 与 Pipeline Gate 准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-22
> 原子任务：12E-08C-R4-03
> 前置任务：R4-01 Model Preflight Contract、R4-02 Two-stage Preflight Service COMPLETE

## 1. 任务目标

R4-03 只把 R4-02 产生的共享诊断事实转换为 `legacy` 与 `global_surface_shell` 两个模式的确定性准入
结论，并在 CLI/pipeline 边界建立 fail-closed、no-fallback 守门。

本任务不接 Qt，不修复模型，不修改 RGBWSV/TIFF/manifest 协议，不授权 `global_surface_shell` 写生产包。
`globalAdmission=passed` 只表示允许进入全局 diagnostic core，不等于 12E-08D production admission。

## 2. 已确认现状

### 2.1 可复用能力

```text
ModelPreflightService：输出 fresh、complete、backend-neutral 的共享诊断事实；
ModelPreflightResult：已包含 legacyAdmission/globalAdmission 合同；
EvaluateMeshRepairPreflight：输出完整 topology/self-intersection issue；
GetOpenVdbStatus：可提供可选 OpenVDB 编译/运行状态；
RunSlicePipelineLegacy：legacy facade，内部仍调用 run_slicer；
RunOpenVdbCandidatePipeline：显式 OpenVDB candidate 入口；
apps/slicer_cli/main.cpp：当前直接调用 run_slicer 或 candidate pipeline。
```

R4-02 有意把两种 admission 保持为 `blocked + E_12E_PREFLIGHT_NOT_RUN`。R4-03 必须在不修改共享
cache value 的前提下，对结果副本重算两种模式结论。

### 2.2 当前缺口

```text
没有独立 ModelPreflightAdmissionPolicy；
CLI 仍可直接启动 legacy/candidate pipeline；
pipeline 入口没有统一 fresh preflight gate；
backend unavailable 尚未映射到 globalAdmission；
没有证明 global blocker 时 core/writer 调用次数均为 0；
没有稳定验证“legacy warning、global blocked、绝不自动回退”的测试。
```

现有 `ProductionAdmissionPolicy` 服务于历史 OpenVDB production/non-production 决策，不能复用为模型导入
准入。R4-03 应新增独立策略，避免把“模型能否进入当前模式”和“当前结果能否写生产包”混为一谈。

## 3. 冻结准入输入

计划新增：

```cpp
struct ModelPreflightAdmissionContext
{
    bool globalBackendAvailable{false};
};

ModelPreflightResult EvaluateModelPreflightAdmissions(
    const ModelPreflightResult& diagnosticResult,
    const ModelPreflightAdmissionContext& context);
```

策略只读取 immutable 诊断结果与显式 capability，不重新加载模型、不执行几何算法、不修改 cache key。输出
保留原 `identity/cacheKey/issues/productionOutputWritten`，只替换 `legacyAdmission/globalAdmission`。

## 4. 模式准入矩阵

### 4.1 两种模式共同阻断

以下状态或问题必须同时阻断 legacy/global：

```text
NotRun/Pending/Running；
Stale/Cancelled；
E_12E_PREFLIGHT_IMPORT_INVALID；
E_12E_PREFLIGHT_RESOURCE_MISSING（severity=error，即 fail_fast）；
E_12E_PREFLIGHT_NON_FINITE_GEOMETRY；
E_12E_PREFLIGHT_AUDIT_INCOMPLETE；
无法解释的 error 级 issue。
```

共同阻断时保留原稳定 code；状态自身没有 issue 时补充对应 `E_12E_PREFLIGHT_*` code。任何未知 error
必须 fail-closed，不能降为 warning。

### 4.2 拓扑问题差异

以下完整诊断 issue 在 legacy 中为 warning，在 global 中为 blocker：

```text
MESH_SELF_INTERSECTION_CONFIRMED；
MESH_BOUNDARY_EDGES；
MESH_NON_MANIFOLD_EDGES；
MESH_DEGENERATE_TRIANGLES；
MESH_DUPLICATE_FACES；
MESH_OPPOSITE_DUPLICATE_FACES；
MESH_DUPLICATE_FACE_ATTRIBUTE_CONFLICT；
MESH_LOCAL_WINDING_INCONSISTENCY。
```

global 的 `blockerCodes` 同时包含原始拓扑 code 和
`E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED`；legacy 的 `warningCodes` 保留原始 code。列表必须稳定去重并
按字典序输出。

R4-03 不扩大 legacy 几何兼容算法。legacy warning 只表示允许沿用现有 legacy 行为，报告/UI 必须明确
风险；若后续发现某 topology code 对 legacy 也是实际 fatal，应通过显式矩阵与测试升级，不允许在调用点
临时判断。

### 4.3 资源 fallback 与普通 warning

`missingTexturePolicy=warn_and_fallback` 形成的 warning 可让两种模式进入 warning 状态；其他 warning/info
同样保留。策略不替调用方决定 fallback 内容，只尊重 R4-02 已验证的配置语义。

### 4.4 Backend capability

```text
globalBackendAvailable=false：只阻断 global，增加 E_12E_PREFLIGHT_BACKEND_UNAVAILABLE；
globalBackendAvailable=true：按共享诊断决定 global pass/warning/blocked；
legacy 结论不受 OpenVDB 是否可用影响。
```

这里的 backend 指当前被选择的 global diagnostic backend capability，不表示 OpenVDB 获得 production
角色。默认 OpenVDB OFF 仍可由已存在的 CPU global-distance diagnostic backend提供 capability；具体探针
必须由 pipeline facade 显式注入，policy 不读取编译宏或全局状态。

### 4.5 最终状态

```text
blockerCodes 非空 -> blocked；
无 blocker 且 warningCodes 非空 -> warning；
无 blocker/warning 且 shared result fresh+complete -> passed。
```

`productionOutputWritten` 在整个 R4-03 中保持 false。

## 5. Pipeline Gate 合同

计划新增 backend-neutral gate：

```cpp
struct ModelPreflightGateRequest
{
    ModelPreflightRequest preflightRequest;
    ModelPreflightPipelineMode selectedMode;
    ModelPreflightAdmissionContext admissionContext;
};

struct ModelPreflightGateResult
{
    ModelPreflightExecutionResult preflight;
    ModeAdmissionResult selectedAdmission;
    bool pipelineAllowed{false};
};
```

执行顺序冻结为：

```text
Run ModelPreflightService
-> reject cancelled/stale/incomplete
-> EvaluateModelPreflightAdmissions
-> select requested mode admission
-> blocked: return gate result, pipeline/core/writer invocation count=0
-> passed/warning: return allowed to the explicitly selected facade
```

gate 不拥有 writer，也不自动改写 mode。调用 global 被阻断时不得调用 legacy facade；调用 legacy 时不得
因为 global blocked 而阻断 legacy warning 路径。

## 6. CLI 与 Pipeline 插入点

R4-03 计划修改：

```text
src/slicer_core/preflight/ModelPreflightAdmissionPolicy.h/.cpp；
src/slicer_core/pipeline/ModelPreflightGate.h/.cpp；
src/slicer_core/pipeline/SlicePipeline.h/.cpp；
src/slicer_core/pipeline/OpenVdbCandidatePipeline.h/.cpp；
apps/slicer_cli/main.cpp；
tests/unit/model_preflight_admission/main.cpp；
tests/unit/model_preflight_pipeline_gate/main.cpp；
CMakeLists.txt。
```

插入规则：

1. CLI 不再直接以“所选模式可运行”为前提；先获得 fresh gate result；
2. `RunSlicePipelineLegacy` 与 OpenVDB/global candidate 的外层 facade 均必须具备 gate 入口；
3. 低层 `run_slicer` 保留为 legacy 实现细节和既有单元测试入口，普通 CLI/Qt 后续不得绕过 facade；
4. blocked global 必须在 `RunOpenVdbCandidatePipeline` 的模型核心、staging 目录和 writer 之前返回；
5. R4-03 对 clean global 只允许 diagnostic 执行，不发布 production package；生产写包仍等待 R4-08 GO、
   12E-08D 用户确认和独立 production admission；
6. 现有显式实验命令若与上述边界冲突，必须保留为测试内部入口或改为 no-output diagnostic，不能借兼容名义
   绕过 R4 gate。

Qt 两个一键按钮的具体接线、异步 worker 和中文展示属于 R4-04；R4-03 只提供 Qt 可复用的 core facade。

## 7. 测试矩阵

### 7.1 Policy unit

```text
fresh clean + backend available：legacy PASS、global PASS；
self-intersection：legacy WARNING、global BLOCKED；
boundary/non-manifold/duplicate/winding/degenerate：逐项验证模式差异；
import/resource fail_fast/non-finite/audit incomplete：两种模式 BLOCKED；
resource warn_and_fallback：两种模式 WARNING；
backend unavailable：legacy 不受影响、global BLOCKED；
stale/cancelled/not-run：两种模式 BLOCKED；
unknown error：两种模式 fail-closed；
issue 顺序不同：输出 blocker/warning 顺序与 hash 无关、结果稳定。
```

### 7.2 Gate unit

使用计数 callback/fake runner，不能依赖实际 writer：

```text
legacy warning：legacy runner=1；
global topology blocked：global core=0、writer=0、legacy runner=0；
global backend unavailable：global core=0、writer=0；
stale/cancelled：所有 runner=0；
clean global diagnostic：global diagnostic runner=1、writer=0；
任何 global failure：fallback count=0。
```

### 7.3 真实输入

正向只读输入：

```text
model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj；
model/obj/yecan/3.obj；
samples/models/3mf/texture2d_checker_cube.3mf。
```

模式差异负向输入使用 generated fixture，并可只读复核 `nai_you/aishen/meigui` 原模型。不得提交、覆盖
或自动修复 `model/obj` 用户资产，也不得把 clean case 计入 required repair PASS。

## 8. 验证命令

R4-03 实施时至少执行：

```powershell
cmake --build build --config Debug --target model_preflight_admission_unit_tests model_preflight_pipeline_gate_unit_tests
ctest --test-dir build -C Debug -R "^(model_preflight_contract_unit_tests|model_preflight_service_unit_tests|model_preflight_admission_unit_tests|model_preflight_pipeline_gate_unit_tests)$" --output-on-failure
cmake --build build --config Debug
scripts/run_ci_quick.ps1
git diff --check
git status --short
```

`scripts/run_ci_quick.ps1` 当前已知会停在既有
`material_process_top2 widthPx expected=48 actual=226` baseline。若该 baseline 未被独立修复，R4-03 必须
如实记录外部失败，不能改 golden 掩盖，也不能宣称 Quick CI 通过。

## 9. 停止条件

```text
需要修改生产 TIFF/manifest/RIP 协议：停止；
需要让 global 自动 fallback legacy：停止；
需要把 OpenVDB candidate 直接提升为 production writer：停止；
需要在本任务接 Qt 线程/UI：停止，留给 R4-04；
需要放宽完整自相交或 strict topology：停止；
需要修改/提交用户 model 资产：停止；
无法证明 blocked 时 core/writer 调用为 0：R4-03 不得 COMPLETE。
```

## 10. 准备结论

R4-03 的准入矩阵、backend capability 语义、pipeline gate、CLI 插入点、no-fallback/no-writer 断言、真实输入
和验证命令均已冻结，达到 `READY FOR DEVELOPMENT`。下一次只应执行 R4-03 代码实现，不得同时进入
R4-04 Qt UI 或 R4-05 正向 width/material 矩阵。
