---
name: project-chat-save
description: Use only when the user explicitly asks to save, archive, checkpoint, persist, or record the current slice_soft_demo AI conversation into the repository knowledge base. Do not use for ordinary planning, code review, debugging, or context handoff unless the user asks to save the chat.
---

# Project Chat Save

This skill creates a durable engineering checkpoint from the current AI conversation.

It is different from `$project-context-handoff`:

- `project-context-handoff` prepares a compact continuation brief for the next AI session.
- `project-chat-save` writes a project archive record that can be searched later as part of the repository knowledge base.

## Preconditions

Before writing a chat save:

1. Confirm the target project is `slice_soft_demo`.
2. Read `.agents/docs/chat-save.md` if it exists.
3. Prefer the configured `ai_workspace/chat_logs` location.
4. If no archive directory is configured, propose one and ask before creating a new long-term location.
5. Do not include secrets, tokens, credentials, or unrelated personal data.
6. Do not claim build/test/runtime validation unless it actually happened in this session.

## Save location

Default template path:

```text
ai_workspace/chat_logs/YYYY/YYYY-MM-DD/YYYYMMDD-HHMM-<topic-slug>.md
```

Recommended concrete path after template customization:

```text
ai_workspace/chat_logs/YYYY/YYYY-MM-DD/YYYYMMDD-HHMM-<topic-slug>.md
```

If the project keeps an index, update:

```text
ai_workspace/chat_logs/CHAT_INDEX.md
```

## Workflow

1. Identify the save scope:
   - Full current conversation.
   - Only the current task.
   - Only architecture decisions.
   - Only code/debugging findings.
2. Generate a short topic slug using lowercase ASCII words separated by hyphens.
3. Create the archive directory if approved or already established.
4. Write a Markdown file with YAML frontmatter.
5. Include only evidence-backed facts.
6. Mark validation as `not-run`, `passed`, `failed`, or `partial`.
7. End with a next prompt that can resume the work.

## Required output structure

Use this Markdown structure:

```markdown
---
type: chat-save
project: slice_soft_demo
date: YYYY-MM-DD
time: HH:mm
source: VS Code + Codex
topic: <topic>
branch_or_ref: spike/09P-openvdb-experimental-pipeline
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

State why the archive matters.

## 2. Current objective

Summarize the user goal and the immediate task boundary.

## 3. Confirmed facts

Separate facts by evidence level:

- A: current code/config/tests.
- B: formal docs/ADR/PRD.
- C: historical notes/chat/demo/archive.
- D: deprecated/conflicting sources.

## 4. Decisions made

List decisions, rationale, and rejected alternatives if relevant.

## 5. Files and paths involved

List files read, files modified, and files recommended for next inspection.

## 6. Commands and validation

Record commands actually run and observed results. If no command was run, say so.

## 7. Risks and unresolved questions

List technical risks, missing evidence, and user decisions still needed.

## 8. Next recommended prompt

Provide one concise prompt that can restart the work in a new session.
```

## Index update format

If maintaining `CHAT_INDEX.md`, append one row:

```markdown
| Date | Topic | File | Evidence | Status |
|---|---|---|---|---|
| YYYY-MM-DD | <topic> | `<relative-path>` | A/B/C/D | saved |
```

## Quality bar

A good chat save is concise, searchable, evidence-labeled, and safe to commit.

Avoid dumping the raw transcript unless the user explicitly asks for the raw transcript.
