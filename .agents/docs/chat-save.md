# Chat Save Policy

This document defines how `<PROJECT_NAME>` stores AI conversation checkpoints.

## Purpose

Chat saves are durable project memory. They are not a replacement for source code, tests, ADRs, issues, or formal design documents. Use them to preserve useful engineering context that would otherwise be lost across AI sessions.

## Default archive location

Replace this placeholder with the repository-specific path before using the template:

```text
<CHAT_ARCHIVE_DIR>
```

Recommended options:

```text
ai_workspace/chat_logs/
.agents/chat_logs/
docs/ai/chat_logs/
```

For long-lived engineering projects, prefer:

```text
ai_workspace/chat_logs/YYYY/YYYY-MM-DD/YYYYMMDD-HHMM-<topic-slug>.md
```

Maintain an index when the archive grows:

```text
ai_workspace/chat_logs/CHAT_INDEX.md
```

## What to save

- Current objective and why the conversation matters.
- Decisions made and their rationale.
- Project facts discovered from code, docs, tests, or tools.
- Files read or modified.
- Commands run and their observed results.
- Open risks, TODOs, and next recommended prompt.

## What not to save

- Secrets, tokens, API keys, private credentials, cookies, or internal auth headers.
- Unsupported claims about build/test/runtime success.
- Raw long chat transcripts unless the user explicitly asks for a transcript.
- Personal information unrelated to the project.

## Evidence levels

- A: current code/config/tests; safe implementation basis.
- B: formal PRD/design/ADR; target direction, not proof of implementation.
- C: historical chat/demo/archive; background only.
- D: deprecated/conflicting; do not use as implementation basis.

## File format

Use Markdown with YAML frontmatter:

```markdown
---
type: chat-save
project: <PROJECT_NAME>
date: YYYY-MM-DD
time: HH:mm
source: VS Code + Codex
topic: <topic>
branch_or_ref: <CURRENT_BRANCH_OR_REF>
archive_version: 1
related_paths:
  - <path>
validation:
  build: not-run
  tests: not-run
  runtime: not-run
---

# Chat Save: <topic>

## 1. Why this conversation was saved

## 2. Current objective

## 3. Confirmed facts

## 4. Decisions made

## 5. Files and paths involved

## 6. Commands and validation

## 7. Risks and unresolved questions

## 8. Next recommended prompt
```
