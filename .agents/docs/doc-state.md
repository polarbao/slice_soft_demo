# Slice Document State Rules

Use A/B/C/D evidence classification.

## Evidence levels

```text
A = current code/config/tests; safe implementation basis
B = formal target design; direction only
C = historical reference; background only
D = deprecated/conflicting; not implementation basis
```

## Rules

- Current code wins over old docs when checking implemented behavior.
- Stage reports are evidence of reported validation, not automatic proof unless commands and results are listed.
- `docs/slice` PRD/DEV/ROADMAP documents describe formal target direction, not implementation status.
- `docs/codex_task/current` describes active AI execution tasks, not product truth by itself.
- `docs/archive/2026-06-30_slicer_legacy` is historical evidence unless a current formal doc explicitly promotes part of it.
- Historical chat logs may explain rationale, but should not be used as direct implementation truth.
- If a source is missing, stale, or expired, state that clearly.

## Source Priority

```text
1. Current source code, CMake, scripts, tests, generated schemas, and checked-in fixtures
2. Latest verified reports that include concrete commands and outcomes
3. Formal docs in docs/slice
4. Active task docs in docs/codex_task/current
5. Archived reports/prompts/tasks in docs/archive and docs/codex_task/archive
6. Chat logs and unverified external conversation notes
```

## Required state split

For architecture/refactor/protocol questions, output:

```text
Current State
Target State
Historical State
Pending Confirmation
```
