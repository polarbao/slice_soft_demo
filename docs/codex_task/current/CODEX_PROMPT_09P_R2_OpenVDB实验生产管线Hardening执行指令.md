# CODEX_PROMPT_09P_R2_OpenVDB实验生产管线Hardening执行指令

> 文档版本：v0.1
> 文档状态：Codex Prompt / Stage 09P-R2
> 生成日期：2026-07-01

---

## 调用模板

```text
请阅读 AGENTS.md、.agents/AGENTS.md、docs/slice/README.md、
docs/slice/PRD/PRD_09P_R2_OpenVDB实验生产管线Hardening.md、
docs/slice/DEV/DEV_09P_R2_ReportSchema_AdmissionGate_CI_UI设计.md、
docs/slice/DEMO/DEMO_09P_R2_OpenVDB实验生产管线Hardening验证方案.md、
docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md。

现在只执行 Task 09P-R2-X：<任务标题>。

要求：
1. 只做这个任务，不执行下一个任务。
2. 开始前运行 git status --short。
3. 修改前读取相关源码和文档。
4. 只修改任务允许范围内的文件。
5. 完成后运行任务指定验证命令。
6. 不修改 p0.rgbwsv.2。
7. 不默认启用 OpenVDB。
8. 不让 OpenVDB 成为强制依赖。
9. 不从 experimental path 写真实 OBJ/3MF production RGBWSV TIFF。
10. 不把 warn_and_attempt 视为 production-safe。
11. 不实现 RIP 半色调、设备通信或喷头 bitstream。
12. 不 push。
```
