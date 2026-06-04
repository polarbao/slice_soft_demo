---
name: project-code-review
description: Use for <PROJECT_NAME> code review, diffs, PRs, uncommitted changes, and pre-merge checks against project architecture, code standards, build, and tests.
---

# Project Code Review

Read first:

- `AGENTS.md`
- `.agents/docs/code-standards.md`
- `.agents/docs/architecture-boundary.md`
- Diff or changed files

Review order:

1. Correctness and data safety.
2. Architecture boundary violations.
3. Build/test impact.
4. Error handling/logging.
5. Performance/threading.
6. Style only after functional risks.

Output P0/P1/P2/P3 findings with file paths and suggested fixes.
