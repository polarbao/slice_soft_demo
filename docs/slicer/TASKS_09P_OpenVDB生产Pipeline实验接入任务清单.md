# TASKS_09P_OpenVDB生产Pipeline实验接入任务清单

> 阶段：09P-R1  
> 当前基线：`spike/09B-R3-shell-production-readiness`  
> 工作分支建议：`spike/09P-openvdb-experimental-pipeline`

## 总规则

```text
每次只执行一个 Task。
每个 Task 开始前确认 git status --short 干净。
每个 Task 只修改当前任务相关文件。
每个 Task 完成后运行指定验证命令。
验证通过后立即提交。
不要自动执行下一个 Task。
不要 push，除非用户明确要求。
```

生产安全规则：

```text
OpenVDB 默认关闭。
legacy slicer_cli 生产路径不得被替代。
warn_and_attempt 只能 nonProduction。
strict_closed 必须拒绝 non-manifold / duplicate / opposite duplicate / local winding。
confirmed self-intersection 必须 fail_fast。
production RGBWSV 协议不修改。
真实 OBJ/3MF 当前不能直接 production RGBWSV 输出。
```

## Task 01：修正文档中的当前阶段基线

状态：已完成。

提交信息：

```text
docs: align current phase with 09B-R3 readiness
```

验证：

```powershell
git status --short
git diff --check
```

## Task 02：新增 09P 阶段文档骨架

状态：当前任务。

新增：

```text
docs/slicer/PRD_09P_OpenVDB表面壳层纹理实验生产管线接入.md
docs/slicer/DEV_09P_OpenVDB与LegacyPipeline融合设计.md
docs/slicer/TASKS_09P_OpenVDB生产Pipeline实验接入任务清单.md
docs/slicer/CODEX_PROMPT_09P_OpenVDB生产Pipeline实验接入执行指令.md
```

提交信息：

```text
docs: add 09P experimental pipeline planning documents
```

验证：

```powershell
git status --short
git diff --check
```

## Task 03：新增 ProductionAdmissionPolicy 模块

目标：

```text
把 R3 stable issue code 转成 production admission decision。
```

验收：

```text
StrictClosed 无 blocker => ProductionAllowed
confirmed self-intersection => FailFast
non-manifold / duplicate / opposite duplicate / local winding => NonProductionOnly
WarnAndAttempt => NonProductionOnly
DiagnosticOnly => DiagnosticOnly
RepairThenStrict placeholder 不得 ProductionAllowed
```

## Task 04：把 admission decision 接入 R3 report/diagnostic 层

目标：

```text
在 surface shell report 中输出 machine-readable productionAdmission。
不接 slicer_cli。
不写 TIFF。
```

## Task 05：新增 09P experimental config 字段，默认关闭

目标：

```text
新增 experimental.openvdbPipeline 配置，默认 enabled=false、engine=legacy。
```

## Task 06：新增 OpenVdbGeometryKernelService 抽象层

目标：

```text
封装 OpenVDB geometry kernel，USE_OPENVDB=OFF 时返回 OPENVDB_UNAVAILABLE。
```

## Task 07：新增 SurfaceShellTextureService 抽象层

目标：

```text
封装 R3 surface shell texture transfer，保持 UV/material seam 策略。
```

## Task 08：新增 MaterialChannelComposer bridge 的最小实现

目标：

```text
新增 in-memory RGBWSV channel composition bridge，不写 TIFF。
```

## Task 09：给 slicer_cli 增加 experimental flag，但只输出 diagnostic/report

目标：

```text
新增显式 experimental flag，默认关闭，不改变 legacy 行为。
```

## Task 10：新增 09P 验证脚本

目标：

```text
新增 scripts/run_09p_experimental_pipeline_tests.ps1，集中执行 OFF 与可选 OpenVDB ON 验证。
```

## Task 11：新增 09P-R1 阶段报告

目标：

```text
总结 Task 01 到 Task 10 的实现结果、验证命令和限制。
```

## Task 12：最终本地总验证

目标：

```text
确认 Task 01 到 Task 11 串起来后仍然可构建、可测试。
原则上不产生 commit。
```
