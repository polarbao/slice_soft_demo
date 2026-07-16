# CODEX_PROMPT_12F Release 运行环境与切片性能优化执行指令

执行任一 12F 原子任务前读取：

```text
AGENTS.md；
.agents/docs/build-and-test.md；
DOC_DECISION_12F_Release运行环境与切片性能优化专项.md；
PRD_12F_Release运行环境与切片性能优化.md；
DEV_12F_Release运行环境与切片性能优化设计.md；
ROADMAP_12F_Release运行环境与切片性能优化路线.md；
TASKS_12F_Release运行环境与切片性能优化任务清单.md；
当前 12D/12E 状态和 dirty worktree。
```

强制规则：

```text
每次只执行用户明确指定的一个 12F 任务；
性能结论只使用 Release；
before/after 必须同模型、同姿态、同分辨率、同语义；
不修改 RGBWSV 固定协议；
不默认启用 OpenVDB；
不提前实现后续优化；
不覆盖并行进行的 12D 修改；
无实际 benchmark 不宣称提速。
```

Runtime 命令：

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Debug
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release
.\runtime\slicesoft\Debug\slicer_debug_ui.exe --self-test
.\runtime\slicesoft\Release\slicer_debug_ui.exe --self-test
```
