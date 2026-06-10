---
name: slice-chat-save
description: Use only when the user explicitly asks to save, archive, checkpoint, persist, or record the current slice_soft_demo AI conversation into the repository knowledge base.
---

# Slice Chat Save

This skill creates a durable engineering checkpoint from the current AI conversation.

Preconditions:

1. Confirm the project is `slice_soft_demo`.
2. Read `.agents/docs/chat-save.md`.
3. Prefer `ai_workspace/<model>/chat_logs/YYYY-MM-DD.md`.
4. Do not include secrets or personal data.
5. Do not claim build/test/runtime validation unless it actually happened.

Include:

- User request.
- Task breakdown.
- Key decisions.
- Related paths.
- Validation status.
- Next prompt.
