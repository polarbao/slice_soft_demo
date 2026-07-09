# CODEX_PROMPT_12B_R2 OpenVDB SDF Utility 执行指令

> 文档状态：Codex Prompt / Stage 12B-R2
> 日期：2026-07-08

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/SLICE_AI_SKILL_MASTER.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
.agents/docs/doc-state.md
docs/slice/README.md
docs/slice/DOC/DOC_DECISION_12B_R0_R1_R2_切片引擎性能阶段拆分.md
docs/slice/REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md
docs/slice/REPORT/REPORT_12B_R1_LegacyHeightfield优化当前状态.md
docs/slice/PRD/PRD_12B_R2_OpenVDB_SDFUtility定位.md
docs/slice/DEV/DEV_12B_R2_OpenVDB_SDFUtility评估设计.md
docs/slice/DEMO/DEMO_12B_R2_OpenVDB_SDFUtility验证方案.md
docs/codex_task/current/TASKS_12B_R2_OpenVDB_SDFUtility定位任务清单.md
```

然后只执行用户明确指定的一个 R2 task。

硬性边界：

```text
1. 不默认启用 OpenVDB；
2. 不替代 legacy production slicer；
3. 不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
4. 不从 diagnostic path 写 production RGBWSV TIFF；
5. 不把 warn_and_attempt 视为 production-safe；
6. 不让 slicer_core public API 暴露 Qt 或 OpenVDB implementation types；
7. 不声称未运行的 ON lane 验证通过。
```

每个 task 开始前：

```powershell
git status --short
```

文档任务验证：

```powershell
占位标记扫描：目标为本任务涉及文档
git diff --check
```

默认 OFF 代码任务最低验证：

```powershell
cmake --build build --config Debug --target slicer_cli slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

OpenVDB ON 任务验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
```

如果 ON build 不存在，应输出 blocker，不要伪造通过。

完成后输出：

```text
1. 本次 task 完成情况；
2. 修改文件；
3. 已运行验证命令和结果；
4. 未运行验证及原因；
5. 下一建议 task。
```
