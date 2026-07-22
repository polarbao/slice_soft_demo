# DOC_EXEC_12E-08C-R4-03 Mode Admission 与 Pipeline Gate 结果

> 文档状态：COMPLETE
> 日期：2026-07-22
> 原子任务：12E-08C-R4-03
> 下一任务：R4-04 Qt Preflight UI（已准备，等待明确启动）

## 1. 完成结论

R4-03 已完成。共享 `ModelPreflightService` 诊断事实现在可确定性派生 `legacy` 与
`global_surface_shell` 两种准入结论；CLI 的普通 legacy 入口和显式 OpenVDB candidate 入口均在核心算法、
staging 目录及 writer 之前执行 fail-closed gate。

本任务没有授权 global production output。`globalAdmission=passed` 只允许进入既有非生产 candidate 链，
不表示 12E-08D 已获得 GO。

## 2. 实现内容

### 2.1 模式准入策略

新增 `ModelPreflightAdmissionPolicy`：

```text
shared fatal / lifecycle / unknown error -> legacy 与 global 均 BLOCKED；
完整拓扑问题 -> legacy WARNING，global BLOCKED；
global backend unavailable -> 仅 global BLOCKED；
普通 warning -> 两种模式 WARNING；
blocker/warning code -> 稳定去重并按字典序输出；
productionOutputWritten -> 始终保持 false。
```

global 拓扑阻断同时保留原始 `MESH_*` code 和
`E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED`，便于 CLI、Qt 和报告层稳定定位原因。

### 2.2 Pipeline Gate

新增 backend-neutral `ModelPreflightGate`：

```text
Run ModelPreflightService
-> EvaluateModelPreflightAdmissions
-> 选择显式 mode
-> stale/cancelled/incomplete/blocked：action invocation=0
-> passed/warning：只调用所选 mode action 一次
```

gate 不改写 mode，也没有 global -> legacy 自动回退。失败消息包含稳定 blocker code，并保留 importer 的
底层稳定 detail，例如 3MF `E_3MF_*` 错误码。

### 2.3 CLI 与两个 facade

```text
slicer_cli 普通切片：run_slicer -> RunSlicePipelineLegacy；
RunSlicePipelineLegacy：先执行 legacy preflight gate，再调用原 legacy 实现；
RunOpenVdbCandidatePipeline：先执行 global preflight/backend gate，再进入 candidate core；
benchmark 内部 no-output legacy 调用仍保留低层 run_slicer，未伪装为普通生产入口。
```

blocked 输入不会创建目标 package，且不会进入 candidate core 或 legacy writer。历史显式 candidate 的
非生产输出行为只在已准入输入上保留，未提升为 production writer。

### 2.4 兼容性修正

R4-03 接入 Quick CI 后发现两处集成兼容问题并已修正：

1. `ModelPreflightService` 的相对模型路径解析与 importer 对齐：优先 config-relative，文件不存在时回退
   process current-working-directory-relative；
2. gate 失败消息保留 importer detail，避免 bad 3MF 回归只看到统一 preflight code 而丢失 `E_3MF_*`。

## 3. 测试覆盖

新增：

```text
model_preflight_admission_unit_tests；
model_preflight_pipeline_gate_unit_tests。
```

覆盖 clean、八类 topology、shared fatal、fallback warning、backend unavailable、生命周期、unknown error、
稳定排序、cache 复用、blocked legacy/global 零 action、blocked package 不落盘及 importer detail 保留。

开发过程先确认缺失 admission policy 产生 RED 编译失败，再完成策略和 gate 后转为 GREEN。

## 4. 验证结果

实际执行并得到以下结果：

```text
cmake --build build --config Debug --target
  model_preflight_admission_unit_tests
  model_preflight_pipeline_gate_unit_tests
PASS

ctest --test-dir build -C Debug -R
  ^(model_preflight_contract_unit_tests|model_preflight_service_unit_tests|
    model_preflight_admission_unit_tests|model_preflight_pipeline_gate_unit_tests)$
4/4 PASS

cmake --build build --config Debug
PASS

scripts/run_ci_quick.ps1
切片 quick regression、RIP、3MF 正负向、schema/support 均通过；
最终仍停在既有 golden baseline：
material_process_top2 widthPx expected=48 actual=226。
```

Quick CI 的已知失败与 R4-03 准入/gate 无关，本任务没有修改 golden 掩盖该问题。

## 5. 固定边界

```text
Qt UI 未接入，留给 R4-04；
模型 repair 未启用；
legacy 仍为默认模式；
global_surface_shell 仍为 diagnostic-only；
OpenVDB 仍可选且默认关闭；
不存在 silent fallback；
p0.rgbwsv.2、R G B W S V、uint8、black_is_print 未修改；
未修改、提交或覆盖 model/obj 用户资产；
12E-08D 继续 BLOCKED。
```

## 6. 下一步

R4-04 需要在现有 Qt 工作台上接入异步 preflight controller、中文状态/问题列表、stale/取消生命周期和两个
一键入口的统一 gate。开始编码前应先冻结 QObject 生命周期、线程归属、关闭窗口取消和 UI Smoke 矩阵。
