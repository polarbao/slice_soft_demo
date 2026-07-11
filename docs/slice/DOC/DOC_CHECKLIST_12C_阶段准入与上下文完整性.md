# DOC_CHECKLIST_12C 阶段准入与上下文完整性

> 文档状态：Readiness Checklist / Stage 12C
> 日期：2026-07-10

## 1. 文档闭环检查

| 类型 | 文件 | 状态 |
|---|---|---|
| 前置报告 | `REPORT_12B_R2_OpenVDB_SDFUtility当前状态.md` | READY |
| PRD | `PRD_12C_Qt_UI配置预览工作台收口.md` | READY |
| DEV | `DEV_12C_Qt_UI配置预览工作台设计.md` | READY |
| DEMO | `DEMO_12C_Qt_UI配置预览验证方案.md` | READY |
| 现状审查 | `DOC_AUDIT_12C_现有QtUI能力与收口缺口审查.md` | READY |
| 阶段决策 | `DOC_DECISION_12C_R0_R1_R2_Qt工作台阶段拆分.md` | READY |
| 产品默认值 | `DOC_DECISION_12C_UI产品默认值与交互冻结.md` | READY |
| ROADMAP | `ROADMAP_12C_Qt工作台分阶段执行路线.md` | READY |
| TASKS | `TASKS_12C_Qt_UI配置预览任务清单.md` | READY |
| CODEX_PROMPT | `CODEX_PROMPT_12C_Qt工作台收口执行指令.md` | READY |
| 启动报告 | `REPORT_12C_Qt工作台启动状态.md` | READY |
| 布局基线 | `DOC_AUDIT_12C_R0_03_现有Qt布局与组件复用基线.md` | READY |
| 会话交接 | `ai_workspace/context_handoff/2026-07-12_12C-R0-03_布局组件基线.md` | READY |

## 2. 上下文入口检查

以下入口已切换到 12C-R0：

```text
README.md
AGENTS.md
.agents/AGENTS.md
.agents/docs/project-profile.md
.agents/docs/workflow-map.md
docs/slice/README.md
docs/slice/DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md
docs/codex_task/README.md
ai_workspace/CONTEXT_INDEX.md
ai_workspace/AI_WORKSPACE_TOPIC_INDEX.md
```

## 3. 依赖与边界检查

```text
12A 材料/支撑/光油语义：可作为 12C 设置映射依据；
12B legacy/OpenVDB 定位：已收口，OpenVDB 只作为 utility/candidate；
12D：只读展示可后接，不阻断 12C-R0/R1；
RGBWSV 协议：保持 p0.rgbwsv.2、uint8、black_is_print、R G B W S V；
Qt 边界：只允许修改 apps/slicer_debug_ui 和明确的 UI 构建配置；
切片算法：12C 不新增、不替换。
```

## 4. 原子任务检查

任务清单已覆盖：

```text
R0：fresh build lane、self-test/smoke 基线、布局与复用边界；
R1：Profile metadata、SliceSettingsModel、generated effective config、帮助元数据；
R2：PreviewWorkspace、图例/探针、DiagnosticsDock、OpenVDB 摘要、最终 smoke/手册/report。
```

每个任务均受以下规则约束：一次只执行一个原子任务，运行指定验证，按项目模板提交，不自动进入下一项。

## 5. 已知 Blocker

Qt 5.15.2 与 MSVC 19.51 的 `stdext::make_checked_array_iterator` 兼容问题已由 `12C-R0-01` 解决。fresh `build-12c-ui` 和 fresh binary self-test 已通过。

R0-02 已使用 `build-12c-ui` fresh binary 完成完整 smoke 基线，未回退到历史 binary。

## 6. 准入判定

```text
12B-R2：COMPLETE
12C 文档准备：COMPLETE
12C 上下文交接：COMPLETE
12C-R0-01：COMPLETE
12C-R0-02：COMPLETE
12C-R0-03：COMPLETE
12C-R0：COMPLETE
12C-R1-01：READY TO START
12C-R2：BLOCKED UNTIL R1 SETTINGS PIPELINE PASSES
12C overall implementation：NOT COMPLETE
```

结论：12C-R0 已完成，下一步只能执行 `12C-R1-01 Profile Metadata 收口`。
