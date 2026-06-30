---
name: project-architecture-guardrails
description: Use for slice_soft_demo architecture boundary decisions, module responsibility checks, dependency direction, public API changes, and ADR creation.
---

# Project Architecture Guardrails

Read `.agents/docs/architecture-boundary.md` and relevant module docs.

Rules:

- Do not introduce reverse dependencies across layers.
- Public API changes require impact analysis and tests.
- Separate current implementation facts from target design.
- For irreversible choices, create or update an ADR.
