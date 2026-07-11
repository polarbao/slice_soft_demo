# CODEX_PROMPT_12C Qt 工作台收口执行指令

> 文档状态：Codex Prompt / Stage 12C
> 日期：2026-07-10

执行前阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/code-standards.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/REPORT/REPORT_12B_R2_OpenVDB_SDFUtility当前状态.md
docs/slice/DOC/DOC_CHECKLIST_12C_阶段准入与上下文完整性.md
docs/slice/DOC/DOC_AUDIT_12C_现有QtUI能力与收口缺口审查.md
docs/slice/DOC/DOC_DECISION_12C_R0_R1_R2_Qt工作台阶段拆分.md
docs/slice/DOC/DOC_DECISION_12C_UI产品默认值与交互冻结.md
docs/slice/PRD/PRD_12C_Qt_UI配置预览工作台收口.md
docs/slice/DEV/DEV_12C_Qt_UI配置预览工作台设计.md
docs/slice/DEMO/DEMO_12C_Qt_UI配置预览验证方案.md
docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md
ai_workspace/context_handoff/2026-07-10_12B-R2到12C-R0阶段交接.md
```

每次只执行用户明确指定的一个 12C 原子任务。

硬性边界：

```text
1. R0 build lane 未通过前不开始大范围 UI 重构；
2. 不复制一套与 ScenarioRegistry 平行的 Profile 系统；
3. 不重写已有 LayerPreview/Overlay/RawPreview 底层能力；
4. 不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
5. 不新增切片算法；
6. 不默认启用 OpenVDB；
7. 不将 OpenVDB utility/candidate 标为 production-safe；
8. 新增 C++/Qt 代码遵循项目命名、Allman、Doxygen 和函数指针 connect 规则。
```

代码任务最低验证：

```powershell
cmake --build <fresh-ui-build-dir> --config Debug --target slicer_debug_ui
.<fresh-ui-exe> --self-test
```

涉及场景、配置或预览时，再运行任务清单指定的 `--ui-smoke-test` case。不能 fresh build 时必须记录 blocker，不得使用旧 binary 冒充 fresh build 通过。
