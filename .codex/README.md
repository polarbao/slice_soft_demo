# Slice Soft Demo Codex Context

This directory contains project-scoped Codex metadata and optional agent profiles for `polarbao/slice_soft_demo`.

## Scope

- Runtime preferences live in `config.toml`.
- Human-readable project metadata lives in `project-context.toml`.
- Optional subagent profiles live in `agents/`.
- Hook notes live in `hooks/`.

## Truth Sources

Codex should treat these files as routing and context hints only. Project facts and guardrails come from:

1. `AGENTS.md`
2. `.agents/AGENTS.md`
3. `.agents/docs/SLICE_AI_SKILL_MASTER.md`
4. `.agents/docs/*.md`
5. `docs/slice`
6. `docs/codex_task/current`
7. current source code and tests

Historical docs under `docs/archive/2026-06-30_slicer_legacy` are C-level evidence unless promoted by current formal docs or code.

## Safety

OpenVDB is an optional experimental path. Do not enable it by default, make it mandatory, replace legacy `slicer_cli`, or write production RGBWSV TIFF from the experimental path without an explicit approved task.
