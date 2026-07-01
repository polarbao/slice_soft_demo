# CODEX_PROMPT_00_08_历史阶段文档补齐执行指令

> 文档版本：v0.1
> 文档状态：Codex Prompt / Historical Docs Supplement
> 生成日期：2026-07-01

---

## 调用模板

```text
请阅读 AGENTS.md、docs/slice/README.md、
docs/slice/DOC/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md、
docs/slice/DOC/DOC_CLASSIFICATION_2026-06-30_docs治理归档清单.md、
docs/codex_task/current/TASKS_00_08_历史阶段文档补齐任务清单.md。

现在只执行 Task 00-08-X：<任务标题>。

要求：
1. 只做这个任务，不执行下一个任务。
2. 开始前运行 git status --short。
3. 只把 archive 作为历史证据，不恢复为当前入口。
4. 不修改源码。
5. 完成后运行 git diff --check。
6. 不 push。
```
