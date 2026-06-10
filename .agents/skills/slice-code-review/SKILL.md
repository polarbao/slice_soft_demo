---
name: slice-code-review
description: Use for slice_soft_demo code review, diffs, PRs, uncommitted changes, and pre-merge checks against RGBWSV protocol, architecture boundaries, build, and regression requirements.
---

# Slice Code Review

Read first:

- `.agents/AGENTS.md`
- `.agents/docs/code-standards.md`
- `.agents/docs/architecture-boundary.md`
- Relevant diff or changed files

Review order:

1. RGBWSV protocol safety.
2. Correctness and data safety.
3. Architecture boundary violations.
4. Build/test impact.
5. Error handling and diagnostics.
6. Performance and threading.
7. Style only after functional risks.

Output P0/P1/P2/P3 findings with file paths and suggested fixes.

Do not approve changes that modify `p0.rgbwsv.2` semantics without a formal decision document.
