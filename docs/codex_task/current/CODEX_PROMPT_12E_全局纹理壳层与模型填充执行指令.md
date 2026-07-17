# CODEX_PROMPT_12E 全局纹理壳层与模型填充执行指令

> 文档状态：12E-04 COMPLETE / 12E-05 READY FOR USER ADMISSION
> 日期：2026-07-17
> 当前不得自动执行；下一候选任务为 12E-05，仍须用户明确指定

## 1. 角色

你负责在 `slice_soft_demo` 中执行 Stage 12E 的单个原子任务。12E 要把 Texture Surface 和 Model Fill 实现为完整三维模型上的互补分区，不允许逐层二维近似冒充全局结果。

## 2. 开始前必读

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/SLICE_AI_SKILL_MASTER.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/DOC/DOC_DECISION_12E_全局纹理表面层与模型填充互补策略.md
docs/slice/PRD/PRD_12E_全局纹理表面层与模型填充连续调节.md
docs/slice/DEV/DEV_12E_全局纹理壳层与模型填充分区设计.md
docs/slice/DEMO/DEMO_12E_全局纹理壳层与模型填充验证方案.md
docs/slice/ROADMAP/ROADMAP_12E_全局纹理壳层与模型填充分阶段路线.md
docs/slice/DOC/DOC_PREP_12E_R0_ConfigDTO契约准备.md
docs/slice/DOC/DOC_PREP_12E_R1_GlobalPartitionService骨架准备.md
docs/slice/DOC/DOC_PREP_12E_R1_LegacyCpuGlobalDistanceCandidate准备.md
docs/slice/DOC/DOC_PREP_12E_R1_OpenVdbConformanceAdapter准备.md
docs/slice/DOC/DOC_PREP_12E_R2_WidthSweep与ReportSchema准备.md
docs/slice/DOC/DOC_SCHEMA_12E_TextureFillPartitionReport.md
docs/slice/DOC/DOC_MATRIX_12E_全局纹理填充分区验收矩阵.md
docs/slice/REPORT/REPORT_12E_启动准备状态.md
docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md
相关 12A/12B-R2/12C/12D 正式文档
当前源文件和测试
```

## 3. 必做仓库检查

```powershell
git branch --show-current
git status --short
```

报告无关 dirty state，不得覆盖、删除或回退用户修改。

## 4. 执行边界

```text
只执行用户明确指定的一个 Task 12E-XX；12E-04 已完成，不得自动启动 12E-05；
不要自动执行下一任务；
不要修改 12D-R3 的 repair/UI/真实模型范围；
不要把 OpenVDB 设为默认或强制依赖；
不要从 experimental path 写 production TIFF；
不要修改 p0.rgbwsv.2、RGBWSV 顺序、uint8、black_is_print；
不要用 per-layer 2D shell 实现 global_3d_distance；
不要通过 modelFill.enabled=false 实现 allTexture；
不要绕过 strict topology blocker；
不要声称未运行的构建或测试通过。
```

## 5. 固定语义

```text
TextureSurfaceMask ∩ ModelFillMask = Empty；
TextureSurfaceMask ∪ ModelFillMask = ModelMask；
width 增大时 texture 单调增加、fill 单调减少；
baseMinimumWidthMm = 0.10；
effectiveMinimumWidthMm = max(0.10, 2 * 最粗分类分辨率)；
allTextureThresholdMm = max(effectiveMinimumWidthMm, 模型最大内部距离按 0.01 mm 向上取整)；
allTexture 时 texture=model、fill=0、unassigned=0；
TextureSurface 可叠加 W/V，但不能同时计为 ModelFill；
Model > OuterVarnishShell > Support > Empty。
```

## 6. Required Output Before Code

在任何代码修改前输出：

```markdown
## Implementation Plan

### Problem Type
### Layer(s) Involved
### Official Documents
### Historical Documents
### AI Workspace Evidence
### Current Code Reality
### Current State
### Target State
### Historical State
### Pending Confirmation
### Risk Points
### Files To Change
### Verification Plan
```

若任务涉及 production path、依赖或架构迁移，给出方案后停止并等待确认。

## 7. 实现原则

```text
先 contract/DTO，后 backend；
先 diagnostic，后 production admission；
先 generated fixture，后真实模型；
先默认 OFF，后 optional ON conformance；
先 exact partition，后 UI；
先实际 benchmark，再冻结性能预算。
```

## 8. 验证

每个任务使用 TASKS_12E 中的定向验证。代码任务至少包括：

```powershell
git diff --check
```

并按修改面选择：

```text
config/partition unit tests；
generated golden；
OpenVDB OFF/ON 独立 lane；
slicer_cli/rip_reader strict；
12D closure exact；
Qt self-test/UI smoke；
Release runtime/peak memory。
```

计划中的脚本名在文件实际创建前不能当作已存在入口。

## 9. 完成后输出

```text
1. 实际修改文件；
2. Current/Target 状态变化；
3. 实际运行的命令与结果；
4. 未运行的验证和原因；
5. 风险、blocker 和 rollback；
6. git status --short；
7. 明确停止，不自动开始下一任务。
```

除非用户明确要求，不 commit、不 push。
