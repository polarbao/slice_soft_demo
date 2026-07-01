# CODEX_PROMPT_11_UI切片层预览交互配置与多模型评估执行指令

> 文档版本：v0.1
> 文档状态：Codex Prompt / Stage 11
> 生成日期：2026-06-30

---

## 调用模板

```text
请阅读 AGENTS.md、.agents/AGENTS.md、docs/slice/README.md、
docs/slice/PRD/PRD_11_UI切片层预览交互配置与多模型能力.md、
docs/slice/DEV/DEV_11_LayerPreview_UIConfig_MultiModel设计.md、
docs/slice/DEMO/DEMO_11_UI切片层预览交互配置验证方案.md、
docs/slice/DOC/DOC_DECISION_11_多模型切片处理范围决策.md、
docs/codex_task/current/TASKS_11_UI切片层预览交互配置与多模型评估任务清单.md。

现在只执行 Task 11-X：<任务标题>。

要求：
1. 只做这个任务，不执行下一个任务。
2. 开始前运行 git status --short。
3. 修改前读取相关源码和文档。
4. 只修改任务允许范围内的文件。
5. 完成后运行任务指定验证命令。
6. 不修改 p0.rgbwsv.2。
7. 不默认启用 OpenVDB。
8. 不实现 RIP 半色调、设备通信或喷头 bitstream。
9. 不默认启用多模型 production 输出。
10. 不 push。
```

