# Slice Chat Save Policy

## Purpose

Chat saves are durable project memory for useful engineering context across AI sessions.

## Default archive location

```text
ai_workspace/<model>/chat_logs/YYYY-MM-DD.md
```

Longer standalone reports go to:

```text
ai_workspace/<model>/analysis_reports/
ai_workspace/integrated_reports/
ai_workspace/context_handoff/
```

## What to save

- Current objective and why it matters.
- Decisions made and rationale.
- Evidence-backed project facts.
- Files read or modified.
- Commands run and observed results.
- Open risks, TODOs, and next recommended prompt.

## What not to save

- Secrets, tokens, credentials, cookies.
- Unsupported validation claims.
- Raw long transcripts unless explicitly requested.
- Personal data unrelated to the project.

## Format

Use Markdown with clear sections:

```markdown
## YYYY-MM-DD
### <topic>
用户请求：
> ...

### 当前状态
### 关键决策
### 涉及文件
### 验证情况
### 未解决问题
### 下一步建议
```
