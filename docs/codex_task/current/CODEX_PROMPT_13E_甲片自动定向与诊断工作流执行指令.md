# CODEX PROMPT 13E 甲片自动定向与诊断工作流执行指令

按顺序读取：

```text
AGENTS.md
.agents/AGENTS.md
docs/slice/DOC/DOC_DECISION_13E_甲片自动定向与诊断工作流插入专项.md
docs/slice/PRD/PRD_13E_甲片自动定向与诊断工作流修正.md
docs/slice/DEV/DEV_13E_AutoOrient与Qt诊断信息架构设计.md
docs/slice/DEMO/DEMO_13E_自动定向与诊断工作流验证方案.md
docs/codex_task/current/TASKS_13E_甲片自动定向与诊断工作流任务清单.md
```

执行 13E-02..05。

要求：

```text
先写测试再修改候选选择；
浮点比较必须使用明确容差；
产品默认 maxHeightMm=9.0；
fixture 的显式历史值不得批量替换；
warningsDiagnosticView 必须只有一个实例；
底部任务详情默认折叠；
不改变 TIFF/RIP/Legacy/Global；
不得覆盖工作树已有修改；
未运行的验证不得宣称 PASS。
```
