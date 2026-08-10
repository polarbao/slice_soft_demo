# DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理

> 文档版本：v0.4
> 文档状态：Document Control / PRD-DEV Index
> 生成日期：2026-06-30
> 更新日期：2026-08-09
> 当前分支：`feature/14-slicer-capability-package`，每个任务开始前仍需重新确认
> 当前阶段判断：Stage 12E COMPLETE；Stage 13 COMPLETE（批准范围）；03D-LIBTIFF GO_OPTIONAL；03E-02 **GO_ON_DEMAND**；Stage 14 **SLICER-SIDE COMPLETE / EXTERNAL VALIDATION DEFERRED**；Stage 15 **COMPLETE / 19 OF 19**；HOSTFLOW **LOCAL COMPLETE / H-A..H-E 与 H-F-01 COMPLETE**
> 适用范围：`docs/slice` 正式文档入口、`docs/codex_task` Codex 任务入口、`docs/archive` 历史归档

---

## 1. 目的

原 `docs/slicer` 已经积累了 P0 到 09P-R1 的大量 PRD、DEV、TASKS、REPORT、DOC_DECISION、ROADMAP 和 CODEX_PROMPT。
这些文档记录了项目真实演进过程，但因为早期大量内容来自网页 ChatGPT 对话、阶段性 Codex 指令和临时报告，当前存在以下问题：

```text
1. 文档数量多，但没有统一入口。
2. PRD / DEV / TASKS / REPORT 混在同一目录，读者很难判断哪个是当前真源。
3. README、AGENTS、.agents、handoff 和 MASTER 类入口已通过 09P-R2-0 同步到 09P-R1 已完成 / 09P-R2 hardening 口径。
4. 阶段文档记录了历史决策，但不一定代表当前实现状态。
5. 09P-R2 之前需要先把 demo -> 正式项目的文档体系和开发路线收束。
```

本文件用于把旧阶段文档整理成可读、可维护、可交给 Codex 继续执行的结构。

---

## 2. 当前事实基线

### 2.1 Current State

当前代码和报告显示：

```text
当前分支：feature/14-slicer-capability-package，任务开始前通过 git 命令确认
最新完成阶段：Stage 12E 与 Stage 13 批准范围已收口
当前执行阶段：Stage 14 切片侧已收口，外部验收延期；Stage 15 COMPLETE / 19 OF 19；HOSTFLOW LOCAL COMPLETE
最新完成任务：HOSTFLOW H-A..H-E 与 H-F-01 操作流回归收口全部完成（2026-08-10）
历史状态：HOSTFLOW **H-D-01 俯视画布接线**、**H-D-02 三维画布与 RB-P1**、
**H-D-05 生产包目录入口**已完成；H-D-03/04 已解除前置并完成准备。
已裁决：HQ-09 = 乙（参考宿主等价于封装前切片软件，分 E1/E2/E3 三批）；HQ-10 = 甲（场景保存加载归 PrintApp）
```

09P-R1 已完成：

```text
ProductionAdmissionPolicy
experimental.openvdbPipeline safe-off config
OpenVdbGeometryKernelService
SurfaceShellTextureService
MaterialChannelComposer bridge
slicer_cli --experimental-openvdb-shell diagnostic path
run_09p_experimental_pipeline_tests.ps1
REPORT_09P_R1
```

生产安全边界仍然不变：

```text
OpenVDB 默认关闭
legacy slicer_cli production path 不替代
不从 experimental OpenVDB path 写真实 OBJ/3MF production RGBWSV TIFF
不修改 p0.rgbwsv.2
不修改 channelOrder = R G B W S V
不修改 bitDepth = 8
不修改 polarity = black_is_print
warn_and_attempt 不得视为 production-safe
```

### 2.2 Target State

正式项目化后的文档体系应支持：

```text
1. 一个产品级 PRD 真源；
2. 一个技术级 DEV 真源；
3. 一个阶段路线图；
4. 一个文档状态矩阵；
5. 每个阶段有 PRD / DEV / TASKS / DEMO / CODEX_PROMPT / REPORT；
6. 每个 Codex 任务都能从明确入口开始，不再要求模型全量扫所有历史文档。
```

### 2.3 Historical State

历史文档仍有价值，尤其用于解释为什么某阶段存在：

```text
P0 / 00A / 00B / 00C：RGBWSV 最小闭环与浮雕修正
03 / 03B / 03C：协议固化与 RIP Reader
04 / 04A：OBJ/MTL/PNG 纹理基础
05 / 05A：MaterialPolicy 与真实工艺 profile
06 / 06A / 06B：3MF / Texture2D / ColorGroup
07 / 07A / 07B：Qt Debug UI 与 UI smoke
R0 / R1 / R2：正式项目化架构、模块边界、配置报告测试 CI
08 / 08A：支撑形态与桥接 fixture
09 / 09A / 09B / 09B-R1/R2/R3：OpenVDB / SDF / surface shell 纹理验证
09P-R1：OpenVDB experimental pipeline 接入边界
```

但历史文档不能直接替代当前实现判断。

### 2.4 Pending Confirmation

12C 初始开放项已经通过正式决策冻结：

```text
1. 普通用户默认四类稳定 Profile；
2. UI 运行时自动生成 session effective config；
3. DiagnosticsDock 默认位于底部并折叠；
4. 12D 不阻断 12C-R0/R1，12C 不实现其业务算法；
5. OpenVDB 保持默认关闭的 utility/candidate。
```

R0 当前没有产品需求待确认；Qt/MSVC 兼容路线由 `12C-R0-01` 的实际构建证据决定。

---

## 3. 证据等级规则

后续判断统一使用 A/B/C/D：

| 等级 | 来源 | 用途 |
|---|---|---|
| A | 当前代码、CMake、脚本、测试、当前分支最新 REPORT 中列明的验证命令 | 可作为实现依据 |
| B | 当前正式 PRD / DEV / ROADMAP / TASKS / DOC_DECISION | 可作为目标方向 |
| C | 历史阶段文档、网页 GPT 聊天归档 PDF、旧 handoff、旧 roadmap | 只作为背景 |
| D | 与当前代码或最新 REPORT 冲突的描述 | 不作为实现依据 |

冲突时优先级：

```text
当前代码/脚本/测试
> 最新阶段 REPORT
> DOC_DECISION
> 当前正式 PRD / DEV / TASKS
> 历史 REPORT / PRD / DEV
> PDF / 聊天记录 / 旧 README
```

---

## 4. 旧 docs/slicer 文档类型分类

当前目录大致包含：

| 类型 | 作用 | 当前处理建议 |
|---|---|---|
| `PRD_*` | 阶段需求 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/prd` |
| `DEV_*` | 阶段技术方案 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/dev` |
| `TASKS_*` / `CODEX_TASKS_*` | 阶段任务清单 | 已归档到 `docs/codex_task/archive/completed_tasks` |
| `REPORT_*` | 阶段实现状态 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/reports` |
| `DOC_DECISION_*` | 阶段决策 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/decisions` |
| `DOC_REVIEW_*` | 文档或代码审查 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/reviews` |
| `ROADMAP_*` | 后续路线 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/roadmaps`，当前路线以 formal roadmap 为准 |
| `CODEX_PROMPT_*` | 给 Codex 的阶段执行指令 | 已归档到 `docs/codex_task/archive/completed_prompts` |
| `DEMO_*` | 验证方案 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/demos` |
| `ARCH_*` | 架构审查与边界设计 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/architecture` |
| `OPENVDB_*` / `VCPKG_*` | 依赖说明 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/environment` |
| PDF / 聊天归档 | 历史推理链 | 已归档到 `docs/archive/2026-06-30_slicer_legacy/chat_exports`，只作背景 |

---

## 5. 历史 09P-R2-0 入口同步记录

以下入口已同步到 09P-R1 已完成 / 09P-R2 hardening 口径：

```text
README.md
AGENTS.md
.agents/docs/project-profile.md
.agents/docs/build-and-test.md
.agents/docs/architecture-boundary.md
.agents/docs/doc-state.md
docs/slice/README.md
docs/slice/DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md
```

同步原则：

```text
只更新当前阶段和执行入口；
不重写历史阶段内容；
不删除旧报告；
明确 09P-R1 已完成；
明确下一阶段为 09P-R2 hardening 或 mesh repair/admission gate；
明确 OpenVDB experimental path 仍不能直接 production 输出。
```

---

## 6. 推荐新的正式文档入口

本轮新增/建议使用以下入口：

```text
docs/slice/README.md
docs/slice/DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md
docs/slice/DOC/DOC_CLASSIFICATION_2026-06-30_docs治理归档清单.md
docs/slice/PRD/PRD_FORMAL_SliceSoft_正式切片软件产品需求总览.md
docs/slice/DEV/DEV_FORMAL_SliceSoft_正式切片软件总体技术方案.md
docs/slice/PRD/PRD_DEMO_IMPLEMENTED_SliceSoft_当前Demo功能基线.md
docs/slice/DEV/DEV_DEMO_IMPLEMENTED_SliceSoft_当前Demo技术基线.md
docs/slice/ROADMAP/ROADMAP_FORMAL_SliceSoft_Demo到正式项目演进路线.md
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md
docs/slice/DOC/DOC_AUDIT_00_08_历史阶段文档缺口与补齐清单.md
docs/slice/PRD/PRD_00_08_Demo阶段功能基线汇总.md
docs/slice/DEV/DEV_00_08_Demo阶段技术基线汇总.md
docs/slice/DEMO/DEMO_00_08_Demo阶段验证与回归基线.md
docs/codex_task/current/TASKS_00_08_历史阶段文档补齐任务清单.md
docs/codex_task/current/CODEX_PROMPT_00_08_历史阶段文档补齐执行指令.md
docs/slice/ROADMAP/ROADMAP_SHORT_MID_LONG_SliceSoft_项目运行计划.md
docs/slice/PRD/PRD_09P_R2_OpenVDB实验生产管线Hardening.md
docs/slice/DEV/DEV_09P_R2_ReportSchema_AdmissionGate_CI_UI设计.md
docs/slice/DEMO/DEMO_09P_R2_OpenVDB实验生产管线Hardening验证方案.md
docs/codex_task/current/CODEX_PROMPT_09P_R2_OpenVDB实验生产管线Hardening执行指令.md
docs/slice/PRD/PRD_10_切片输出交付契约与纹理保真验收.md
docs/slice/DEV/DEV_10_OutputContract_TextureFidelity设计.md
docs/slice/DEMO/DEMO_10_切片输出契约与纹理保真验证方案.md
docs/slice/DOC/DOC_DECISION_10_RIP边界与切片输出契约.md
docs/codex_task/current/TASKS_10_切片输出交付契约与纹理保真验收任务清单.md
docs/codex_task/current/CODEX_PROMPT_10_切片输出交付契约与纹理保真验收执行指令.md
docs/slice/PRD/PRD_11_UI切片层预览交互配置与多模型能力.md
docs/slice/DEV/DEV_11_LayerPreview_UIConfig_MultiModel设计.md
docs/slice/DEMO/DEMO_11_UI切片层预览交互配置验证方案.md
docs/slice/DOC/DOC_DECISION_11_多模型切片处理范围决策.md
docs/codex_task/current/TASKS_11_UI切片层预览交互配置与多模型评估任务清单.md
docs/codex_task/current/CODEX_PROMPT_11_UI切片层预览交互配置与多模型评估执行指令.md
```

它们的角色：

| 文件 | 角色 |
|---|---|
| `DOC_INDEX_*` | 文档地图、证据等级、旧文档状态归属 |
| `DOC_CLASSIFICATION_*` | 本轮分类、归档位置和当前入口清单 |
| `PRD_FORMAL_*` | 产品级当前真源 |
| `DEV_FORMAL_*` | 技术架构当前真源 |
| `PRD_DEMO_IMPLEMENTED_*` | 当前 demo 已实现功能基线 |
| `DEV_DEMO_IMPLEMENTED_*` | 当前 demo 已实现技术基线 |
| `ROADMAP_FORMAL_*` | demo 到正式项目转型路线 |
| `TASKS_09P_R2_*` | 09P-R2 前置治理与 hardening 可执行任务 |
| `DOC_AUDIT_00_08_*` | 00-08 历史阶段文档缺口审计与补齐说明 |
| `PRD_00_08_*` | 00-08 demo 阶段功能基线汇总 |
| `DEV_00_08_*` | 00-08 demo 阶段技术基线汇总 |
| `DEMO_00_08_*` | 00-08 demo 阶段验证与回归基线 |
| `TASKS_00_08_*` | 00-08 历史阶段文档补齐任务清单 |
| `CODEX_PROMPT_00_08_*` | 00-08 历史阶段文档补齐执行提示词 |
| `ROADMAP_SHORT_MID_LONG_*` | 短期 / 中期 / 长期项目运行计划 |
| `PRD_09P_R2_*` | 09P-R2 OpenVDB experimental hardening 产品需求 |
| `DEV_09P_R2_*` | 09P-R2 report schema、admission gate、CI、UI 技术方案 |
| `DEMO_09P_R2_*` | 09P-R2 hardening 验证方案 |
| `CODEX_PROMPT_09P_R2_*` | 09P-R2 Codex 执行提示词 |
| `PRD_10_*` | 10 阶段切片输出契约与纹理保真产品需求 |
| `DEV_10_*` | 10 阶段 output contract / texture fidelity 技术方案 |
| `DEMO_10_*` | 10 阶段输出契约验证方案 |
| `DOC_DECISION_10_*` | 10 阶段 RIP 边界与切片输出契约决策 |
| `TASKS_10_*` | 10 阶段可执行任务清单 |
| `CODEX_PROMPT_10_*` | 10 阶段 Codex 执行提示词 |
| `PRD_11_*` | 11 阶段 UI 层预览、交互配置、多模型能力产品需求 |
| `DEV_11_*` | 11 阶段 LayerPreview / UI Config / MultiModel 技术方案 |
| `DEMO_11_*` | 11 阶段 layer preview、伪彩、配置面板验证方案 |
| `DOC_DECISION_11_*` | 多模型处理范围决策，当前只新增 11 一个阶段 |
| `TASKS_11_*` | 11 阶段可执行任务清单 |
| `CODEX_PROMPT_11_*` | 11 阶段 Codex 执行提示词 |

---

## 7. 后续文档治理规则

后续新增任何阶段，必须形成闭环：

```text
PRD_<stage>：为什么做，产品目标是什么
DEV_<stage>：怎么做，模块边界和接口是什么
TASKS_<stage>：如何拆成可执行任务
DEMO_<stage>：怎么验证
CODEX_PROMPT_<stage>：给 Codex 的执行入口
REPORT_<stage>：完成后当前状态和验证结果
DOC_DECISION_<stage>：如果有方向性决策，单独记录
```

每个阶段的 `TASKS` 必须包含：

```text
任务目标
允许修改文件
禁止事项
验证命令
完成条件
是否需要 commit
不允许自动执行下一任务
```

---

## 8. 历史 09P-R2 入口建议

本节仅保留 2026-06-30 的历史规划，不再是当前执行入口。当前入口见第 9、10 节。

09P-R2 不应直接开始写更多功能。建议先执行：

```text
09P-R2-0：同步文档当前状态和入口（已完成）
09P-R2-1：新增 09P-R2 PRD / DEV / DEMO / CODEX_PROMPT
09P-R2-2：固化 experimental report schema
09P-R2-3：强化 topology admission gate
09P-R2-4：定义 mesh repair 前置判断，但不实现自动 repair
09P-R2-5：收敛 OpenVDB service / texture service / MaterialChannelComposer 数据契约
09P-R2-6：设计 experimental golden / downstream output contract / texture fidelity compatibility
09P-R2-7：Qt Debug UI 读取 experimental report
09P-R2-8：CI matrix：OpenVDB OFF / ON 分层
09P-R2-9：生成 REPORT_09P_R2
```

---

## 9. Stage 12 当前入口更新

截至 2026-07-16：

```text
12A：材料、支撑、光油语义 P0/P1 基本完成，后续全局纹理/填充互补需求转入 12E；
12B：R0/R1/R2 已完成并生成最终状态报告；
12C：R0/R1/R2 已完成并收口；
12D：R0/R1/R2/R3 已完成，12D-10 三个真实 OBJ 验收通过；
12E：12E-01..07、12E-08A/08B/08C、12E-08C-R1/R2/R3/R4 与 12E-08D-01..04 已完成；CPU/OpenVDB OFF/ON conformance、Width Sweep、Texture Transfer、Diagnostic Composer、12D model/full-material closure、classification-to-raster、Release repair/global/legacy regression、Mesh Repair DTO/hash/report contract、Eligibility、Generated Golden、真实模型 Baseline、Cleanup、Weld/Winding Guard、Simple Boundary Fill、独立 evidence validator、non-manifold classifier、完整自相交证据、真实模型 repair matrix、共享 RGBWSV writer 和受限 Global production package 可复现；真实复杂 OBJ confirmed self-intersection 仍 BLOCKED；restricted Global Profile GO，普通支撑/光油/0.01mm 等价 NO-GO；
12F：Release/Debug 统一运行环境与专项文档已建立；12F-02 Release 性能基线刷新等待用户明确启动。
```

12E 规划入口：

```text
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
docs/slice/DOC/DOC_PREP_12E_R3_TextureTransfer与DiagnosticComposer准备.md
docs/slice/DOC/DOC_PREP_12E_R4_ProductionAdmission准备.md
docs/slice/DOC/DOC_DECISION_12E_Legacy与GlobalSurfaceShell双切片模式.md
docs/slice/DOC/DOC_SCHEMA_12E_DualSlicePipelineConfig.md
docs/slice/DOC/DOC_PREP_12E_08D_双模式生产写包准备.md
docs/slice/DOC/DOC_PREP_12E_09B_Qt双模式生产入口准备.md
docs/slice/PRD/PRD_12E_09B_Qt双模式生产入口与能力锁定.md
docs/slice/DEV/DEV_12E_09B_Qt双模式ProductionProfile设计.md
docs/slice/DEMO/DEMO_12E_09B_Qt双模式生产入口验证方案.md
docs/slice/REPORT/REPORT_12E_09B_Qt双模式生产入口当前状态.md
docs/slice/DOC/DOC_PREP_12E_09C_XY_DPI准备.md
docs/slice/PRD/PRD_12E_09C_XY_DPI配置与生产协议兼容.md
docs/slice/DEV/DEV_12E_09C_XY_DPI配置Reader与UI设计.md
docs/slice/DEMO/DEMO_12E_09C_XY_DPI验证方案.md
docs/slice/REPORT/REPORT_12E_09C_XY_DPI当前状态.md
docs/slice/DOC/DOC_DECISION_12E_09D_生产纹理厚度与单材料材质收口.md
docs/slice/PRD/PRD_12E_09D_生产纹理厚度与单材料材质控制.md
docs/slice/DEV/DEV_12E_09D_生产纹理厚度与单材料材质控制设计.md
docs/slice/DEMO/DEMO_12E_09D_生产纹理厚度与单材料材质验证方案.md
docs/slice/DOC/DOC_PREP_12E_09D_生产纹理厚度与单材料材质收口准备.md
docs/slice/REPORT/REPORT_12E_09D_生产纹理厚度与单材料材质准备状态.md
docs/slice/REPORT/REPORT_12E_09D_01_生产纹理合同与配置映射当前状态.md
docs/slice/PRD/PRD_12E_10_双模式最终闭环.md
docs/slice/DEV/DEV_12E_10_双模式最终闭环设计.md
docs/slice/DEMO/DEMO_12E_10_双模式最终闭环验证方案.md
docs/slice/DOC/DOC_PREP_12E_10_双模式最终闭环准备.md
docs/slice/DOC/DOC_PREP_12E_10B_真实模型双模式矩阵准备.md
docs/slice/DOC/DOC_PREP_12E_10C_Release性能与内存准备.md
docs/slice/DOC/DOC_SCHEMA_12E_ReleasePerformanceMatrix.md
docs/slice/DOC/DOC_PREP_12E_10D_阶段封口准备.md
docs/slice/REPORT/REPORT_12E_10A_同层Preview最终一致性当前状态.md
docs/slice/REPORT/REPORT_12E_10B_真实OBJ_3MF双模式矩阵当前状态.md
docs/slice/REPORT/REPORT_12E_10C_Release性能与内存当前状态.md
docs/slice/REPORT/REPORT_12E_全局纹理壳层与模型填充当前状态.md
docs/user_guides/SLICE_12E_双模式纹理壳层与模型填充验收说明.md
docs/slice/DOC/DOC_REVIEW_12G_TCWS_现有RIP白区合同与六通道策略比对.md
docs/slice/DOC/DOC_EXEC_12E_R4A_ClassificationRaster映射结果.md
docs/slice/DOC/DOC_EXEC_12E_R4B_完整材料语义闭环结果.md
docs/slice/DOC/DOC_DECISION_12E_08C_R1_R2_R3_真实模型拓扑修复前置专项.md
docs/slice/PRD/PRD_12E_08C_真实模型拓扑修复与严格准入.md
docs/slice/DEV/DEV_12E_08C_MeshRepairThenStrict设计.md
docs/slice/DEMO/DEMO_12E_08C_真实模型拓扑修复验证方案.md
docs/slice/ROADMAP/ROADMAP_12E_08C_真实模型拓扑修复分阶段路线.md
docs/slice/DOC/DOC_SCHEMA_12E_MeshRepairReport.md
docs/slice/DOC/DOC_MATRIX_12E_真实模型拓扑修复与严格准入.md
docs/slice/DOC/DOC_PREP_12E_08C_R1_拓扑分类与修复契约准备.md
docs/slice/DOC/DOC_EXEC_12E_08C_R1_01_MeshRepairContract结果.md
docs/slice/DOC/DOC_EXEC_12E_08C_R1_02_EligibilityPolicy结果.md
docs/slice/DOC/DOC_EXEC_12E_08C_R1_03_GeneratedFixtureGolden结果.md
docs/slice/DOC/DOC_EXEC_12E_08C_R1_04_真实模型PreRepairBaseline结果.md
docs/slice/DOC/DOC_PREP_12E_08C_R1_EligibilityFixtureBaseline准备.md
docs/slice/DOC/DOC_PREP_12E_08C_R2_ConservativeRepair准备.md
docs/slice/DOC/DOC_PREP_12E_08C_R3_RealModelReleaseGate准备.md
docs/slice/DOC/DOC_PREP_12E_08C_R3_01A_完整自相交证据准备.md
docs/slice/REPORT/REPORT_12E_08C_真实模型拓扑修复专项启动状态.md
docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_真实模型拓扑修复执行指令.md
docs/slice/DOC/DOC_DECISION_12E_08C_R4_模型导入预检与修复资产准入插入专项.md
docs/slice/DOC/DOC_DECISION_12E_08C_R4_06_真实模型族准入替代规则.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_模型预检与修复资产准入准备.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_01_ModelPreflightContract准备.md
docs/slice/DOC/DOC_EXEC_12E_08C_R4_01_ModelPreflightContract结果.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_02_TwoStagePreflightService准备.md
docs/slice/DOC/DOC_EXEC_12E_08C_R4_02_TwoStagePreflightService结果.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_03_ModeAdmission与PipelineGate准备.md
docs/slice/DOC/DOC_EXEC_12E_08C_R4_03_ModeAdmission与PipelineGate结果.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_04_QtPreflightUI准备.md
docs/slice/DOC/DOC_EXEC_12E_08C_R4_04_QtPreflightUI结果.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_05_CleanPositiveMatrix准备.md
docs/slice/DOC/DOC_EXEC_12E_08C_R4_05_CleanPositiveMatrix结果.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_06_RepairedAssetIntake准备.md
docs/slice/DOC/DOC_ANALYSIS_12E_R3_04后续可达性与模型治理.md
docs/slice/PRD/PRD_12E_08C_R4_模型导入预检与修复资产准入.md
docs/slice/DEV/DEV_12E_08C_R4_ModelPreflight与RepairAssetAdmission设计.md
docs/slice/DEMO/DEMO_12E_08C_R4_模型预检与修复资产准入验证方案.md
docs/slice/ROADMAP/ROADMAP_12E_08C_R4_模型预检与修复资产准入路线.md
docs/slice/REPORT/REPORT_12E_08C_R4_模型预检与修复资产准入准备状态.md
docs/slice/REPORT/REPORT_12E_08C_R4_模型资产预检清单.md
docs/codex_task/current/TASKS_12E_08C_R4_模型导入预检与修复资产准入任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_R4_模型导入预检与修复资产准入执行指令.md
docs/slice/DOC/DOC_PREP_12E_R5_QtUI与EffectiveConfig准备.md
docs/slice/DOC/DOC_PREP_12E_R6_Preview真实模型与阶段收口准备.md
docs/slice/DOC/DOC_SCHEMA_12E_TextureFillPartitionReport.md
docs/slice/DOC/DOC_MATRIX_12E_全局纹理填充分区验收矩阵.md
docs/slice/REPORT/REPORT_12E_启动准备状态.md
docs/codex_task/current/TASKS_12E_全局纹理壳层与模型填充任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_全局纹理壳层与模型填充执行指令.md
ai_workspace/context_handoff/2026-07-20_12E-08C真实模型拓扑修复专项准备.md
ai_workspace/context_handoff/2026-07-20_12E双切片模式与统一TIFF契约.md
```

12F 运行环境与切片性能优化专项入口：

```text
docs/slice/DOC/DOC_DECISION_12F_Release运行环境与切片性能优化专项.md
docs/slice/PRD/PRD_12F_Release运行环境与切片性能优化.md
docs/slice/DEV/DEV_12F_Release运行环境与切片性能优化设计.md
docs/slice/ROADMAP/ROADMAP_12F_Release运行环境与切片性能优化路线.md
docs/codex_task/current/TASKS_12F_Release运行环境与切片性能优化任务清单.md
docs/codex_task/current/CODEX_PROMPT_12F_Release运行环境与切片性能优化执行指令.md
```

Stage 13 模型场景、排版联合切片与 TIFF 原生预览入口：

```text
docs/slice/DOC/DOC_DECISION_13_模型场景排版与TIFF原生预览专项拆分.md
docs/slice/DOC/DOC_MATRIX_13_模型场景专项依赖与准入矩阵.md
docs/slice/DOC/DOC_CHECKLIST_13_未决产品输入与阶段Gate.md
docs/slice/DOC/DOC_PREP_13A_01_ModelTransform与ModelInstance合同准备.md
docs/slice/DOC/DOC_PREP_13B_01_MultiModelScene与EffectiveConfig准备.md
docs/slice/DOC/DOC_PREP_13B_06_单Package与SceneReport准备.md
docs/slice/REPORT/REPORT_13B_06_单Package与SceneReport当前状态.md
docs/slice/REPORT/REPORT_13B_07_真实模型矩阵与阶段收口当前状态.md
docs/slice/DOC/DOC_PREP_13C_01_TIFFLayerSource与Cache准备.md
docs/slice/DOC/DOC_PREP_13_全阶段原子任务实施准备与文件所有权.md
docs/slice/ROADMAP/ROADMAP_13_模型场景排版联合切片与TIFF预览路线.md
docs/slice/PRD/PRD_13A_模型俯视工作区与实例变换.md
docs/slice/DEV/DEV_13A_模型俯视渲染与变换架构设计.md
docs/slice/DEMO/DEMO_13A_模型俯视与变换验证方案.md
docs/slice/PRD/PRD_13B_多模型规则排版与联合切片.md
docs/slice/DEV/DEV_13B_MultiModelScene规则排版与联合切片设计.md
docs/slice/DEMO/DEMO_13B_多模型排版联合切片验证方案.md
docs/slice/PRD/PRD_13C_RGBWSV_TIFF原生统一预览.md
docs/slice/DEV/DEV_13C_TIFFLayerSource与统一材料合成设计.md
docs/slice/DEMO/DEMO_13C_TIFF原生统一预览验证方案.md
docs/slice/REPORT/REPORT_13_模型场景排版与TIFF原生预览准备状态.md
docs/codex_task/current/TASKS_12_13_后续开发计划总览清单.md
docs/codex_task/current/CODEX_PROMPT_13B_06_单Package与SceneReport执行指令.md
docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_13_模型场景排版联合切片与TIFF预览执行指令.md
docs/slice/DOC/DOC_DECISION_13B_08_场景作业流与13D工作台收口优先级.md
docs/slice/PRD/PRD_13B_08_批量导入与当前场景一键切片.md
docs/slice/DEV/DEV_13B_08_批量导入队列与场景切片编排设计.md
docs/slice/DEMO/DEMO_13B_08_批量导入与场景切片验证方案.md
docs/codex_task/current/TASKS_13B_08_场景作业流收口任务清单.md
docs/slice/PRD/PRD_13D_Qt工作台信息架构与布局收口.md
docs/slice/DEV/DEV_13D_Qt工作台Shell与ContextInspector设计.md
docs/slice/DEMO/DEMO_13D_Qt工作台交互验证方案.md
docs/codex_task/current/TASKS_13D_Qt工作台布局收口任务清单.md
```

Stage 14 / Stage 15 当前入口：

```text
docs/slice/DOC/DOC_DECISION_14_切片能力包封装与打印软件集成专项.md
docs/slice/DOC/DOC_DECISION_14_S2_RIP接口合同定案.md          ← 【S2 权威合同，实施只看这份】
docs/slice/DOC/DOC_DECISION_14A_04_R1_双视图纹理ViewData合同修订.md ← 【冻结合同受控修订】
docs/slice/DOC/DOC_DECISION_14F_R1_HOSTFLOW场景生命周期合同受控修订.md ← 【HQ-01 / DTO v1.5】
docs/slice/DOC/DOC_DECISION_14F_R2_HOSTFLOW隐式场景初始化上下文受控修订.md ← 【HQ-07 已关闭 / DTO v1.6】
docs/slice/DOC/DOC_DECISION_14F_R3_HOSTFLOW规则排版合同受控修订.md ← 【H-A-04 / DTO v1.7】
docs/slice/DOC/DOC_DECISION_HOSTFLOW_H_D_R1_视图接线归属与14E_04d延期作废.md ← 【H-D 组真源 / 决策反转】
docs/slice/DOC/DOC_DECISION_HOSTFLOW_H_E_R1_参考宿主目标水位裁决.md ← 【H-E 组真源 / HQ-09=乙 · HQ-10=甲】
docs/slice/DOC/DOC_PREP_HOSTFLOW_H_D_02_06_视图后续任务准备审查.md ← 【H-D-02..04 完成证据 / H-D-06 人工待验】
docs/slice/REPORT/REPORT_HOSTFLOW_H_D_02_三维画布与RB_P1当前状态.md ← 【H-D-02 / RB-P1 状态真源】
docs/slice/REPORT/REPORT_HOSTFLOW_H_D_03_三车道拖拽接线当前状态.md ← 【H-D-03 状态真源】
docs/slice/REPORT/REPORT_HOSTFLOW_H_D_04_双视图刷新接线当前状态.md ← 【H-D-04 状态真源】
docs/slice/REPORT/REPORT_HOSTFLOW_H_E_01_STL导入当前状态.md ← 【H-E-01 状态真源】
docs/slice/REPORT/REPORT_HOSTFLOW_H_E_03_支撑参数编辑当前状态.md ← 【H-E-03 状态真源】
docs/slice/REPORT/REPORT_HOSTFLOW_H_E_04_材料工艺Profile编辑当前状态.md ← 【H-E-04 状态真源】
docs/slice/REPORT/REPORT_HOSTFLOW_H_E_05_生产纹理设置当前状态.md ← 【H-E-05 / E2 Gate 状态真源】
docs/slice/DOC/DOC_PREP_HOSTFLOW_H_E_E1_STL与支撑参数实施准备.md ← 【H-E E1 准备真源】
docs/slice/DOC/DOC_PREP_HOSTFLOW_H_E_E2_材料与纹理实施准备.md ← 【H-E E2 合同与复核真源】
docs/slice/DOC/DOC_PREP_HOSTFLOW_H_E_E3_批量导入与白区预检实施准备.md ← 【H-E E3 合同与准入真源】
docs/slice/REPORT/REPORT_HOSTFLOW_H_E_02_批量导入当前状态.md ← 【H-E-02 状态真源】
docs/slice/REPORT/REPORT_HOSTFLOW_H_F_01_参考宿主操作流回归收口当前状态.md ← 【H-F-01 状态真源】
docs/slice/DOC/DOC_PREP_RENDER_R_B_LOD缺陷修复选型准备.md ← 【R-B 选型准备 / RD-B 待裁决】
docs/slice/DOC/DOC_PREP_HOSTFLOW_H_A_02_场景生命周期运行时准备.md ← 【H-A-02 准备真源】
docs/slice/DOC/DOC_PREP_HOSTFLOW_H_A_04_规则排版运行时准备.md ← 【H-A-04 准备真源】
docs/slice/DOC/DOC_PREP_HOSTFLOW_H_A_03_空场景端到端闭环准备.md ← 【H-A-03 闭环证据】
docs/slice/DOC/DOC_DECISION_14_UI_宿主模拟改造专项.md          ← 【14E 权威设计 v1.3】
docs/slice/DOC/DOC_SCHEMA_14_SceneViewData网格DTO规格.md       ← 【ViewData v1.2】
contracts/slicer_ui_view_spec.json                             ← 【双视图设置与网格真源】
docs/slice/PRD/PRD_14_切片能力包封装与打印软件集成.md
docs/slice/DEV/DEV_14_切片能力包封装与打印软件集成.md
docs/slice/DEMO/DEMO_14_切片能力包封装与打印软件集成验收方案.md
docs/slice/REPORT/REPORT_14_切片能力包封装与打印软件集成准备状态.md
docs/codex_task/current/TASKS_14_切片能力包封装与打印软件集成任务清单.md
docs/codex_task/current/CODEX_PROMPT_14_切片能力包封装与打印软件集成执行指令.md
docs/codex_task/current/TASKS_HOSTFLOW_宿主业务流程与场景生命周期补齐任务清单.md
docs/codex_task/current/CODEX_PROMPT_HOSTFLOW_宿主业务流程与场景生命周期执行指令.md
docs/slice/DOC/DOC_DECISION_15_纹理纯白区按需补白与材料闭合修复专项.md
docs/slice/DOC/DOC_PREP_15_纹理纯白区按需补白实施准备与依赖审查.md
docs/slice/PRD/PRD_15_纹理纯白区按需补白与材料闭合修复.md
docs/slice/DEV/DEV_15_纹理纯白区按需补白设计.md
docs/slice/DEMO/DEMO_15_纹理纯白区按需补白验收方案.md
docs/slice/REPORT/REPORT_15_纹理纯白区按需补白当前状态.md
docs/codex_task/current/TASKS_15_纹理纯白区按需补白任务清单.md
docs/codex_task/current/CODEX_PROMPT_15_纹理纯白区按需补白执行指令.md
```

12C 当前闭环文档：

```text
docs/slice/PRD/PRD_12C_Qt_UI配置预览工作台收口.md
docs/slice/DEV/DEV_12C_Qt_UI配置预览工作台设计.md
docs/slice/DEMO/DEMO_12C_Qt_UI配置预览验证方案.md
docs/slice/DOC/DOC_AUDIT_12C_现有QtUI能力与收口缺口审查.md
docs/slice/DOC/DOC_DECISION_12C_R0_R1_R2_Qt工作台阶段拆分.md
docs/slice/DOC/DOC_DECISION_12C_R0_01_QtMSVCFreshBuildLane.md
docs/slice/DOC/DOC_DECISION_12C_UI产品默认值与交互冻结.md
docs/slice/DOC/DOC_CHECKLIST_12C_阶段准入与上下文完整性.md
docs/slice/ROADMAP/ROADMAP_12C_Qt工作台分阶段执行路线.md
docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md
docs/codex_task/current/CODEX_PROMPT_12C_Qt工作台收口执行指令.md
docs/slice/REPORT/REPORT_12C_Qt工作台启动状态.md
ai_workspace/context_handoff/2026-07-10_12B-R2到12C-R0阶段交接.md
```

## 10. 结论

当前文档控制层已将历史材料和当前执行入口分离：

```text
历史阶段文档 = 背景与证据
最新 REPORT = 当前状态
FORMAL PRD / DEV / ROADMAP = 当前总控
TASKS_12D / TASKS_12E / TASKS_12F = 保留候选和历史执行入口
TASKS_13 = 当前新增产品专项执行入口
TASKS_15 = 当前已授权专项执行入口；下一原子任务 15D-03
```

12B、12C、12D 已收口；12E-08D、09A、09B、09C、09D 已完成，Legacy 仍为默认，Global 仅显式候选。
Stage 13 原 P0、13B-08、13D、13E、13G 已完成。`03D-LIBTIFF` 已完成 03D-01..07，最终
`GO_OPTIONAL`，当前默认 TIFF Writer 仍为 handwritten，LibTIFF 4.7.1 为显式可选轨道。
`12E-09D` 已完成生产纹理厚度、显式 all_texture、诊断隔离、单材料 W/V、Qt 控件和 Release/RIP
矩阵；`12E-10A..10D` 已完成同层绑定、真实模型矩阵、Release 性能/内存和最终文档封口。Stage 12E
当前批准范围已完成。12G-TCWS 已记录现有 RIP 的同包透明/白色和 `WSV=000` 白区信号事实，
但因其与固定物理通道语义冲突继续冻结，且不包含纹理铺底。12F 性能算法仍需逐项授权。
**Stage 14 切片侧已于 2026-08-07 收口，外部验收延期**。RIP 六问两轮闭合，S2 权威条款见
`DOC/DOC_DECISION_14_S2_RIP接口合同定案.md`（含 8 项作废方案禁止实现清单）。HOSTFLOW 已获 HQ-01 授权，
H-A-01 已完成 DTO v1.5 合同冻结；H-A-02 及后续卡仍需逐卡点名。
Stage 15 已全量收口（19/19 任务卡、G1–G8 全通过），按需补白 Profile 已启用为 production。

03D TIFF Writer 专项入口：

```text
docs/slice/DOC/DOC_DECISION_03D_LibTIFFWriter兼容迁移与性能Gate.md
docs/slice/PRD/PRD_03D_LibTIFF_RGBWSV兼容写入与性能优化.md
docs/slice/DEV/DEV_03D_LibTIFFWriter双后端迁移设计.md
docs/slice/DEMO/DEMO_03D_LibTIFF兼容与性能验证方案.md
docs/slice/ROADMAP/ROADMAP_03D_LibTIFFWriter兼容迁移路线.md
docs/slice/REPORT/REPORT_03D_LibTIFF兼容迁移准备状态.md
docs/codex_task/current/TASKS_03D_LibTIFF兼容迁移任务清单.md
docs/codex_task/current/CODEX_PROMPT_03D_LibTIFF兼容迁移执行指令.md
```
