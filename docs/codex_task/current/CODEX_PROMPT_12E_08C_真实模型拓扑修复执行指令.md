# CODEX_PROMPT_12E-08C 真实模型拓扑修复执行指令

> 文档状态：PREPARED
> 日期：2026-07-20
> 当前允许任务：12E-08C-R1-01

## 1. 角色

你负责执行真实模型拓扑修复前置专项的单个原子任务。目标是形成可解释的
`repair_then_strict`，不是通过放宽门禁让模型尽快进入生产。

## 2. 必读顺序

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/SLICE_AI_SKILL_MASTER.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
.agents/docs/doc-state.md
docs/slice/DOC/DOC_DECISION_09P_R2_mesh_repair_admission_gate.md
docs/slice/DOC/DOC_DECISION_12E_08C_R1_R2_R3_真实模型拓扑修复前置专项.md
docs/slice/PRD/PRD_12E_08C_真实模型拓扑修复与严格准入.md
docs/slice/DEV/DEV_12E_08C_MeshRepairThenStrict设计.md
docs/slice/DEMO/DEMO_12E_08C_真实模型拓扑修复验证方案.md
docs/slice/DOC/DOC_SCHEMA_12E_MeshRepairReport.md
docs/slice/DOC/DOC_MATRIX_12E_真实模型拓扑修复与严格准入.md
docs/slice/DOC/DOC_PREP_12E_08C_R1_拓扑分类与修复契约准备.md
docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md
当前源代码和测试。
```

## 3. 开始前

```powershell
git branch --show-current
git status --short
```

工作树不干净时分类现有修改，不得覆盖或回退用户内容。

## 4. 必须输出的实施计划

修改前按项目规则输出：

```text
Problem Type
Layer(s) Involved
Official Documents
Historical Documents
AI Workspace Evidence
Current Code Reality
Current State
Target State
Historical State
Pending Confirmation
Risk Points
Files To Change
Verification Plan
```

## 5. 执行规则

```text
只执行用户指定的一个原子任务；
先读实现，再做最小改动；
不自动进入下一任务；
不把任务完成写成生产准入通过；
不引入第三方库，除非先有单独 ADR 和用户确认；
不在 strict_closed 内隐式修复；
不使用 warn_and_attempt production；
confirmed self-intersection fail-fast；
无法唯一修复时输出 manual_repair_required；
修复候选必须重新 strict；
属性未知时阻断 production。
```

## 6. 架构边界

```text
Qt 仅在 UI；
repair service 不写 TIFF/report 文件；
report writer 不决定 repair；
公共 DTO 不包含 Qt/OpenVDB 类型；
OpenVDB optional/OFF；
legacy slicer_cli production path 不替代。
```

## 7. 协议边界

```text
schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
bitDepth=8；
polarity=black_is_print；
printValue=0；emptyValue=255。
```

本专项不得修改这些值。

## 8. 代码规范

遵循项目 C++20/Allman/命名/Doxygen 规则及现有模块局部风格。新增正式 Public 接口必须有 Doxygen；
不做无关命名或目录重构。

## 9. 验证与证据

运行任务指定的 target/test，并记录真实输出。未运行的测试明确说明。提交前至少运行：

```powershell
git diff --check
git status --short
```

是否提交遵循用户当前指令和 `AGENTS.md`，不得自动 push。

## 10. 停止条件

遇到以下情况必须停止并说明：

```text
需要新增第三方依赖；
需要放宽 strict 或修改 Production Safety Rules；
需要改变 RGBWSV 协议；
修复会丢失 UV/material/texture provenance；
真实模型只能通过 destructive boolean/voxel remesh 处理；
需要调整 12E-08D required-case matrix。
```

## 11. 双模式隔离规则

repair-then-strict 只服务于 `slicePipeline.mode=global_surface_shell` 的准入。不得把 repair 前移为 legacy
导入的通用强制步骤，不得修改 legacy 原始 SceneModel/TIFF 行为，也不得在 global 失败时调用 legacy
作为隐式回退。本专项仍禁止写生产 TIFF；共享 writer 接入由 12E-08D 单独实施。

## 12. 后续阶段准备入口

```text
docs/slice/DOC/DOC_PREP_12E_08C_R1_EligibilityFixtureBaseline准备.md
docs/slice/DOC/DOC_PREP_12E_08C_R2_ConservativeRepair准备.md
docs/slice/DOC/DOC_PREP_12E_08C_R3_RealModelReleaseGate准备.md
```
