# {{PROJECT_NAME}} Codex Instructions

## Project Identity

- Project: `{{PROJECT_NAME}}`
- Repository: `{{REPOSITORY_NAME}}`
- Main branch/ref: `{{CURRENT_BRANCH_OR_REF}}`
- Main application path: `{{MAIN_APP_PATH}}`
- Deprecated or historical paths: `{{DEPRECATED_PATHS}}`
- Tech stack: `{{TECH_STACK}}`
- Build system: `{{BUILD_SYSTEM}}`
- Main build command: `{{BUILD_COMMAND}}`
- Main test command: `{{TEST_COMMAND}}`
- Formal docs path: `{{FORMAL_DOCS_PATH}}`
- Codex task path: `{{CODEX_TASK_PATH}}`
- Historical archive path: `{{ARCHIVE_DOCS_PATH}}`

## Always-On Hard Rules

- Default response language: `{{DEFAULT_LANGUAGE}}`.
- Current implementation mainline is `{{MAIN_APP_PATH}}`, not `{{DEPRECATED_PATHS}}`.
- Read relevant files before modifying code.
- Do not claim commands, tests, builds, deployments, or external verification ran unless they actually ran.
- Before destructive operations, dependency upgrades, data migrations, production data changes, or broad rewrites, explain the plan and wait for confirmation.
- Prefer small, scoped changes that follow existing project boundaries.
- Before starting a task, inspect the working tree with `git status --short`.
- Execute only the user-requested task; do not continue to the next task unless explicitly asked.
- After a minimal task, run the task-specific validation commands and report any validation not run.
- Do not push unless explicitly instructed.

## Evidence Classification

- A: Current source, build files, tests, runtime config, or verified command output.
- B: Current formal PRD/DEV/ADR/design docs or accepted plans.
- C: Historical docs, demos, archived notes, old task cards, or old discussions.
- D: Deprecated, conflicting, or superseded material.

Use A/B/C/D labels for high-risk work.

## Skill Routing

- Development planning / cross-module design: `project-dev-workflow`
- Code review / diff review: `project-code-review`
- Architecture boundaries / ADRs: `project-architecture-guardrails`
- Build, dependency, test command, and toolchain work: `project-build`
- Context handoff: `project-context-handoff`
- Documentation state conflicts: `project-doc-state-resolver`
- Save/archive current AI conversation: `project-chat-save`

## Reference Docs

- `.agents/docs/README.md`
- `.agents/docs/always-on-rules.md`
- `.agents/docs/workflow-map.md`
- `.agents/docs/project-profile.md`
- `.agents/docs/architecture-boundary.md`
- `.agents/docs/build-and-test.md`
- `.agents/docs/code-standards.md`
- `.agents/docs/doc-state.md`
- `.agents/docs/chat-save.md`

## Current AI Task Entry

- Formal PRD/DEV/DOC documents: `{{FORMAL_DOCS_PATH}}`
- Codex task cards and prompts: `{{CODEX_TASK_PATH}}`
- Historical docs and completed task cards: `{{ARCHIVE_DOCS_PATH}}`
- Current task card: `{{CURRENT_TASK_FILE}}`
