---
name: slice-architecture-guardrails
description: Use for slice_soft_demo architecture boundary decisions, module responsibility checks, dependency direction, public API changes, and ADR/DOC_DECISION creation.
---

# Slice Architecture Guardrails

Read:

- `.agents/docs/architecture-boundary.md`
- `.agents/docs/SLICE_AI_SKILL_MASTER.md`
- Relevant `docs/slicer/ARCH_*.md` and `DOC_DECISION_*.md`

Rules:

- Do not introduce reverse dependencies across layers.
- Public API changes require impact analysis and tests.
- Separate current implementation facts from target design.
- For irreversible choices, create or update a DOC_DECISION or ADR.
- During R1, follow `wrap first, move later, rewrite last`.
- Do not implement out-of-scope strategies during architecture refactor.
