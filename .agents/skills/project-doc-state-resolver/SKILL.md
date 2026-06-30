---
name: project-doc-state-resolver
description: Use for slice_soft_demo questions about whether behavior is implemented, only designed, historical, deprecated, or conflicting across docs, code, and chat logs.
---

# Project Doc State Resolver

Use evidence levels:

- A = current code/config/tests; safe for implementation.
- B = formal target design; direction only.
- C = historical reference; background only.
- D = deprecated/conflicting; not implementation basis.

Always prefer current code over historical docs. Report conflicts explicitly.
