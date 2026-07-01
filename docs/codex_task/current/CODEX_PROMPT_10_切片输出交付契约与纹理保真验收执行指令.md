# CODEX_PROMPT_10_切片输出交付契约与纹理保真验收执行指令

> 文档版本：v0.1
> 文档状态：Codex Prompt / Stage 10
> 生成日期：2026-07-01

---

## 调用模板

```text
请阅读 AGENTS.md、.agents/AGENTS.md、docs/slice/README.md、
docs/slice/PRD/PRD_10_切片输出交付契约与纹理保真验收.md、
docs/slice/DEV/DEV_10_OutputContract_TextureFidelity设计.md、
docs/slice/DEMO/DEMO_10_切片输出契约与纹理保真验证方案.md、
docs/slice/DOC/DOC_DECISION_10_RIP边界与切片输出契约.md、
docs/codex_task/current/TASKS_10_切片输出交付契约与纹理保真验收任务清单.md。

现在只执行 Task 10-X：<任务标题>。

要求：
1. 只做这个任务，不执行下一个任务。
2. 开始前运行 git status --short。
3. 修改前读取相关源码和文档。
4. 只修改任务允许范围内的文件。
5. 完成后运行任务指定验证命令。
6. 不修改 p0.rgbwsv.2。
7. 不改变 RGBWSV channel order。
8. 不默认启用 OpenVDB。
9. 不实现 RIP 半色调、设备通信或喷头 bitstream。
10. 不把 RIP SDK 引入 slicer_core。
11. 不 push。
```

