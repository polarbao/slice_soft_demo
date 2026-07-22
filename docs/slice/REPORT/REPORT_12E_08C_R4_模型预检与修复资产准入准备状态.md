# REPORT_12E-08C-R4 模型预检与修复资产准入准备状态

> 文档状态：IN PROGRESS / R4-01..03 COMPLETE / R4-04 READY
> 日期：2026-07-21

## 1. 阶段结论

R4 作为 R3-04 NO-GO 与 12E-08D 之间的正式插入专项，准备文档已经完整。该专项允许正常闭合模型继续
推进 Texture Surface/Model Fill 功能，但不降低三个 required 真实 OBJ 的生产 Gate。

## 2. 已完成准备

```text
可达性和模型治理分析；
插入专项 Decision；
PRD/DEV/DEMO；
分阶段 Roadmap；
R4-01..08 原子任务清单；
Codex 执行提示；
启动依赖与停止条件；
主 PRD/DEV/DEMO/Matrix/Roadmap/Report/Index 同步；
AI context handoff。
```

## 3. 当前允许工作

```text
R4-01 Preflight Contract；
R4-02 Two-stage Preflight；
R4-03 Mode Admission/Pipeline Gate；
R4-04 Qt Preflight UI；
R4-05 Clean Positive Matrix。
```

这些任务不要求先取得三个修复模型，但不得写 global production package。

## 4. 当前阻断工作

```text
R4-06：等待三个 required OBJ 修复资产；
R4-07：等待 R4-06 全部 admitted；
R4-08：等待四 case Release/global/legacy 证据；
12E-08D：等待 R4-08 GO 和用户明确授权。
```

## 5. 固定产品参数

```text
Texture Surface base minimum = 0.10mm；
UI/config step = 0.01mm；
effective minimum = max(0.10mm, 2 * classificationResolutionMm)；
maximum = dynamic allTextureThresholdMm；
C/M/Y/K = MaterialProcessProfile roles，非新增 TIFF channels；
legacy 默认；global fail-closed；无 silent fallback。
```

## 6. 下一任务

`12E-08C-R4-03 Mode Admission and Pipeline Gate` 已完成；R4-04 Qt Preflight UI 已完成准备，等待明确启动。

## 7. 模型资产准备结果

`model` 目录已完成 15 个 OBJ/3MF 的统一 Release 预检。7 个 OBJ strict PASS 且第二次完整审计结果稳定，
已满足 R4-01..05 的真实 OBJ 输入准备；1 个 OBJ 需人工修复，另外 4 个 OBJ 和 3 个 3MF 需重建。

当前目录没有 strict PASS 3MF，R4 正向 Texture2D 3MF 仍使用既有
`samples/models/3mf/texture2d_checker_cube.3mf`。该缺口不阻断 R4-01..05，但不得据此解除 R4-06..08。

完整清单见 `REPORT_12E_08C_R4_模型资产预检清单.md`。

## 8. R4-01 实施结果

ModelPreflight DTO、稳定错误码、双模式 admission、cache identity/key、report schema、unit 和 golden 已实现。
定向测试、相关合同测试、Debug 全量构建及 Qt self-test 通过。Quick CI 仍被既有
`material_process_top2 widthPx expected=48 actual=226` golden baseline 阻断。

详细证据见 `../DOC/DOC_EXEC_12E_08C_R4_01_ModelPreflightContract结果.md`。

## 9. R4-02 准备结果

已冻结 importer/最终 transform 边界、fast/full 执行链、结果合并优先级、source/resource 双 hash stale
检测、进程内 cache、阶段边界取消、正向/负向 fixture 和验证命令。明确 `load_model_report` 返回的三角形
已应用 transform/autoOrient，后续服务不得重复变换；完整审计不足不得 PASS。

详细准备见 `../DOC/DOC_PREP_12E_08C_R4_02_TwoStagePreflightService准备.md`。

## 10. R4-02 实施结果

两阶段预检服务、内容身份、cache/stale/cancel 和完整审计 fail-closed 已实现。generated fixture、真实
`xiao_ma` OBJ、`yecan/3.obj` 与 Texture2D checker 3MF 均通过定向测试；服务不接 UI/pipeline/writer。

Debug 全量构建和相关 CTest 通过。Quick CI 仍停在既有
`material_process_top2 widthPx expected=48 actual=226` golden。详细证据见
`../DOC/DOC_EXEC_12E_08C_R4_02_TwoStagePreflightService结果.md`。

## 11. R4-03 准备结果

已冻结 shared fatal、legacy warning/global blocked 的拓扑差异、backend unavailable、未知 error fail-closed、
稳定 code 排序和 `productionOutputWritten=false` 语义；同时明确 CLI/pipeline gate 必须在 global core、staging
目录和 writer 之前阻断，且任何 global 失败不得自动回退 legacy。

R4-03 达到 `READY FOR DEVELOPMENT`。详细准备见
`../DOC/DOC_PREP_12E_08C_R4_03_ModeAdmission与PipelineGate准备.md`。

## 12. R4-03 实施结果

独立模式准入策略、backend-neutral pipeline gate、legacy/global facade 和普通 CLI 入口已经接通。shared fatal
同时阻断两种模式，完整拓扑问题保持 legacy warning/global blocked；blocked 输入不启动核心或 writer，且
不存在 global -> legacy 自动回退。

定向 4/4 CTest 与 Debug 全量构建通过。Quick CI 的切片/RIP/3MF 正负向/schema/support 已通过，仍在既有
`material_process_top2 widthPx expected=48 actual=226` golden baseline 失败。详细结果见
`../DOC/DOC_EXEC_12E_08C_R4_03_ModeAdmission与PipelineGate结果.md`。

## 13. R4-04 准备结果

Qt controller/presenter/coordinator/panel 边界、QThreadPool/generation/cancel/QPointer 生命周期、外部 OpenVDB
capability probe、三条切片入口统一守门、中文状态机和 UI Smoke 已冻结。R4-04 达到
`READY FOR DEVELOPMENT`。

R4-05 基础准备完整并等待 R4-04；R4-06 的合同准备完整但仍缺三个 required 外部修复资产；R4-07/08 为
依赖已准备、不可提前执行。详细准备见
`../DOC/DOC_PREP_12E_08C_R4_04_QtPreflightUI准备.md`。
