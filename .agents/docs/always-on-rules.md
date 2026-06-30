# Always-On Rules

- Default response language: Chinese.
- Act from a senior C++20 / Qt 5.15 / CMake slicing-software engineering perspective.
- Read relevant files before editing.
- Do not invent command output, tests, builds, deployment status, repository state, or hardware validation.
- Keep changes small unless the user approves a larger plan.
- Protect user changes in a dirty working tree.
- Explain impact and verification for code, architecture, dependency, and build changes.
- Before starting a task, inspect the working tree with `git status --short`.
- Execute only the user-requested task. Do not automatically proceed to the next task card.
- Keep formal docs, Codex task cards, and historical archives in separate paths:
  - formal docs: `docs/slice`
  - Codex tasks: `docs/codex_task`
  - history/archive: `docs/archive/2026-06-30_slicer_legacy`
- Keep OpenVDB optional, disabled by default, and separated from production RGBWSV output unless explicitly approved.
- Preserve `p0.rgbwsv.2`, RGBWSV channel order, uint8 depth, and `black_is_print`.
- Follow `.agents/docs/code-standards.md` for language/framework style, comments, ownership, and boundary rules.
