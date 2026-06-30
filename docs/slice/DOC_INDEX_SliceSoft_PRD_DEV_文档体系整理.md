# DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理

> 文档版本：v0.1
> 文档状态：Document Control / PRD-DEV Index
> 生成日期：2026-06-30
> 当前分支：`spike/09P-openvdb-experimental-pipeline`
> 当前阶段判断：09P-R1 已完成，09P-R2 hardening 前置文档治理
> 适用范围：`docs/slice` 正式文档入口、`docs/codex_task` Codex 任务入口、`docs/archive` 历史归档

---

## 1. 目的

原 `docs/slicer` 已经积累了 P0 到 09P-R1 的大量 PRD、DEV、TASKS、REPORT、DOC_DECISION、ROADMAP 和 CODEX_PROMPT。
这些文档记录了项目真实演进过程，但因为早期大量内容来自网页 ChatGPT 对话、阶段性 Codex 指令和临时报告，当前存在以下问题：

```text
1. 文档数量多，但没有统一入口。
2. PRD / DEV / TASKS / REPORT 混在同一目录，读者很难判断哪个是当前真源。
3. 部分 README、handoff、MASTER 文档仍停留在 09B-R3 或 09P-R1 前。
4. 阶段文档记录了历史决策，但不一定代表当前实现状态。
5. 09P-R2 之前需要先把 demo -> 正式项目的文档体系和开发路线收束。
```

本文件用于把旧阶段文档整理成可读、可维护、可交给 Codex 继续执行的结构。

---

## 2. 当前事实基线

### 2.1 Current State

当前代码和报告显示：

```text
当前分支：spike/09P-openvdb-experimental-pipeline
当前阶段：09P-R1 已完成
下一阶段建议：09P-R2 hardening
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

进入 09P-R2 前，建议确认：

```text
1. 是否接受 09P-R1 为已完成阶段；
2. 是否将 09P-R2 定义为 hardening，而不是继续 R1 功能开发；
3. 是否单独拆出 mesh repair / admission gate 阶段；
4. 是否需要把 README、AGENTS.md、.agents/docs 同步到 09P-R1 后状态；
5. 目录级重组已经完成：正式文档进入 docs/slice，Codex 任务进入 docs/codex_task，旧阶段资料进入 docs/archive。
```

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

## 5. 当前必须修订的过时入口

以下当前入口仍可能含 09B-R3 / 09P-R1 前状态，应在 09P-R2 前单独修订：

```text
README.md
AGENTS.md
.agents/docs/project-profile.md
.agents/docs/build-and-test.md
docs/slice/README.md
docs/slice/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md
```

修订原则：

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
docs/slice/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md
docs/slice/DOC_CLASSIFICATION_2026-06-30_docs治理归档清单.md
docs/slice/PRD_FORMAL_SliceSoft_正式切片软件产品需求总览.md
docs/slice/DEV_FORMAL_SliceSoft_正式切片软件总体技术方案.md
docs/slice/PRD_DEMO_IMPLEMENTED_SliceSoft_当前Demo功能基线.md
docs/slice/DEV_DEMO_IMPLEMENTED_SliceSoft_当前Demo技术基线.md
docs/slice/ROADMAP_FORMAL_SliceSoft_Demo到正式项目演进路线.md
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md
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

## 8. 对 09P-R2 的入口建议

09P-R2 不应直接开始写更多功能。建议先执行：

```text
09P-R2-0：同步文档当前状态和入口
09P-R2-1：固化 experimental report schema
09P-R2-2：扩展 topology admission gate
09P-R2-3：定义 mesh repair 前置判断，但不实现自动 repair
09P-R2-4：收敛 OpenVDB service / texture service / MaterialChannelComposer 数据契约
09P-R2-5：建立 RGBWSV experimental golden / RIP compatibility 设计
09P-R2-6：Qt Debug UI 读取 experimental report
09P-R2-7：CI matrix：OpenVDB OFF / ON 分层
09P-R2-8：生成 REPORT_09P_R2
```

---

## 9. 结论

当前不是缺少文档，而是缺少文档控制层。
本文件将历史文档从“都可能是入口”收束为：

```text
历史阶段文档 = 背景与证据
最新 REPORT = 当前状态
FORMAL PRD / DEV / ROADMAP = 当前总控
TASKS_09P_R2 = 后续执行入口
```

进入 09P-R2 前，应先接受这个文档层级，随后再按 `TASKS_09P_R2_*` 逐项执行。
