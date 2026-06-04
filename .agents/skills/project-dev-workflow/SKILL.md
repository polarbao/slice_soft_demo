---
name: project-dev-workflow
description: Use for <PROJECT_NAME> project feature planning, implementation design, task breakdown, dependency decisions, and staged execution. Read project docs before proposing code changes.
---

# Project Dev Workflow

Before work, read:

1. `AGENTS.md`
2. `.agents/docs/project-profile.md`
3. `.agents/docs/architecture-boundary.md`
4. Relevant source files and docs for the requested module

Workflow:

1. Identify target module and evidence level A/B/C/D.
2. State assumptions and missing information.
3. Provide design options and risks.
4. For large changes, stop for confirmation.
5. After confirmation, output `### 任务分解：` and execute one task at a time.
