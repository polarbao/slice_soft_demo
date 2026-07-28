# CODEX PROMPT 13B-08-01 批量导入与主切片入口执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
.agents/docs/code-standards.md
docs/slice/DOC/DOC_DECISION_13B_08_场景作业流与13D工作台收口优先级.md
docs/slice/PRD/PRD_13B_08_批量导入与当前场景一键切片.md
docs/slice/DEV/DEV_13B_08_批量导入队列与场景切片编排设计.md
docs/slice/DEMO/DEMO_13B_08_批量导入与场景切片验证方案.md
docs/slice/DOC/DOC_PREP_13B_08_01_批量导入与主切片入口准备.md
```

现在只执行 `13B-08-01`。

要求：

1. 将模型导入文件对话框改为多选，但不维护第二套导入逻辑。
2. 新增串行 `SceneBatchImportController`；不得并发加载 22 个模型。
3. 在启动前校验场景剩余容量；超过 22 时整体阻断，不静默截断。
4. 单项失败记录稳定错误并继续后续文件；成功实例保留。
5. 批次结束只执行一次规则排版，失败时不删除已导入实例。
6. 取消和迟到结果使用 batchId/generation 保护。
7. 增加始终可见的“切片当前场景”主动作；本任务中不得接旧单模型切片路径。
8. 场景生产服务未接通时，按钮保持禁用并显示明确中文原因。
9. 新增三模型、部分失败、容量、取消、单次排版 UI Smoke/单元测试。
10. 不修改生产 TIFF、RIP、OpenVDB 默认、Scene schema 或 13C 预览代码。
11. 当前工作树若存在 13C-03 未提交改动，保留且与本任务分开提交。
12. 遵循 C++ Allman、项目命名、Qt 函数指针 connect 和 Public API Doxygen。
13. 完成后生成 `REPORT_13B_08_01_批量导入与主切片入口当前状态.md`。
14. 运行 PREP 指定验证；未运行不得声称通过。
15. 按中文 `【模块】` 风格原子提交，不提交 `.specstory` 或 `docs/claude`。
