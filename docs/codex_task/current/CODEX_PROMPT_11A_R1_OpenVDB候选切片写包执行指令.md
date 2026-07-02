# CODEX_PROMPT_11A_R1_OpenVDB候选切片写包执行指令

请按以下顺序阅读：

```text
AGENTS.md
.agents/AGENTS.md
docs/slice/DOC/DOC_ANALYSIS_OpenVDB切片功能当前不可用原因.md
docs/slice/REPORT/REPORT_11A_OpenVDB_OBJ彩色纹理切片前置当前状态.md
docs/slice/DOC/DOC_DECISION_11A_R1_OpenVDB候选切片写包与Preview收口.md
docs/slice/PRD/PRD_11A_R1_OpenVDB候选切片写包与Preview收口.md
docs/slice/DEV/DEV_11A_R1_OpenVDBCandidatePipeline_RGBWSVWriter设计.md
docs/slice/DEMO/DEMO_11A_R1_OpenVDB候选包与Preview验证方案.md
docs/codex_task/current/TASKS_11A_R1_OpenVDB候选切片写包任务清单.md
```

执行规则：

```text
每次只执行用户明确指定的任务；
每个任务开始前运行 git status --short；
不修改 p0.rgbwsv.2；
不默认启用 OpenVDB；
不替换 legacy production path；
不绕过 ProductionAdmissionPolicy；
strict_closed 失败不得写 package；
diagnostic_only 不得写 package；
OpenVDB OFF 默认轨道必须继续通过；
完成后运行任务指定验证命令；
输出实际验证结果；
如需提交，提交信息使用中文并遵循仓库提交模板。
```

当前建议从：

```text
Task 11A-R1-1：Pipeline 入口与防误用 guard
```

开始。

